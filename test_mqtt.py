import json
import paho.mqtt.client as mqtt

client = mqtt.Client()

client.connect("https://edge-iot-project-1.onrender.com", 1883, 60)

payload = {
    "online": True,
    "timestamp": None
}

client.publish(
    "machine/motor_01/status",
    json.dumps(payload),
    qos=1
)

print("Đã gửi:", json.dumps(payload))

client.disconnect()