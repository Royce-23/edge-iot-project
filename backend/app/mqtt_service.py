"""MQTT subscriber for real ESP32 telemetry."""

import asyncio
import logging
import os
import re

import paho.mqtt.client as mqtt
from pydantic import ValidationError

from .database import SessionLocal
from .ingest import event_dict, record_dict, save_feature, save_status
from .schemas import FeaturePayload, StatusPayload

logger = logging.getLogger(__name__)
TOPIC = re.compile(r"^machine/([A-Za-z0-9_-]{1,64})/(features|status)$")


class MqttService:
    def __init__(self, broadcast):
        self.broadcast = broadcast
        self.loop = None
        self.client = None
        self.connected = False
        self.enabled = bool(os.getenv("MQTT_HOST"))

    def start(self):
        host = os.getenv("MQTT_HOST", "").strip()
        if not host:
            logger.warning("MQTT_HOST is unset; device telemetry subscription is disabled")
            return
        if "://" in host or "/" in host:
            raise ValueError("MQTT_HOST must be a hostname, without a URL scheme or path")
        self.loop = asyncio.get_running_loop()
        tls = os.getenv("MQTT_TLS", "false").lower() in ("1", "true", "yes")
        port = int(os.getenv("MQTT_PORT", "8883" if tls else "1883"))
        client_id = os.getenv("MQTT_CLIENT_ID", "edge-iot-backend")
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            clean_session=False,
            manual_ack=True,
        )
        username = os.getenv("MQTT_USER")
        if username:
            self.client.username_pw_set(username, os.getenv("MQTT_PASSWORD", ""))
        if tls:
            self.client.tls_set(ca_certs=os.getenv("MQTT_CA_FILE") or None)
        self.client.reconnect_delay_set(min_delay=1, max_delay=60)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        self.client.connect_async(host, port, keepalive=30)
        self.client.loop_start()
        logger.info("MQTT subscriber starting for %s:%s", host, port)

    def stop(self):
        if self.client:
            self.client.disconnect()
            self.client.loop_stop()
            self.client = None
        self.connected = False

    def _on_connect(self, client, _userdata, _flags, reason_code, _properties):
        self.connected = reason_code == 0
        if self.connected:
            client.subscribe([("machine/+/features", 1), ("machine/+/status", 1)])
            logger.info("MQTT connected and subscribed")
        else:
            logger.error("MQTT connect rejected: %s", reason_code)

    def _on_disconnect(self, _client, _userdata, _flags, reason_code, _properties):
        self.connected = False
        logger.warning("MQTT disconnected: %s", reason_code)

    def _send(self, device_id, message):
        if self.loop and self.loop.is_running():
            asyncio.run_coroutine_threadsafe(self.broadcast(device_id, message), self.loop)

    def _on_message(self, client, _userdata, message):
        match = TOPIC.fullmatch(message.topic)
        if not match or len(message.payload) > 4096:
            logger.warning("Rejected MQTT message: invalid topic or oversize payload")
            client.ack(message.mid, message.qos)
            return
        device_id, kind = match.groups()
        try:
            if kind == "features":
                payload = FeaturePayload.model_validate_json(message.payload)
            else:
                payload = StatusPayload.model_validate_json(message.payload)
            if payload.device_id != device_id:
                raise ValueError("topic and payload device_id differ")
            if kind == "features" and message.retain:
                client.ack(message.mid, message.qos)
                return
            # A retained ONLINE value can be old; only a live heartbeat proves presence.
            if kind == "status" and message.retain and payload.online:
                client.ack(message.mid, message.qos)
                return
        except (ValidationError, ValueError) as exc:
            detail = (", ".join(str(error["loc"][0]) if error["loc"] else error["type"]
                                for error in exc.errors()[:5])
                      if isinstance(exc, ValidationError) else str(exc))
            logger.warning("Rejected MQTT message on %s: %s", message.topic, detail)
            client.ack(message.mid, message.qos)
            return

        try:
            with SessionLocal() as db:
                if kind == "features":
                    record, event, duplicate = save_feature(db, payload)
                    if not duplicate:
                        self._send(device_id, {"type": "features", "device_id": device_id, "data": record_dict(record)})
                        if event:
                            self._send(device_id, {"type": "event", "device_id": device_id, "data": event_dict(event)})
                else:
                    status = save_status(db, payload)
                    self._send(device_id, {"type": "status", "device_id": device_id, "data": {
                        "device_id": device_id, "online": status.online, "last_seen": status.last_seen,
                    }})
        except Exception:
            logger.exception("Failed to persist MQTT message on %s; not acknowledging it", message.topic)
            return
        client.ack(message.mid, message.qos)
