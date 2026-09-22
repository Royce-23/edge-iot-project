"""MQTT-to-database-to-API checks without an external broker."""

import json
import os
import tempfile
import unittest
from types import SimpleNamespace

from fastapi import HTTPException
from pydantic import ValidationError

_db_dir = tempfile.TemporaryDirectory()
os.environ["DATABASE_URL"] = f"sqlite:///{_db_dir.name}/test.db"
os.environ["DEVICE_API_KEY"] = "integration-test-key"
os.environ.pop("MQTT_HOST", None)

from app import models  # noqa: E402
from app.database import SessionLocal, engine  # noqa: E402
from app.main import get_events, get_history, get_latest, get_status, require_device_key, mqtt_service  # noqa: E402
from app.schemas import FeaturePayload  # noqa: E402


class FakeMqttClient:
    def __init__(self):
        self.acks = []

    def ack(self, mid, qos):
        self.acks.append((mid, qos))


class LivePipelineTest(unittest.TestCase):
    def setUp(self):
        models.Base.metadata.drop_all(engine)
        models.Base.metadata.create_all(engine)
        self.mqtt = FakeMqttClient()

    def publish(self, topic, payload, mid=1, retain=False):
        mqtt_service._on_message(self.mqtt, None, SimpleNamespace(
            topic=topic, payload=json.dumps(payload).encode(), mid=mid, qos=1,
            retain=retain,
        ))

    def test_real_payload_is_stored_once_and_available_to_api(self):
        payload = {
            "schema_version": 1, "device_id": "motor_01", "boot_id": "boot1",
            "sequence": 7, "timestamp": None, "uptime_ms": 1000,
            "sample_rate_hz": 800, "sample_count": 512, "rms": 0.3,
            "peak_to_peak": 0.6, "crest_factor": 2,
            "dominant_frequency": 220, "band_energy": 0.03,
            "anomaly_score": None, "health_state": "WARNING",
            "temperature_c": None,
        }
        with SessionLocal() as db:
            with self.assertRaises(HTTPException) as missing:
                get_latest("motor_01", db)
            self.assertEqual(missing.exception.status_code, 404)

        self.publish("machine/motor_01/features", payload)
        self.publish("machine/motor_01/features", payload, mid=2)
        with SessionLocal() as db:
            self.assertEqual(get_latest("motor_01", db)["rms"], 0.3)
            self.assertEqual(len(get_history("motor_01", db=db)), 1)
            self.assertEqual(len(get_events("motor_01", db=db)), 1)
        self.assertEqual(self.mqtt.acks, [(1, 1), (2, 1)])

        status = {"device_id": "motor_01", "online": True, "timestamp": None}
        self.publish("machine/motor_01/status", status, mid=3)
        with SessionLocal() as db:
            self.assertTrue(get_status("motor_01", db)["online"])
            record = db.query(models.FeatureRecord).first()
            record.timestamp = 1  # A delayed replay must be shown as stale.
            db.commit()
            self.assertTrue(get_status("motor_01", db)["stale"])
        self.publish("machine/motor_01/status", {**status, "online": False}, mid=4)
        self.publish("machine/motor_01/status", status, mid=5, retain=True)
        with SessionLocal() as db:
            self.assertFalse(get_status("motor_01", db)["online"])

    def test_invalid_payload_and_http_key(self):
        bad = {"device_id": "motor_01", "rms": 100}
        self.publish("machine/motor_01/features", bad)
        mqtt_service._on_message(self.mqtt, None, SimpleNamespace(
            topic="machine/motor_01/features", payload=b"{", mid=2, qos=1,
            retain=False,
        ))
        with SessionLocal() as db:
            self.assertEqual(db.query(models.FeatureRecord).count(), 0)
        self.assertEqual(self.mqtt.acks, [(1, 1), (2, 1)])
        with self.assertRaises(ValidationError):
            FeaturePayload.model_validate(bad)
        with self.assertRaises(HTTPException) as unauthorized:
            require_device_key(None)
        self.assertEqual(unauthorized.exception.status_code, 401)
        require_device_key("integration-test-key")


if __name__ == "__main__":
    unittest.main()
