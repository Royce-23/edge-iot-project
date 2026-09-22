from fastapi import (
    FastAPI,
    Depends,
    HTTPException,
    WebSocket,
    WebSocketDisconnect,
    Header
)

from fastapi.middleware.cors import CORSMiddleware

from sqlalchemy.orm import Session

from .database import engine, get_db
from . import models

import time
import os


# =========================================================
# DATABASE
# =========================================================

models.Base.metadata.create_all(bind=engine)


# =========================================================
# FASTAPI
# =========================================================

app = FastAPI(
    title="Edge IoT API - Nhóm 5"
)


# =========================================================
# CORS
# =========================================================

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# =========================================================
# WEBSOCKET MANAGER
# =========================================================

class ConnectionManager:

    def __init__(self):
        self.active_connections = {}

    async def connect(
        self,
        websocket: WebSocket,
        device_id: str
    ):

        await websocket.accept()

        if device_id not in self.active_connections:
            self.active_connections[device_id] = []

        self.active_connections[device_id].append(
            websocket
        )

        print(
            f"[WS] Connected: {device_id} | "
            f"Connections: "
            f"{len(self.active_connections[device_id])}"
        )

    def disconnect(
        self,
        websocket: WebSocket,
        device_id: str
    ):

        if device_id in self.active_connections:

            if websocket in self.active_connections[device_id]:

                self.active_connections[device_id].remove(
                    websocket
                )

            if not self.active_connections[device_id]:

                del self.active_connections[device_id]

        print(
            f"[WS] Disconnected: {device_id}"
        )

    async def broadcast(
        self,
        device_id: str,
        message: dict
    ):

        connections = self.active_connections.get(
            device_id,
            []
        )

        if not connections:
            return

        disconnected = []

        for websocket in connections:

            try:

                await websocket.send_json(
                    message
                )

            except Exception:

                disconnected.append(
                    websocket
                )

        for websocket in disconnected:

            self.disconnect(
                websocket,
                device_id
            )


# =========================================================
# GLOBAL WEBSOCKET MANAGER
# =========================================================

manager = ConnectionManager()


# =========================================================
# STARTUP
# =========================================================

@app.on_event("startup")
def startup_event():

    print(
        "[SYSTEM] Starting Edge-IoT backend..."
    )

    print(
        "[SYSTEM] HTTP/HTTPS ingestion mode"
    )

    print(
        "[SYSTEM] MQTT disabled"
    )

    print(
        "[SYSTEM] Backend startup complete"
    )


# =========================================================
# WEBSOCKET REALTIME
# =========================================================

@app.websocket("/ws/{device_id}")
async def websocket_endpoint(
    websocket: WebSocket,
    device_id: str
):

    await manager.connect(
        websocket,
        device_id
    )

    try:

        while True:

            await websocket.receive_text()

    except WebSocketDisconnect:

        manager.disconnect(
            websocket,
            device_id
        )

    except Exception as e:

        print(
            f"[WS] Error {device_id}: {e}"
        )

        manager.disconnect(
            websocket,
            device_id
        )


# =========================================================
# 1. ESP32 → BACKEND
# NHẬN FEATURE DATA
# =========================================================

