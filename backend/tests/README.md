# Kiểm thử tích hợp

Chạy từ thư mục gốc sau khi cài `backend/requirements.txt`:

```bash
PYTHONPATH=backend python -m unittest discover -s backend/tests -p 'test_*.py' -v
```

`test_live_pipeline.py` đưa payload qua MQTT callback, kiểm tra dedup, heartbeat,
retained ONLINE, dữ liệu cũ và các hàm REST. Không cần broker hay bo mạch.
