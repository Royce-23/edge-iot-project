<<<<<<< HEAD
from fastapi import FastAPI, Depends, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session

from .database import engine, get_db
from . import models, mqtt_service

import time


# =========================================================
# DATABASE
# =========================================================

# Tự động tạo bảng nếu chưa có
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
=======
from fastapi import FastAPI, Depends, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session
from .database import engine, get_db
from . import models, mqtt_service
import time

# Tự động tạo bảng nếu chưa có
models.Base.metadata.create_all(bind=engine)

app = FastAPI(title="Edge IoT API - Nhóm 5")

# BẬT CORS: Cho phép Dashboard của P5 gọi API mà không bị chặn lỗi Origin
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"], # Trong thực tế có thể giới hạn URL web của P5
>>>>>>> 0f1fedffb9f7fb4f315d1a143226d9b240b6bf77
    allow_methods=["*"],
    allow_headers=["*"],
)

<<<<<<< HEAD

# =========================================================
# WEBSOCKET MANAGER
# =========================================================

class ConnectionManager:

    def __init__(self):
        # Mỗi device có danh sách WebSocket riêng
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

    print("[SYSTEM] Starting Edge-IoT backend...")

    # Cho MQTT service biết WebSocket manager
    mqtt_service.set_websocket_manager(
        manager
    )

    # Khởi động MQTT
    mqtt_service.start_mqtt()

    print("[SYSTEM] Backend startup complete")


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

            # Giữ WebSocket mở.
            # Dashboard có thể gửi text/ping nếu cần.
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
# 1. API LẤY DANH SÁCH THIẾT BỊ
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
# 2. API LẤY DỮ LIỆU MỚI NHẤT
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
            models.FeatureRecord.device_id ==
            device_id
        )
        .order_by(
            models.FeatureRecord.id.desc()
        )
        .first()
    )

    if not record:

        raise HTTPException(
            status_code=404,
            detail="Chưa có dữ liệu đo đạc cho máy này"
        )

    return record


# =========================================================
# 3. API LẤY LỊCH SỬ
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
            models.FeatureRecord.device_id ==
            device_id
        )
        .order_by(
            models.FeatureRecord.id.desc()
        )
        .limit(limit)
        .all()
    )

    # Trả về từ cũ -> mới
    return records[::-1]


# =========================================================
# 4. API LẤY LỊCH SỬ CẢNH BÁO
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
            models.HealthEvent.device_id ==
            device_id
        )
        .order_by(
            models.HealthEvent.id.desc()
        )
        .limit(limit)
        .all()
    )

    return events


# =========================================================
# 5. API TRẠNG THÁI MẠNG
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
            models.DeviceStatus.device_id ==
            device_id
        )
        .first()
    )

    if not status:

        raise HTTPException(
            status_code=404,
            detail="Không tìm thấy trạng thái thiết bị"
        )

    is_stale = False

    latest_record = (
        db.query(
            models.FeatureRecord
        )
        .filter(
            models.FeatureRecord.device_id ==
            device_id
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

=======
# Khởi động MQTT chạy ngầm khi API bật lên
@app.on_event("startup")
def startup_event():
    mqtt_service.start_mqtt()

# ---------------------------------------------------------
# 1. API Lấy danh sách toàn bộ thiết bị
# ---------------------------------------------------------
@app.get("/api/devices")
def get_devices(db: Session = Depends(get_db)):
    statuses = db.query(models.DeviceStatus).all()
    # Chỉ bốc đúng 3 biến theo hợp đồng yêu cầu
    return [{"device_id": s.device_id, "online": s.online, "last_seen": s.last_seen} for s in statuses]

# ---------------------------------------------------------
# 2. API Lấy dữ liệu ĐO ĐẠC MỚI NHẤT của 1 máy
# ---------------------------------------------------------
@app.get("/api/devices/{device_id}/latest")
def get_latest(device_id: str, db: Session = Depends(get_db)):
    record = db.query(models.FeatureRecord)\
               .filter(models.FeatureRecord.device_id == device_id)\
               .order_by(models.FeatureRecord.id.desc())\
               .first()
               
    if not record:
        raise HTTPException(status_code=404, detail="Chưa có dữ liệu đo đạc cho máy này")
    return record

# ---------------------------------------------------------
# 3. API Lấy LỊCH SỬ ĐO ĐẠC (Để vẽ biểu đồ)
# ---------------------------------------------------------
@app.get("/api/devices/{device_id}/history")
def get_history(device_id: str, limit: int = 100, db: Session = Depends(get_db)):
    # Hợp đồng: limit giới hạn từ 1 đến 1000
    if limit < 1 or limit > 1000:
        limit = 100
        
    # Bước A: Bốc N bản ghi mới nhất từ database
    records = db.query(models.FeatureRecord)\
                .filter(models.FeatureRecord.device_id == device_id)\
                .order_by(models.FeatureRecord.id.desc())\
                .limit(limit)\
                .all()
                
    # Bước B: Đảo ngược mảng ( [::-1] ) để dữ liệu trả ra xếp TĂNG DẦN thời gian
    # Hợp đồng yêu cầu biểu đồ phải vẽ từ cũ tới mới
    return records[::-1]

# ---------------------------------------------------------
# 4. API Lấy LỊCH SỬ CẢNH BÁO (Lỗi, Cảnh báo AI)
# ---------------------------------------------------------
@app.get("/api/devices/{device_id}/events")
def get_events(device_id: str, limit: int = 100, db: Session = Depends(get_db)):
    events = db.query(models.HealthEvent)\
               .filter(models.HealthEvent.device_id == device_id)\
               .order_by(models.HealthEvent.id.desc())\
               .limit(limit)\
               .all()
    return events

# ---------------------------------------------------------
# 5. API Lấy TRẠNG THÁI MẠNG chi tiết
# ---------------------------------------------------------
@app.get("/api/devices/{device_id}/status")
def get_status(device_id: str, db: Session = Depends(get_db)):
    status = db.query(models.DeviceStatus).filter(models.DeviceStatus.device_id == device_id).first()
    if not status:
        raise HTTPException(status_code=404, detail="Không tìm thấy trạng thái thiết bị")
    
    # Tính toán biến "stale" (Dữ liệu cũ): 
    # Mạch có thể có mạng (online), nhưng nếu 5 phút (300s) rồi không đo được thông số nào -> stale = True
    is_stale = False
    latest_record = db.query(models.FeatureRecord).filter(models.FeatureRecord.device_id == device_id).order_by(models.FeatureRecord.id.desc()).first()
    
    if latest_record:
        try:
            time_since_last_record = time.time() - float(latest_record.received_at)
            if time_since_last_record > 300: 
                is_stale = True
        except:
            is_stale = True
    else:
>>>>>>> 0f1fedffb9f7fb4f315d1a143226d9b240b6bf77
        is_stale = True

    return {
        "device_id": status.device_id,
        "online": status.online,
        "last_seen": status.last_seen,
        "stale": is_stale
<<<<<<< HEAD
    }


# =========================================================
# 6. API ACKNOWLEDGE EVENT
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
            models.HealthEvent.event_id ==
            event_id
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
        "event_id": event.event_id,
        "acknowledged": event.acknowledged,
        "acknowledged_at": event.acknowledged_at
    }


# =========================================================
# 7. API TÌM EVENT THEO ID
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
            models.HealthEvent.event_id ==
            event_id
        )
        .first()
    )

    if not event:

        raise HTTPException(
            status_code=404,
            detail="Không tìm thấy event"
        )

    return event
=======
    }
>>>>>>> 0f1fedffb9f7fb4f315d1a143226d9b240b6bf77