@app.post("/api/ingest/features")
async def ingest_features(
    payload: dict,
    x_device_key: str = Header(default="")
):

    # -----------------------------------------------------
    # DEVICE KEY
    # -----------------------------------------------------

    device_key = os.getenv(
        "DEVICE_KEY",
        ""
    )

    if device_key:

        if x_device_key != device_key:

            raise HTTPException(
                status_code=401,
                detail="Invalid device key"
            )


    # -----------------------------------------------------
    # DEVICE ID
    # -----------------------------------------------------

    device_id = payload.get(
        "device_id",
        "motor_01"
    )


    # -----------------------------------------------------
    # REQUIRED DATA
    # -----------------------------------------------------

    boot_id = payload.get(
        "boot_id"
    )

    sequence = payload.get(
        "sequence"
    )

    if not boot_id:

        raise HTTPException(
            status_code=400,
            detail="Missing boot_id"
        )

    if sequence is None:

        raise HTTPException(
            status_code=400,
            detail="Missing sequence"
        )


    # -----------------------------------------------------
    # DATABASE
    # -----------------------------------------------------

    db = next(get_db())

    try:

        # -------------------------------------------------
        # CHỐNG LƯU TRÙNG
        # -------------------------------------------------

        existing = (
            db.query(
                models.FeatureRecord
            )
            .filter(
                models.FeatureRecord.device_id
                == device_id,

                models.FeatureRecord.boot_id
                == boot_id,

                models.FeatureRecord.sequence
                == sequence
            )
            .first()
        )

        if existing:

            print(
                f"[HTTP] Duplicate ignored: "
                f"{device_id} | "
                f"boot={boot_id} | "
                f"seq={sequence}"
            )

            return {
                "success": True,
                "duplicate": True,
                "id": existing.id,
                "device_id": device_id,
                "sequence": sequence
            }


        # -------------------------------------------------
        # TẠO DEVICE NẾU CHƯA CÓ
        # -------------------------------------------------

        device = (
            db.query(
                models.Device
            )
            .filter(
                models.Device.id
                == device_id
            )
            .first()
        )

        if not device:

            device = models.Device(
                id=device_id
            )

            db.add(device)

            db.commit()


        # -------------------------------------------------
        # FEATURE DATA
        # -------------------------------------------------

        now = time.time()

        record = models.FeatureRecord(

            device_id=device_id,

            boot_id=boot_id,

            sequence=sequence,

            timestamp=payload.get(
                "timestamp",
                now
            ),

            received_at=now,

            uptime_ms=payload.get(
                "uptime_ms",
                0
            ),

            sample_rate_hz=payload.get(
                "sample_rate_hz",
                0
            ),

            sample_count=payload.get(
                "sample_count",
                0
            ),

            rms=payload.get(
                "rms",
                0
            ),

            peak_to_peak=payload.get(
                "peak_to_peak",
                0
            ),

            crest_factor=payload.get(
                "crest_factor",
                0
            ),

            dominant_frequency=payload.get(
                "dominant_frequency",
                0
            ),

            band_energy=payload.get(
                "band_energy",
                0
            ),

            anomaly_score=payload.get(
                "anomaly_score",
                0
            ),

            health_state=payload.get(
                "health_state",
                "NORMAL"
            ),

            temperature_c=payload.get(
                "temperature_c",
                0
            )
        )


        db.add(record)


        # -------------------------------------------------
        # HEALTH EVENT
        # -------------------------------------------------

        health_state = payload.get(
            "health_state",
            "NORMAL"
        )

        if health_state in [
            "WARNING",
            "ABNORMAL",
            "FAULT"
        ]:

            event_id = (
                f"{device_id}_"
                f"{boot_id}_"
                f"{sequence}"
            )

            existing_event = (
                db.query(
                    models.HealthEvent
                )
                .filter(
                    models.HealthEvent.event_id
                    == event_id
                )
                .first()
            )

            if not existing_event:

                event = models.HealthEvent(

                    event_id=event_id,

                    device_id=device_id,

                    timestamp=payload.get(
                        "timestamp",
                        now
                    ),

                    received_at=now,

                    type=health_state,

                    message=(
                        f"Machine {device_id}: "
                        f"{health_state}"
                    ),

                    acknowledged=False
                )

                db.add(event)


        # -------------------------------------------------
        # COMMIT
        # -------------------------------------------------

        db.commit()

        db.refresh(record)


        # -------------------------------------------------
        # WEBSOCKET → DASHBOARD
        # -------------------------------------------------

        await manager.broadcast(

            device_id,

            {
                "type": "features",

                "device_id": device_id,

                "data": {

                    "id": record.id,

                    "device_id": record.device_id,

                    "boot_id": record.boot_id,

                    "sequence": record.sequence,

                    "timestamp": record.timestamp,

                    "uptime_ms": record.uptime_ms,

                    "sample_rate_hz":
                        record.sample_rate_hz,

                    "sample_count":
                        record.sample_count,

                    "rms": record.rms,

                    "peak_to_peak":
                        record.peak_to_peak,

                    "crest_factor":
                        record.crest_factor,

                    "dominant_frequency":
                        record.dominant_frequency,

                    "band_energy":
                        record.band_energy,

                    "anomaly_score":
                        record.anomaly_score,

                    "health_state":
                        record.health_state,

                    "temperature_c":
                        record.temperature_c
                }
            }
        )


        print(
            f"[HTTP] Features received: "
            f"{device_id} | "
            f"seq={sequence} | "
            f"state={health_state}"
        )


        return {

            "success": True,

            "duplicate": False,

            "id": record.id,

            "device_id": device_id,

            "sequence": sequence
        }


    except HTTPException:

        db.rollback()

        raise


    except Exception as e:

        db.rollback()

        print(
            f"[HTTP] Features error: {e}"
        )

        raise HTTPException(
            status_code=500,
            detail=str(e)
        )

    finally:

        db.close()


