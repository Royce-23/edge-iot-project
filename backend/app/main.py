from fastapi import FastAPI, Depends
from sqlalchemy.orm import Session
from . import models
from .database import get_db
from .mqtt_service import start_mqtt

# Khởi tạo ứng dụng FastAPI
app = FastAPI(
    title="Edge-IoT Predictive Maintenance API",
    description="REST API cho hệ thống giám sát tình trạng máy quay",
    version="1.0.0"
)

# Sự kiện này báo cho server biết: Khi nào Server bật lên, hãy bật luôn trạm MQTT
@app.on_event("startup")
def startup_event():
    start_mqtt()

@app.get("/")
def read_root():
    return {"message": "Server Backend đang hoạt động tốt!"}

@app.get("/api/devices")
def get_devices(db: Session = Depends(get_db)):
    """Lấy danh sách tất cả thiết bị và trạng thái online/offline"""
    devices = db.query(models.Device).all()
    return {"status": "success", "data": devices}

@app.get("/api/devices/{device_id}/latest")
def get_latest_data(device_id: str, db: Session = Depends(get_db)):
    """Lấy đặc trưng (features) và trạng thái sức khỏe mới nhất của thiết bị"""
    latest_record = db.query(models.FeatureRecord)\
                      .filter(models.FeatureRecord.device_id == device_id)\
                      .order_by(models.FeatureRecord.timestamp.desc())\
                      .first()
                      
    if not latest_record:
        return {"device_id": device_id, "message": "Chưa có dữ liệu"}
        
    return {
        "device_id": device_id,
        "timestamp": latest_record.timestamp, 
        "features": {
            "rms": latest_record.rms,
            "peak_to_peak": latest_record.peak_to_peak,
            "crest_factor": latest_record.crest_factor
        }
    }

@app.get("/api/devices/{device_id}/history")
def get_device_history(device_id: str, limit: int = 50, db: Session = Depends(get_db)):
    """API 1: Lấy lịch sử đo đạc gần nhất của thiết bị"""
    all_records_count = db.query(models.FeatureRecord).count()
    print(f"🔍 [DEBUG] Tổng số bản ghi đang có trong bảng feature_records là: {all_records_count}")

    records = db.query(models.FeatureRecord)\
                .filter(models.FeatureRecord.device_id == device_id)\
                .order_by(models.FeatureRecord.id.desc())\
                .limit(limit)\
                .all()
                
    print(f"🔍 [DEBUG] Số bản khớp với device_id '{device_id}' là: {len(records)}")
    
    return {"device_id": device_id, "history": records}

@app.get("/api/devices/{device_id}/events")
def get_health_events(device_id: str, limit: int = 20, db: Session = Depends(get_db)):
    """API 2: Lấy danh sách cảnh báo (alerts) của thiết bị"""
    events = db.query(models.HealthEvent)\
               .filter(models.HealthEvent.device_id == device_id)\
               .order_by(models.HealthEvent.id.desc())\
               .limit(limit)\
               .all()
    return {"device_id": device_id, "events": events}

@app.get("/api/devices/{device_id}/status")
def get_device_status(device_id: str, db: Session = Depends(get_db)):
    """API 3: Kiểm tra xem máy bơm đang online hay offline"""
    status = db.query(models.DeviceStatus)\
               .filter(models.DeviceStatus.device_id == device_id)\
               .first()
               
    if not status:
        return {"device_id": device_id, "message": "Chưa có dữ liệu trạng thái"}
        
    return {
        "device_id": device_id, 
        "is_online": status.is_online,
        "last_seen": status.last_seen
    }