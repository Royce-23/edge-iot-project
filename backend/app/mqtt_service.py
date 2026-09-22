import json
import time
import os
import asyncio

import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from sqlalchemy.orm import Session

from .database import SessionLocal, engine
from . import models


# =======================================================
# 1. CẤU HÌNH MQTT
# =======================================================

load_dotenv()

MQTT_HOST = os.getenv("MQTT_HOST", "127.0.0.1")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_TOPIC = "machine/#"


# =======================================================
# 2. WEBSOCKET
# =======================================================

websocket_manager = None
websocket_loop = None


def set_websocket_manager(manager):
    """
    Nhận ConnectionManager từ main.py.
    Được gọi khi FastAPI khởi động.
    """
    global websocket_manager
    global websocket_loop

    websocket_manager = manager

    try:
        websocket_loop = asyncio.get_running_loop()
    except RuntimeError:
        websocket_loop = None

    print("✅ WebSocket Manager đã được kết nối với MQTT")


def broadcast_realtime(device_id, message):

    if websocket_manager is None:
        return

    if websocket_loop is None:
        return

    if not device_id:
        print("⚠️ WebSocket broadcast thiếu device_id")
        return

    try:

        asyncio.run_coroutine_threadsafe(
            websocket_manager.broadcast(
                device_id,
                message
            ),
            websocket_loop
        )

    except Exception as e:

        print(
            f"⚠️ Không thể broadcast WebSocket: {e}"
        )


# =======================================================
# 3. TỰ ĐỘNG TẠO DATABASE
# =======================================================

models.Base.metadata.create_all(bind=engine)


# =======================================================
# 4. XỬ LÝ DỮ LIỆU FEATURES
#
# Topic:
# machine/{device_id}/features
# =======================================================

