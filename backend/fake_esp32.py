import time
import json
import random
import paho.mqtt.client as mqtt

# 1. Cấu hình trạm phát (Phải khớp 100% với mqtt_service.py)
BROKER = "test.mosquitto.org"
PORT = 1883
DEVICE_ID = "MAY_BOM_01"
# Đổi kênh phát thành features để chuẩn bị cho việc phân loại dữ liệu sau này
TOPIC = f"machine/{DEVICE_ID}/features" 

def main():
    # Khởi tạo mạch phát (Mỗi mạch cần một ID ngẫu nhiên để không đụng độ nhau)
    client = mqtt.Client(client_id=f"Fake_ESP32_{random.randint(100,999)}")
    
    print(f"📡 Đang kết nối tới trạm MQTT {BROKER}...")
    client.connect(BROKER, PORT, 60)
    
    print("✅ Kết nối thành công! Bắt đầu phát sóng (nhấn Ctrl+C để tắt nguồn)")
    
    try:
        while True:
            # Bước 1: Mô phỏng dữ liệu nhiễu (Giống như đọc ADC và tính toán)
            rms = round(random.uniform(1.0, 5.5), 2)
            peak_to_peak = round(rms * random.uniform(1.4, 2.0), 2)
            crest_factor = round(peak_to_peak / rms, 2)
            
            # Bước 2: Đóng gói Payload thành JSON chuẩn
            payload = {
                "rms": rms,
                "peak_to_peak": peak_to_peak,
                "crest_factor": crest_factor,
                "timestamp": int(time.time()) # Lấy giờ hệ thống nhét vào để sau này đo độ trễ
            }
            
            payload_str = json.dumps(payload)
            
            # Bước 3: Bắn tín hiệu vô tuyến lên MQTT
            client.publish(TOPIC, payload_str)
            print(f"📤 Đã gửi: {payload_str} -> tới kênh: {TOPIC}")
            
            # Bước 4: Đứng chờ 3 giây rồi lặp lại (Y hệt hàm delay(3000) trong C/C++)
            time.sleep(3)
            
    except KeyboardInterrupt:
        print("\n🛑 Đã ngắt điện mạch giả lập ESP32.")
        client.disconnect()

if __name__ == "__main__":
    main()