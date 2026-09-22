import json
import paho.mqtt.client as mqtt

client = mqtt.Client()

client.connect("127.0.0.1", 1883, 60)

payload = {
    "online": True,
    "timestamp": None
}

client.publish(
    "machine/MACHINE-01/status",
    json.dumps(payload),
    qos=1
)

print("Đã gửi:", json.dumps(payload))

client.disconnect()