def handle_features_message(
    db: Session,
    device_id: str,
    payload: dict
):

    boot_id = payload.get("boot_id")
    sequence = payload.get("sequence")
    timestamp = payload.get("timestamp")


    # ---------------------------------------------------
    # DEDUPLICATION
    # ---------------------------------------------------
    existing_record = (
        db.query(models.FeatureRecord)
        .filter(
            models.FeatureRecord.device_id == device_id,
            models.FeatureRecord.boot_id == boot_id,
            models.FeatureRecord.sequence == sequence
        )
        .first()
    )

    if existing_record:

        print(
            f"⚠️ Đã chặn lưu trùng gói tin "
            f"seq={sequence} "
            f"(boot: {boot_id}) "
            f"của máy {device_id}"
        )

        return


    # ---------------------------------------------------
    # KHAI BÁO THIẾT BỊ NẾU CHƯA CÓ
    # ---------------------------------------------------

    device = (
        db.query(models.Device)
        .filter(
            models.Device.device_id == device_id
        )
        .first()
    )

    if not device:

        db.add(
            models.Device(
                device_id=device_id
            )
        )

        db.commit()


    # ---------------------------------------------------
    # SERVER RECEIVE TIME
    # ---------------------------------------------------

    received_at_str = str(time.time())


    # ---------------------------------------------------
    # TẠO FEATURE RECORD
    # ---------------------------------------------------

    new_record = models.FeatureRecord(

        device_id=device_id,

        boot_id=boot_id,

        sequence=sequence,

        timestamp=timestamp,

        received_at=received_at_str,

        uptime_ms=payload.get(
            "uptime_ms",
            0
        ),

        sample_rate_hz=payload.get(
            "sample_rate_hz",
            800.0
        ),

        sample_count=payload.get(
            "sample_count",
            512
        ),

        rms=payload.get(
            "rms",
            0.0
        ),

        peak_to_peak=payload.get(
            "peak_to_peak",
            0.0
        ),

        crest_factor=payload.get(
            "crest_factor",
            0.0
        ),

        dominant_frequency=payload.get(
            "dominant_frequency",
            0.0
        ),

        band_energy=payload.get(
            "band_energy",
            0.0
        ),

        anomaly_score=payload.get(
            "anomaly_score"
        ),

        health_state=payload.get(
            "health_state",
            "NORMAL"
        ),

        temperature_c=payload.get(
            "temperature_c"
        )
    )


    db.add(new_record)


    # ---------------------------------------------------
    # KIỂM TRA HEALTH STATE
    # ---------------------------------------------------

    health_state = payload.get(
        "health_state",
        "NORMAL"
    )


    # ---------------------------------------------------
    # TẠO HEALTH EVENT
    # ---------------------------------------------------

    event_id = None
    new_event = None

    if health_state in [
        "WARNING",
        "ABNORMAL",
        "FAULT"
    ]:

        event_id = (
            f"{device_id}_"
            f"{boot_id}_"
            f"{sequence}"
        )

        new_event = models.HealthEvent(

            event_id=event_id,

            device_id=device_id,

            timestamp=timestamp,

            received_at=received_at_str,

            type=health_state,

            message=(
                f"Phát hiện trạng thái "
                f"{health_state}. "
                f"Điểm bất thường (AI): "
                f"{payload.get('anomaly_score')}"
            )
        )

        db.add(new_event)

        print(
            f"🚨 BÁO ĐỘNG {health_state}: "
            f"Đã lưu sự kiện lỗi "
            f"cho {device_id}!"
        )


    # ---------------------------------------------------
    # COMMIT DATABASE
    # ---------------------------------------------------

    db.commit()

    # Refresh để lấy ID database
    db.refresh(new_record)


    print(
        f"✅ Đã lưu Features "
        f"(RMS: {new_record.rms}) "
        f"từ {device_id}"
    )


    # ===================================================
    # GỬI FEATURES REALTIME QUA WEBSOCKET
    # ===================================================

    broadcast_realtime(
        device_id,
        {

        "type": "features",

        "data": {

            "id": new_record.id,

            "boot_id": new_record.boot_id,

            "sequence": new_record.sequence,

            "timestamp": new_record.timestamp,

            "received_at": new_record.received_at,

            "uptime_ms": new_record.uptime_ms,

            "sample_rate_hz":
                new_record.sample_rate_hz,

            "sample_count":
                new_record.sample_count,

            "rms":
                new_record.rms,

            "peak_to_peak":
                new_record.peak_to_peak,

            "crest_factor":
                new_record.crest_factor,

            "dominant_frequency":
                new_record.dominant_frequency,

            "band_energy":
                new_record.band_energy,

            "anomaly_score":
                new_record.anomaly_score,

            "health_state":
                new_record.health_state,

            "temperature_c":
                new_record.temperature_c
        }
    }
    )


    # ===================================================
    # GỬI EVENT REALTIME QUA WEBSOCKET
    # ===================================================

    if new_event is not None:

        broadcast_realtime(
            device_id,
            {

            "type": "event",

            "data": {

                "event_id":
                    new_event.event_id,

                "timestamp":
                    new_event.timestamp,

                "received_at":
                    new_event.received_at,

                "type":
                    new_event.type,

                "message":
                    new_event.message,

                "acknowledged":
                    new_event.acknowledged
            }
        }
        )


# =======================================================
# 5. XỬ LÝ STATUS
#
# Topic:
# machine/{device_id}/status
# =======================================================

def handle_status_message(
    db: Session,
    device_id: str,
    payload: dict
):

    is_online = payload.get(
        "online",
        False
    )


    # ---------------------------------------------------
    # LẤY TIMESTAMP
    # ---------------------------------------------------

    last_seen_val = payload.get(
        "timestamp"
    )

    if last_seen_val is None:

        last_seen_val = int(
            time.time()
        )


    # ---------------------------------------------------
    # TÌM STATUS
    # ---------------------------------------------------

    status = (
        db.query(models.DeviceStatus)
        .filter(
            models.DeviceStatus.device_id
            == device_id
        )
        .first()
    )


    # ---------------------------------------------------
    # TẠO STATUS MỚI
    # ---------------------------------------------------

    if not status:

        status = models.DeviceStatus(

            device_id=device_id,

            online=is_online,

            last_seen=last_seen_val
        )

        db.add(status)


    # ---------------------------------------------------
    # UPDATE STATUS
    # ---------------------------------------------------

    else:

        status.online = is_online

        status.last_seen = last_seen_val


    # ---------------------------------------------------
    # COMMIT
    # ---------------------------------------------------

    db.commit()


    state_str = (
        "ONLINE 🟢"
        if is_online
        else "OFFLINE 🔴"
    )


    print(
        f"🌐 Cập nhật "
        f"{device_id} -> "
        f"{state_str}"
    )


    # ===================================================
    # GỬI STATUS REALTIME
    # ===================================================

    broadcast_realtime(
        device_id,
        {

        "type": "status",

        "data": {

            "online":
                is_online,

            "last_seen":
                last_seen_val
        }
    }
    )