# =========================================================
# 2. ESP32 → BACKEND
# NHẬN STATUS
# =========================================================

@app.post("/api/ingest/status")
async def ingest_status(
    payload: dict,
    x_device_key: str = Header(default="")
):

    # -----------------------------------------------------
    # DEVICE KEY
    # -----------------------------------------------------

    device_key = os.getenv(
        "DEVICE_KEY",
        ""
    )

    if device_key:

        if x_device_key != device_key:

            raise HTTPException(
                status_code=401,
                detail="Invalid device key"
            )


    device_id = payload.get(
        "device_id",
        "motor_01"
    )


    db = next(get_db())

    try:

        # -------------------------------------------------
        # DEVICE
        # -------------------------------------------------

        device = (
            db.query(
                models.Device
            )
            .filter(
                models.Device.id
                == device_id
            )
            .first()
        )

        if not device:

            device = models.Device(
                id=device_id
            )

            db.add(device)


        # -------------------------------------------------
        # STATUS
        # -------------------------------------------------

        status = (
            db.query(
                models.DeviceStatus
            )
            .filter(
                models.DeviceStatus.device_id
                == device_id
            )
            .first()
        )

        now = time.time()

        online = payload.get(
            "online",
            True
        )


        if not status:

            status = models.DeviceStatus(

                device_id=device_id,

                online=online,

                last_seen=now
            )

            db.add(status)

        else:

            status.online = online

            status.last_seen = now


        db.commit()


        # -------------------------------------------------
        # WEBSOCKET → DASHBOARD
        # -------------------------------------------------

        await manager.broadcast(

            device_id,

            {
                "type": "status",

                "device_id": device_id,

                "data": {

                    "online":
                        status.online,

                    "last_seen":
                        status.last_seen
                }
            }
        )


        print(
            f"[HTTP] Status received: "
            f"{device_id} | "
            f"online={online}"
        )


        return {

            "success": True,

            "device_id": device_id,

            "online": status.online
        }


    except Exception as e:

        db.rollback()

        print(
            f"[HTTP] Status error: {e}"
        )

        raise HTTPException(
            status_code=500,
            detail=str(e)
        )

    finally:

        db.close()


# =========================================================
# 3. API LẤY DANH SÁCH THIẾT BỊ
# =========================================================

@app.get("/api/devices")
def get_devices(
    db: Session = Depends(get_db)
):

    statuses = (
        db.query(
            models.DeviceStatus
        )
        .all()
    )

    return [

        {
            "device_id": s.device_id,

            "online": s.online,

            "last_seen": s.last_seen
        }

        for s in statuses

    ]


# =========================================================
# 4. API LẤY DỮ LIỆU MỚI NHẤT
# =========================================================

