import json
import time
import os
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from sqlalchemy.orm import Session
from .database import SessionLocal, engine
from . import models

# 1. BẢO MẬT: Tải cấu hình mạng từ file ẩn .env
load_dotenv()
MQTT_HOST = os.getenv("MQTT_HOST", "127.0.0.1")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_TOPIC = "machine/#"

# Tự động tạo bảng database nếu chưa có
models.Base.metadata.create_all(bind=engine)

# =======================================================
# HÀM XỬ LÝ DỮ LIỆU ĐO ĐẠC (Topic: machine/+/features)
# =======================================================
def handle_features_message(db: Session, device_id: str, payload: dict):
    boot_id = payload.get("boot_id")
    sequence = payload.get("sequence")
    timestamp = payload.get("timestamp")
    
    # THUẬT TOÁN MỚI: Deduplication bằng (device_id, boot_id, sequence)
    existing_record = db.query(models.FeatureRecord).filter(
        models.FeatureRecord.device_id == device_id,
        models.FeatureRecord.boot_id == boot_id,
        models.FeatureRecord.sequence == sequence
    ).first()
    
    if existing_record:
        print(f"⚠️ Đã chặn lưu trùng gói tin seq={sequence} (boot: {boot_id}) của máy {device_id}")
        return

    # Khai báo máy mới nếu chưa tồn tại
    device = db.query(models.Device).filter(models.Device.device_id == device_id).first()
    if not device:
        db.add(models.Device(device_id=device_id))
        db.commit()

    # Dấu thời gian Server nhận được tin (luôn có)
    received_at_str = str(time.time())

    # Lưu toàn bộ 16 trường dữ liệu vào database
    new_record = models.FeatureRecord(
        device_id=device_id,
        boot_id=boot_id,
        sequence=sequence,
        timestamp=timestamp,
        received_at=received_at_str,
        uptime_ms=payload.get("uptime_ms", 0),
        sample_rate_hz=payload.get("sample_rate_hz", 800.0),
        sample_count=payload.get("sample_count", 512),
        rms=payload.get("rms", 0.0),
        peak_to_peak=payload.get("peak_to_peak", 0.0),
        crest_factor=payload.get("crest_factor", 0.0),
        dominant_frequency=payload.get("dominant_frequency", 0.0),
        band_energy=payload.get("band_energy", 0.0),
        anomaly_score=payload.get("anomaly_score"),
        health_state=payload.get("health_state", "NORMAL"),
        temperature_c=payload.get("temperature_c")
    )
    db.add(new_record)

    # NẾU MÁY BỊ LỖI -> TỰ ĐỘNG SINH CẢNH BÁO HEALTH EVENT
    health_state = payload.get("health_state", "NORMAL")
    if health_state in ["WARNING", "FAULT"]:
        event_id = f"{device_id}_{boot_id}_{sequence}" # Cấp ID duy nhất cho sự kiện
        new_event = models.HealthEvent(
            event_id=event_id,
            device_id=device_id,
            timestamp=timestamp,
            received_at=received_at_str,
            type=health_state,
            message=f"Phát hiện trạng thái {health_state}. Điểm bất thường (AI): {payload.get('anomaly_score')}"
        )
        db.add(new_event)
        print(f"🚨 BÁO ĐỘNG {health_state}: Đã lưu sự kiện lỗi cho {device_id}!")

    db.commit()
    print(f"✅ Đã lưu Features (RMS: {new_record.rms}) từ {device_id}")

# =======================================================
# HÀM XỬ LÝ TRẠNG THÁI MẠNG (Topic: machine/+/status)
# =======================================================
def handle_status_message(db: Session, device_id: str, payload: dict):
    is_online = payload.get("online", False)
    # Hợp đồng: mạch gửi timestamp=null khi chưa có mạng, ta lấy giờ hệ thống chữa cháy
    last_seen_val = payload.get("timestamp")
    if last_seen_val is None:
        last_seen_val = int(time.time())

    status = db.query(models.DeviceStatus).filter(models.DeviceStatus.device_id == device_id).first()
    if not status:
        status = models.DeviceStatus(device_id=device_id, online=is_online, last_seen=last_seen_val)
        db.add(status)
    else:
        status.online = is_online
        status.last_seen = last_seen_val
    
    db.commit()
    state_str = "ONLINE 🟢" if is_online else "OFFLINE 🔴"
    print(f"🌐 Cập nhật {device_id} -> {state_str}")


# =======================================================
# LÕI QUẢN LÝ GIAO TIẾP MQTT
# =======================================================
def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"🔌 Đã kết nối Mosquitto Broker (Mã: {reason_code})")
    # Match the firmware/contract QoS 1. QoS 1 may redeliver, so database
    # deduplication by (device_id, boot_id, sequence) remains required.
    client.subscribe(MQTT_TOPIC, qos=1)
    print(f"📡 Đang lắng nghe kênh: {MQTT_TOPIC}")

def on_message(client, userdata, msg):
    receive_time = time.time()
    topic = msg.topic
    
    try:
        payload_str = msg.payload.decode('utf-8')
        data = json.loads(payload_str)
        parts = topic.split('/')
        
        if len(parts) >= 3:
            device_id = parts[1]
            msg_type = parts[2]
            
            db = SessionLocal()
            try:
                # ĐO ĐỘ TRỄ MẠNG (Chỉ đo nếu mạch có gửi timestamp thực)
                send_time = data.get("timestamp")
                if send_time:
                    latency_ms = (receive_time - send_time) * 1000
                    print(f"⏱️ Độ trễ ({msg_type}): {latency_ms:.2f} ms")

                # CHIA LUỒNG XỬ LÝ THEO ĐÚNG HỢP ĐỒNG
                if msg_type == "features":
                    handle_features_message(db, device_id, data)
                elif msg_type == "status":
                    handle_status_message(db, device_id, data)
                
            except Exception as e:
                print(f"❌ Lỗi ghi Database: {e}")
                db.rollback()
            finally:
                db.close()
                
    except json.JSONDecodeError:
        print(f"❌ Lỗi: Gói tin không phải JSON chuẩn từ kênh {topic}")
    except Exception as e:
        print(f"❌ Lỗi xử lý tin nhắn: {e}")

def start_mqtt():
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    except AttributeError:
        client = mqtt.Client() # Dự phòng cho thư viện paho-mqtt bản cũ
        
    client.on_connect = on_connect
    client.on_message = on_message
    
    print(f"⏳ Đang khởi động trạm thu tại {MQTT_HOST}:{MQTT_PORT}...")
    try:
        client.connect(MQTT_HOST, MQTT_PORT, 60)
        client.loop_start()
    except Exception as e:
        print(f"❌ KHÔNG THỂ BẬT MQTT: {e}. Nhớ kiểm tra file .env hoặc bật Mosquitto lên nhé!")
