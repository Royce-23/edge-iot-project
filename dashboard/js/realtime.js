import { getLatest } from './api.js';
async function refresh() {
  const message = document.getElementById('message');
  try {
    const data = await getLatest();
    document.getElementById('payload').textContent = JSON.stringify(data, null, 2);
    message.textContent = 'Đã tải dữ liệu MOCK. Timestamp là thời điểm mẫu cố định.';
  } catch (error) {
    message.textContent = `${error.message}. Chạy mock_api.py và mở localhost:8000/dashboard/.`;
  }
}
document.getElementById('reload').addEventListener('click', refresh);
refresh();
// P5 TODO: polling/realtime lifecycle, stale data and connectivity display.
