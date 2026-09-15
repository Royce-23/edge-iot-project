import json
import time
import paho.mqtt.client as mqtt
from pathlib import Path

# Cấu hình phải khớp 100% với file .env của Backend
BROKER = "test.mosquitto.org"
PORT = 1883
DEVICE_ID = "motor_01"

def send_fake_data():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    print(f"🔌 Đang kết nối tới trạm {BROKER}...")
    client.connect(BROKER, PORT, 60)
    
    # ----------------------------------------------------
    # 1. Bắn tin nhắn: CẬP NHẬT TRẠNG THÁI (Online)
    # ----------------------------------------------------
    status_topic = f"machine/{DEVICE_ID}/status"
    status_payload = {
        "device_id": DEVICE_ID, 
        "online": True, 
        "timestamp": int(time.time())
    }
    # Hợp đồng yêu cầu topic status phải có Retain=True
    client.publish(status_topic, json.dumps(status_payload), qos=1, retain=True)
    print(f"🟢 Đã bắn trạng thái ONLINE -> {status_topic}")
    
    time.sleep(1) # Nghỉ 1 giây cho chân thực
    
    # ----------------------------------------------------
    # 2. Bắn tin nhắn: DỮ LIỆU ĐO ĐẠC (Features)
    # ----------------------------------------------------
    # Lấy dữ liệu mẫu từ file features.sample.json của nhóm
    try:
        ROOT = Path(__file__).resolve().parents[1] 
        json_path = ROOT / "test-data" / "features.sample.json"
        with open(json_path, "r", encoding="utf-8") as f:
            feature_payload = json.load(f)
            # Cập nhật thời gian thực để test
            feature_payload["timestamp"] = int(time.time())
            # Cố tình chỉnh RMS > 5.0 để test tính năng "Còi báo động" của Backend
            feature_payload["rms"] = 6.5 
            feature_payload["health_state"] = "WARNING"
    except Exception as e:
        print(f"❌ Không tìm thấy file JSON mẫu, kiểm tra lại đường dẫn: {e}")
        return

    feature_topic = f"machine/{DEVICE_ID}/features"
    client.publish(feature_topic, json.dumps(feature_payload), qos=1)
    print(f"📦 Đã bắn gói đo đạc (seq={feature_payload.get('sequence')}, RMS={feature_payload.get('rms')}) -> {feature_topic}")

    client.disconnect()
    print("✅ Hoàn tất giả lập!")

if __name__ == "__main__":
    send_fake_data()