# =======================================================
# 6. MQTT CONNECT
# =======================================================

def on_connect(
    client,
    userdata,
    flags,
    reason_code,
    properties=None
):

    print(
        f"🔌 Đã kết nối Mosquitto Broker "
        f"(Mã: {reason_code})"
    )


    # ---------------------------------------------------
    # SUBSCRIBE
    # ---------------------------------------------------

    client.subscribe(
        MQTT_TOPIC,
        qos=1
    )


    print(
        f"📡 Đang lắng nghe kênh: "
        f"{MQTT_TOPIC}"
    )


# =======================================================
# 7. MQTT MESSAGE
# =======================================================

def on_message(
    client,
    userdata,
    msg
):

    receive_time = time.time()

    topic = msg.topic


    try:

        # ------------------------------------------------
        # DECODE JSON
        # ------------------------------------------------

        payload_str = (
            msg.payload
            .decode("utf-8")
        )

        data = json.loads(
            payload_str
        )


        # ------------------------------------------------
        # TÁCH TOPIC
        #
        # machine/MACHINE-01/features
        # machine/MACHINE-01/status
        # ------------------------------------------------

        parts = topic.split("/")


        if len(parts) < 3:

            print(
                f"⚠️ Topic không đúng định dạng: "
                f"{topic}"
            )

            return


        device_id = parts[1]

        msg_type = parts[2]


        # ------------------------------------------------
        # DATABASE SESSION
        # ------------------------------------------------

        db = SessionLocal()


        try:

            # --------------------------------------------
            # TÍNH NETWORK LATENCY
            # --------------------------------------------

            send_time = data.get(
                "timestamp"
            )


            if send_time:

                try:

                    latency_ms = (
                        receive_time -
                        float(send_time)
                    ) * 1000

                    print(
                        f"⏱️ Độ trễ "
                        f"({msg_type}): "
                        f"{latency_ms:.2f} ms"
                    )

                except (
                    ValueError,
                    TypeError
                ):

                    pass


            # --------------------------------------------
            # FEATURES
            # --------------------------------------------

            if msg_type == "features":

                handle_features_message(
                    db,
                    device_id,
                    data
                )


            # --------------------------------------------
            # STATUS
            # --------------------------------------------

            elif msg_type == "status":

                handle_status_message(
                    db,
                    device_id,
                    data
                )


            # --------------------------------------------
            # TOPIC KHÔNG XÁC ĐỊNH
            # --------------------------------------------

            else:

                print(
                    f"⚠️ Không hỗ trợ message type: "
                    f"{msg_type}"
                )


        except Exception as e:

            print(
                f"❌ Lỗi ghi Database: "
                f"{e}"
            )

            db.rollback()


        finally:

            db.close()


    except json.JSONDecodeError:

        print(
            f"❌ Lỗi: Gói tin không phải "
            f"JSON chuẩn từ kênh "
            f"{topic}"
        )


    except Exception as e:

        print(
            f"❌ Lỗi xử lý tin nhắn: "
            f"{e}"
        )


# =======================================================
# 8. KHỞI ĐỘNG MQTT
# =======================================================

def start_mqtt():

    try:

        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2
        )

    except AttributeError:

        # Dự phòng cho paho-mqtt bản cũ
        client = mqtt.Client()


    # ---------------------------------------------------
    # CALLBACK
    # ---------------------------------------------------

    client.on_connect = on_connect

    client.on_message = on_message


    print(
        f"⏳ Đang khởi động trạm thu tại "
        f"{MQTT_HOST}:{MQTT_PORT}..."
    )


    try:

        client.connect(
            MQTT_HOST,
            MQTT_PORT,
            60
        )


        # Chạy MQTT background
        client.loop_start()


    except Exception as e:

        print(
            f"❌ KHÔNG THỂ BẬT MQTT: "
            f"{e}. "
            f"Nhớ kiểm tra file .env "
            f"hoặc bật Mosquitto lên nhé!"
        )