import json
import paho.mqtt.client as mqtt
from sqlalchemy.orm import Session
from .database import SessionLocal, engine
from . import models
from datetime import datetime

# 1. Lệnh này sẽ nhìn vào file models.py và TỰ ĐỘNG tạo file iot_data.db trên ổ cứng
models.Base.metadata.create_all(bind=engine)

# 2. Cấu hình MQTT Broker 
MQTT_BROKER = "test.mosquitto.org" 
MQTT_PORT = 1883
MQTT_TOPIC = "machine/#" 

# 3. Hàm xử lý lưu dữ liệu vào Ổ cứng (SQLite) - ĐÃ FIX LỖI THỜI GIAN
def save_feature_data(device_id: str, payload: dict):
    db = SessionLocal()
    try:
        rms_value = payload.get("rms", 0.0)
        raw_ts = payload.get("timestamp", 0)

        # Chuyển đổi mã thời gian Unix (số nguyên) sang chuẩn DateTime (Ngày-Giờ)
        try:
            dt_timestamp = datetime.fromtimestamp(int(raw_ts))
        except:
            dt_timestamp = datetime.now() # Đề phòng lỗi thì lấy giờ hiện tại
            
        str_timestamp = str(dt_timestamp) # Dành cho bảng cảnh báo

        # ==========================================
        # 1. BỘ LỌC CHỐNG LƯU TRÙNG (Deduplication)
        # ==========================================
        existing_record = db.query(models.FeatureRecord).filter(
            models.FeatureRecord.device_id == device_id,
            models.FeatureRecord.timestamp == dt_timestamp
        ).first()
        
        if existing_record:
            print(f"⚠️ Đã chặn 1 gói tin trùng từ {device_id} (thời điểm {str_timestamp})")
            return

        # ==========================================
        # Kiểm tra và thêm thiết bị mới
        # ==========================================
        device = db.query(models.Device).filter(models.Device.device_id == device_id).first()
        if not device:
            new_device = models.Device(device_id=device_id)
            db.add(new_device)
            db.commit()

        # ==========================================
        # 2. LƯU DỮ LIỆU ĐẶC TRƯNG BÌNH THƯỜNG
        # ==========================================
        new_record = models.FeatureRecord(
            device_id=device_id,
            rms=rms_value,
            peak_to_peak=payload.get("peak_to_peak", 0.0),
            crest_factor=payload.get("crest_factor", 0.0),
            timestamp=dt_timestamp
        )
        db.add(new_record)

        # ==========================================
        # 3. KÍCH HOẠT CỜ BÁO LỖI (Health Events)
        # ==========================================
        if rms_value > 5.0:
            event_level = "CRITICAL" if rms_value > 7.0 else "WARNING"
            new_event = models.HealthEvent(
                device_id=device_id,
                event_type=event_level,
                description=f"Độ rung bất thường! RMS = {rms_value}",
                timestamp=str_timestamp
            )
            db.add(new_event)
            print(f"🚨 BÁO ĐỘNG {event_level}: Máy {device_id} đang rung quá mạnh!")

        # ==========================================
        # 4. CẬP NHẬT TRẠNG THÁI (Device Status)
        # ==========================================
        status = db.query(models.DeviceStatus).filter(models.DeviceStatus.device_id == device_id).first()
        if not status:
            status = models.DeviceStatus(device_id=device_id, is_online=1, last_seen=str_timestamp)
            db.add(status)
        else:
            status.is_online = 1
            status.last_seen = str_timestamp

        db.commit()
        print(f"✅ Đã lưu RMS: {rms_value} của {device_id} vào Database!")

    except Exception as e:
        print(f"❌ Lỗi ghi ổ cứng: {e}")
        db.rollback()
    finally:
        db.close() 

# 4. Hàm chạy tự động khi kết nối thành công với Broker
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"🔌 Đã kết nối MQTT Broker (Mã trạng thái: {reason_code})")
    client.subscribe(MQTT_TOPIC)
    print(f"📡 Đang lắng nghe ESP32 qua Topic: {MQTT_TOPIC}")

# 5. Hàm chạy tự động mỗi khi ESP32 bắn tin nhắn lên
def on_message(client, userdata, msg):
    topic = msg.topic
    payload_str = msg.payload.decode('utf-8')
    print(f"📩 Nhận tin nhắn từ '{topic}': {payload_str}")
    
    try:
        data = json.loads(payload_str)
        parts = topic.split('/')
        if len(parts) >= 3:
            device_id = parts[1]
            msg_type = parts[2]
            
            if msg_type == "features":
                save_feature_data(device_id, data)
                
    except Exception as e:
        print(f"❌ Lỗi xử lý tin nhắn: {e}")

# 6. Hàm kích hoạt toàn bộ hệ thống MQTT
def start_mqtt():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message
    
    print("⏳ Đang khởi động trạm thu sóng MQTT...")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()