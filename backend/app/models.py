from sqlalchemy import Column, Integer, Float, String, Boolean, ForeignKey, UniqueConstraint, Index
from .database import Base
import time

class Device(Base):
    """Bảng 1: Danh sách thiết bị"""
    __tablename__ = "devices"
    device_id = Column(String, primary_key=True, index=True)
    name = Column(String, default="Máy Bơm Không Tên")
    created_at = Column(String, default=lambda: str(time.time()))

class FeatureRecord(Base):
    """Bảng 2: Dữ liệu đo đạc chi tiết (Đã nâng cấp theo Contract v1)"""
    __tablename__ = "feature_records"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("devices.device_id"))
    
    # Mã định danh chống lưu trùng
    boot_id = Column(String, nullable=False)
    sequence = Column(Integer, nullable=False)
    
    # Dấu thời gian
    timestamp = Column(Integer, nullable=True)     # Thời gian mạch đo (Unix)
    received_at = Column(String, nullable=False)   # Thời gian Server nhận được tin
    uptime_ms = Column(Integer, nullable=False)    # Thời gian chạy mạch
    
    # Cấu hình lấy mẫu
    sample_rate_hz = Column(Float, nullable=False)
    sample_count = Column(Integer, nullable=False)
    
    # Đặc trưng rung động
    rms = Column(Float, nullable=False)
    peak_to_peak = Column(Float, nullable=False)
    crest_factor = Column(Float, nullable=False)
    dominant_frequency = Column(Float, nullable=False)
    band_energy = Column(Float, nullable=False)
    anomaly_score = Column(Float, nullable=True)   # Điểm bất thường từ AI (P3)
    
    # Trạng thái & Nhiệt độ
    health_state = Column(String, nullable=False)  # OFF, NORMAL, WARNING, FAULT
    temperature_c = Column(Float, nullable=True)

    # Ràng buộc cốt lõi từ hợp đồng: Chống lưu trùng và Tăng tốc truy vấn
    __table_args__ = (
        UniqueConstraint('device_id', 'boot_id', 'sequence', name='_device_boot_seq_uc'),
        Index('ix_device_timestamp', 'device_id', 'timestamp'),
    )

class HealthEvent(Base):
    """Bảng 3: Cảnh báo và sự kiện"""
    __tablename__ = "health_events"
    id = Column(Integer, primary_key=True, index=True)
    event_id = Column(String, unique=True, index=True, nullable=False)
    device_id = Column(String, ForeignKey("devices.device_id"))
    timestamp = Column(Integer, nullable=True)
    received_at = Column(String, nullable=False)
    type = Column(String, nullable=False)
    message = Column(String)

    acknowledged = Column(Boolean, default=False, nullable=False)
    acknowledged_at = Column(String, nullable=True)

class DeviceStatus(Base):
    """Bảng 4: Trạng thái Online/Offline"""
    __tablename__ = "device_status"
    device_id = Column(String, ForeignKey("devices.device_id"), primary_key=True)
    online = Column(Boolean, default=False)
    last_seen = Column(Integer, nullable=True)
