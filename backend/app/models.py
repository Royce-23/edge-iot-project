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
    __tablename__ = "health_events"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("devices.device_id"))
    timestamp = Column(DateTime, default=datetime.utcnow)
    
    event_type = Column(String)  # Ví dụ: NORMAL, WARNING, FAULT
    description = Column(String) # Mô tả chi tiết

# Bảng 4: Trạng thái thiết bị (device_status) - Lần cuối hoạt động
class DeviceStatus(Base):
    __tablename__ = "device_status"
    device_id = Column(String, primary_key=True, index=True)
    last_seen = Column(DateTime, default=datetime.utcnow)
    is_online = Column(Boolean, default=False)