@app.get("/api/devices/{device_id}/latest")
def get_latest(
    device_id: str,
    db: Session = Depends(get_db)
):

    record = (
        db.query(
            models.FeatureRecord
        )
        .filter(
            models.FeatureRecord.device_id
            == device_id
        )
        .order_by(
            models.FeatureRecord.id.desc()
        )
        .first()
    )

    if not record:

        raise HTTPException(
            status_code=404,
            detail=(
                "Chưa có dữ liệu đo đạc "
                "cho máy này"
            )
        )

    return record


# =========================================================
# 5. API LẤY LỊCH SỬ
# =========================================================

@app.get("/api/devices/{device_id}/history")
def get_history(
    device_id: str,
    limit: int = 100,
    db: Session = Depends(get_db)
):

    if limit < 1 or limit > 1000:

        limit = 100

    records = (
        db.query(
            models.FeatureRecord
        )
        .filter(
            models.FeatureRecord.device_id
            == device_id
        )
        .order_by(
            models.FeatureRecord.id.desc()
        )
        .limit(limit)
        .all()
    )

    return records[::-1]


# =========================================================
# 6. API LẤY LỊCH SỬ CẢNH BÁO
# =========================================================

@app.get("/api/devices/{device_id}/events")
def get_events(
    device_id: str,
    limit: int = 100,
    db: Session = Depends(get_db)
):

    if limit < 1 or limit > 1000:

        limit = 100

    events = (
        db.query(
            models.HealthEvent
        )
        .filter(
            models.HealthEvent.device_id
            == device_id
        )
        .order_by(
            models.HealthEvent.id.desc()
        )
        .limit(limit)
        .all()
    )

    return events


# =========================================================
# 7. API TRẠNG THÁI THIẾT BỊ
# =========================================================

@app.get("/api/devices/{device_id}/status")
def get_status(
    device_id: str,
    db: Session = Depends(get_db)
):

    status = (
        db.query(
            models.DeviceStatus
        )
        .filter(
            models.DeviceStatus.device_id
            == device_id
        )
        .first()
    )

    if not status:

        raise HTTPException(
            status_code=404,
            detail=(
                "Không tìm thấy "
                "trạng thái thiết bị"
            )
        )


    is_stale = False


    latest_record = (
        db.query(
            models.FeatureRecord
        )
        .filter(
            models.FeatureRecord.device_id
            == device_id
        )
        .order_by(
            models.FeatureRecord.id.desc()
        )
        .first()
    )


    if latest_record:

        try:

            time_since_last_record = (
                time.time()
                - float(
                    latest_record.received_at
                )
            )

            if time_since_last_record > 300:

                is_stale = True

        except Exception:

            is_stale = True

    else:

        is_stale = True


    return {

        "device_id":
            status.device_id,

        "online":
            status.online,

        "last_seen":
            status.last_seen,

        "stale":
            is_stale
    }


# =========================================================
# 8. API ACKNOWLEDGE EVENT
# =========================================================

@app.post("/api/events/{event_id}/ack")
def acknowledge_event(
    event_id: str,
    db: Session = Depends(get_db)
):

    event = (
        db.query(
            models.HealthEvent
        )
        .filter(
            models.HealthEvent.event_id
            == event_id
        )
        .first()
    )

    if not event:

        raise HTTPException(
            status_code=404,
            detail="Không tìm thấy event"
        )


    if not event.acknowledged:

        event.acknowledged = True

        event.acknowledged_at = str(
            time.time()
        )

        db.commit()

        db.refresh(event)


    return {

        "success": True,

        "event_id":
            event.event_id,

        "acknowledged":
            event.acknowledged,

        "acknowledged_at":
            event.acknowledged_at
    }


# =========================================================
# 9. API TÌM EVENT THEO ID
# =========================================================

@app.get("/api/events/{event_id}")
def get_event(
    event_id: str,
    db: Session = Depends(get_db)
):

    event = (
        db.query(
            models.HealthEvent
        )
        .filter(
            models.HealthEvent.event_id
            == event_id
        )
        .first()
    )

    if not event:

        raise HTTPException(
            status_code=404,
            detail="Không tìm thấy event"
        )

    return event