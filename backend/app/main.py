"""REST and WebSocket API for measured device data."""

import os
import secrets
import time
from contextlib import asynccontextmanager

from fastapi import Depends, FastAPI, Header, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session

from . import models
from .database import engine, get_db
from .ingest import event_dict, record_dict, save_feature, save_status
from .mqtt_service import MqttService
from .schemas import FeaturePayload, StatusPayload

HEARTBEAT_TIMEOUT_SECONDS = int(os.getenv("HEARTBEAT_TIMEOUT_SECONDS", "15"))
DATA_STALE_SECONDS = int(os.getenv("DATA_STALE_SECONDS", "10"))


class ConnectionManager:
    def __init__(self):
        self.connections: dict[str, set[WebSocket]] = {}

    async def connect(self, ws: WebSocket, device_id: str):
        await ws.accept()
        self.connections.setdefault(device_id, set()).add(ws)

    def disconnect(self, ws: WebSocket, device_id: str):
        self.connections.get(device_id, set()).discard(ws)

    async def broadcast(self, device_id: str, message: dict):
        for ws in tuple(self.connections.get(device_id, ())):
            try:
                await ws.send_json(message)
            except Exception:
                self.disconnect(ws, device_id)


manager = ConnectionManager()
mqtt_service = MqttService(manager.broadcast)


@asynccontextmanager
async def lifespan(_app: FastAPI):
    models.Base.metadata.create_all(bind=engine)
    mqtt_service.start()
    try:
        yield
    finally:
        mqtt_service.stop()


app = FastAPI(title="Edge IoT API", lifespan=lifespan)
origins = [item.strip() for item in os.getenv(
    "DASHBOARD_ORIGINS", "https://edge-iot-project-2.onrender.com"
).split(",") if item.strip()]
app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=False,
    allow_methods=["GET", "POST"],
    allow_headers=["Content-Type", "X-Device-Key"],
)


def require_device_key(x_device_key: str | None = Header(default=None)):
    configured = os.getenv("DEVICE_API_KEY", "")
    if not configured:
        raise HTTPException(status_code=503, detail="HTTP ingest chưa được cấu hình")
    if not x_device_key or not secrets.compare_digest(x_device_key, configured):
        raise HTTPException(status_code=401, detail="Device API key không hợp lệ")


def effective_online(status: models.DeviceStatus | None) -> bool:
    return bool(status and status.online and status.last_seen is not None and
                time.time() - status.last_seen < HEARTBEAT_TIMEOUT_SECONDS)


def status_dict(db: Session, device_id: str) -> dict:
    status = db.get(models.DeviceStatus, device_id)
    if status is None:
        raise HTTPException(status_code=404, detail="Chưa có heartbeat từ thiết bị")
    latest = db.query(models.FeatureRecord).filter_by(device_id=device_id).order_by(
        models.FeatureRecord.id.desc()).first()
    if latest is None:
        stale = True
    else:
        try:
            measured_at = latest.timestamp if latest.timestamp is not None else latest.received_at
            stale = time.time() - float(measured_at) >= DATA_STALE_SECONDS
        except (TypeError, ValueError):
            stale = True
    return {
        "device_id": device_id,
        "online": effective_online(status),
        "last_seen": status.last_seen,
        "stale": stale,
    }


@app.get("/")
def root():
    return {"project": "Edge IoT Predictive Maintenance", "status": "running"}


@app.get("/api/health")
def health():
    return {"backend": "ok", "mqtt_enabled": mqtt_service.enabled,
            "mqtt_connected": mqtt_service.connected}


@app.websocket("/ws/{device_id}")
async def websocket_endpoint(ws: WebSocket, device_id: str):
    await manager.connect(ws, device_id)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        manager.disconnect(ws, device_id)


@app.post("/api/ingest/features", dependencies=[Depends(require_device_key)])
async def ingest_features(payload: FeaturePayload, db: Session = Depends(get_db)):
    record, event, duplicate = save_feature(db, payload)
    if not duplicate:
        await manager.broadcast(payload.device_id, {
            "type": "features", "device_id": payload.device_id, "data": record_dict(record),
        })
        if event:
            await manager.broadcast(payload.device_id, {
                "type": "event", "device_id": payload.device_id, "data": event_dict(event),
            })
    return {"success": True, "duplicate": duplicate, "id": record.id,
            "device_id": payload.device_id, "boot_id": payload.boot_id,
            "sequence": payload.sequence}


@app.post("/api/ingest/status", dependencies=[Depends(require_device_key)])
async def ingest_status(payload: StatusPayload, db: Session = Depends(get_db)):
    status = save_status(db, payload)
    result = {"device_id": status.device_id, "online": status.online,
              "last_seen": status.last_seen}
    await manager.broadcast(payload.device_id, {
        "type": "status", "device_id": payload.device_id, "data": result,
    })
    return {"success": True, **result}


@app.get("/api/devices")
def get_devices(db: Session = Depends(get_db)):
    return [{"device_id": status.device_id, "online": effective_online(status),
             "last_seen": status.last_seen} for status in db.query(models.DeviceStatus).all()]


@app.get("/api/devices/{device_id}/latest")
def get_latest(device_id: str, db: Session = Depends(get_db)):
    record = db.query(models.FeatureRecord).filter_by(device_id=device_id).order_by(
        models.FeatureRecord.id.desc()).first()
    if record is None:
        raise HTTPException(status_code=404, detail="Chưa có dữ liệu đo")
    return record_dict(record)


@app.get("/api/devices/{device_id}/history")
def get_history(device_id: str, limit: int = 100, db: Session = Depends(get_db)):
    if not 1 <= limit <= 1000:
        raise HTTPException(status_code=422, detail="limit phải nằm trong khoảng 1..1000")
    records = db.query(models.FeatureRecord).filter_by(device_id=device_id).order_by(
        models.FeatureRecord.id.desc()).limit(limit).all()
    return [record_dict(record) for record in reversed(records)]


@app.get("/api/devices/{device_id}/events")
def get_events(device_id: str, limit: int = 100, db: Session = Depends(get_db)):
    if not 1 <= limit <= 1000:
        raise HTTPException(status_code=422, detail="limit phải nằm trong khoảng 1..1000")
    events = db.query(models.HealthEvent).filter_by(device_id=device_id).order_by(
        models.HealthEvent.id.desc()).limit(limit).all()
    return [event_dict(event) for event in events]


@app.get("/api/devices/{device_id}/status")
def get_status(device_id: str, db: Session = Depends(get_db)):
    return status_dict(db, device_id)


@app.post("/api/events/{event_id}/ack")
def acknowledge_event(event_id: str, db: Session = Depends(get_db)):
    event = db.query(models.HealthEvent).filter_by(event_id=event_id).first()
    if event is None:
        raise HTTPException(status_code=404, detail="Không tìm thấy event")
    if not event.acknowledged:
        event.acknowledged = True
        event.acknowledged_at = str(time.time())
        db.commit()
    return {"success": True, "event_id": event_id,
            "acknowledged": event.acknowledged,
            "acknowledged_at": event.acknowledged_at}


@app.get("/api/events/{event_id}")
def get_event(event_id: str, db: Session = Depends(get_db)):
    event = db.query(models.HealthEvent).filter_by(event_id=event_id).first()
    if event is None:
        raise HTTPException(status_code=404, detail="Không tìm thấy event")
    return event_dict(event)
