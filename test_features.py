import json
import time
import random
import uuid
import paho.mqtt.client as mqtt

# ==============================
# MQTT CLIENT
# ==============================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

print("Đang kết nối Mosquitto...", flush=True)

client.connect("https://edge-iot-project-1.onrender.com", 1883, 60)
client.loop_start()

boot_id = "TEST-" + uuid.uuid4().hex[:8].upper()

print("===================================")
print("TEST NORMAL / WARNING / ABNORMAL")
print("Boot ID:", boot_id)
print("===================================")

sequence = 1

# Mỗi trạng thái chạy 5 lần x 2 giây = 10 giây
STATE_DURATION = 5

states = [
    "NORMAL",
    "WARNING",
    "ABNORMAL"
]

state_index = 0
state_count = 0

try:

    while True:

        health_state = states[state_index]

        # ==============================
        # TẠO DỮ LIỆU THEO TRẠNG THÁI
        # ==============================

        if health_state == "NORMAL":

            rms = round(random.uniform(0.30, 0.45), 3)
            anomaly = round(random.uniform(0.05, 0.20), 3)
            frequency = round(random.uniform(48.0, 52.0), 2)
            temperature = round(random.uniform(34.0, 37.0), 2)

        elif health_state == "WARNING":

            rms = round(random.uniform(0.50, 0.70), 3)
            anomaly = round(random.uniform(0.40, 0.65), 3)
            frequency = round(random.uniform(55.0, 65.0), 2)
            temperature = round(random.uniform(40.0, 45.0), 2)

        else:  # ABNORMAL

            rms = round(random.uniform(0.80, 1.20), 3)
            anomaly = round(random.uniform(0.75, 1.00), 3)
            frequency = round(random.uniform(70.0, 90.0), 2)
            temperature = round(random.uniform(50.0, 60.0), 2)

        # ==============================
        # MQTT PAYLOAD
        # ==============================

        payload = {
            "boot_id": boot_id,
            "sequence": sequence,
            "timestamp": time.time(),
            "uptime_ms": sequence * 2000,

            "sample_rate_hz": 800,
            "sample_count": 512,

            "rms": rms,
            "peak_to_peak": round(rms * 3.4, 3),
            "crest_factor": 3.4,

            "dominant_frequency": frequency,
            "band_energy": round(random.uniform(0.15, 0.25), 3),

            "anomaly_score": anomaly,
            "health_state": health_state,

            "temperature_c": temperature
        }

        message = json.dumps(payload)

        # ==============================
        # GỬI MQTT
        # ==============================

        result = client.publish(
            "machine/motor_01/features",
            message,
            qos=1
        )

        result.wait_for_publish()

        print(
            f"SEQ={sequence:03d} | "
            f"STATE={health_state:<8} | "
            f"RMS={rms} | "
            f"Anomaly={anomaly} | "
            f"Freq={frequency} Hz | "
            f"Temp={temperature} C",
            flush=True
        )

        # ==============================
        # CHUYỂN TRẠNG THÁI
        # ==============================

        sequence += 1
        state_count += 1

        if state_count >= STATE_DURATION:
            state_count = 0
            state_index += 1

            if state_index >= len(states):
                state_index = 0

            print(
                f"\n>>> CHUYỂN SANG {states[state_index]} <<<\n",
                flush=True
            )

        time.sleep(2)

except KeyboardInterrupt:

    print("\nĐã dừng gửi dữ liệu.")

finally:

    client.loop_stop()
    client.disconnect()