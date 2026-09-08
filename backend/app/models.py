from sqlalchemy import Column, Integer, String, Float, DateTime, Boolean, ForeignKey
from datetime import datetime
from .database import Base

# Bảng 1: Thông tin thiết bị (devices)
class Device(Base):
    __tablename__ = "devices"
    device_id = Column(String, primary_key=True, index=True)
    name = Column(String, default="ESP32_Vibration_Sensor")
    created_at = Column(DateTime, default=datetime.utcnow)

# Bảng 2: Dữ liệu đặc trưng (feature_records) do ESP32 tính toán gửi lên
class FeatureRecord(Base):
    __tablename__ = "feature_records"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("devices.device_id"))
    timestamp = Column(DateTime, default=datetime.utcnow)
    
    rms = Column(Float)
    peak_to_peak = Column(Float)
    crest_factor = Column(Float)
    dominant_frequency = Column(Float)
    band_energy = Column(Float)

# Bảng 3: Cảnh báo và sự kiện (health_events)

class HealthEvent(Base):
    """Bảng lưu trữ các cảnh báo khi máy bơm có vấn đề (VD: Độ rung quá cao)"""
    __tablename__ = "health_events"
    
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("devices.device_id"))
    event_type = Column(String)       
    description = Column(String)      
    timestamp = Column(String)        

class DeviceStatus(Base):
    """Bảng lưu trạng thái Online/Offline của thiết bị"""
    __tablename__ = "device_status"
    
    device_id = Column(String, ForeignKey("devices.device_id"), primary_key=True)
    is_online = Column(Integer, default=1)  
    last_seen = Column(String)