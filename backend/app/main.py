from fastapi import FastAPI

# Khởi tạo ứng dụng FastAPI
app = FastAPI(
    title="Edge-IoT Predictive Maintenance API",
    description="REST API cho hệ thống giám sát tình trạng máy quay",
    version="1.0.0"
)

@app.get("/")
def read_root():
    return {"message": "Server Backend đang hoạt động tốt!"}

@app.get("/api/devices")
def get_devices():
    """Lấy danh sách tất cả thiết bị và trạng thái online/offline"""
    # TODO: Kết nối SQLite để lấy dữ liệu thật
    return {"status": "success", "data": []}

@app.get("/api/devices/{device_id}/latest")
def get_latest_data(device_id: str):
    """Lấy đặc trưng (features) và trạng thái sức khỏe mới nhất của thiết bị"""
    return {"device_id": device_id, "message": "Chưa có dữ liệu"}

@app.get("/api/devices/{device_id}/history")
def get_history_data(device_id: str):
    """Lấy dữ liệu lịch sử theo khoảng thời gian"""
    return {"device_id": device_id, "history": []}

@app.get("/api/devices/{device_id}/events")
def get_device_events(device_id: str):
    """Lấy danh sách cảnh báo (alerts) của thiết bị"""
    return {"device_id": device_id, "events": []}

@app.get("/api/devices/{device_id}/status")
def get_device_status(device_id: str):
    """Lấy thông tin last_seen và tình trạng kết nối mạng"""
    return {"device_id": device_id, "status": "offline"}