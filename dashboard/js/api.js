export async function getLatest(deviceId = 'motor_01') {
  const response = await fetch(`/api/devices/${encodeURIComponent(deviceId)}/latest`);
  if (!response.ok) throw new Error(`API trả mã ${response.status}`);
  return response.json();
}
