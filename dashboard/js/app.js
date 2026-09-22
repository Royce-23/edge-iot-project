// =========================================================
// EDGE-IOT DASHBOARD - APP
// Kết nối REST API Backend N4
// =========================================================


// =========================================================
// CONFIG
// =========================================================

const API_BASE = (window.EDGE_IOT_API_BASE || "https://edge-iot-project.onrender.com").replace(/\/$/, "");

const DEVICE_ID = new URLSearchParams(window.location.search).get("device") || "motor_01";

const REFRESH_INTERVAL = 2000;

let ws = null;
let wsConnected = false;

function connectWebSocket() {
    const wsUrl = `${API_BASE.replace(/^http/, "ws")}/ws/${encodeURIComponent(DEVICE_ID)}`;

    console.log("[WS] Connecting:", wsUrl);

    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        wsConnected = true;
        console.log("[WS] Connected");

        // WebSocket chỉ chứng minh backend hoạt động, không chứng minh ESP32 online.
    };

    ws.onmessage = (event) => {
        try {
            const message = JSON.parse(event.data);

            console.log("[WS] Message:", message);

            if (message.type === "features") {
                handleRealtimeFeatures(message.data);
            }

            else if (message.type === "status") {
                handleRealtimeStatus(message.data);
            }

            else if (message.type === "event") {
                handleRealtimeEvent(message.data);
            }

        } catch (error) {
            console.error("[WS] Invalid message:", error);
        }
    };

    ws.onerror = (error) => {
        console.error("[WS] Error:", error);
        wsConnected = false;
    };

    ws.onclose = () => {
        wsConnected = false;

        console.log("[WS] Disconnected. Reconnecting...");

        // thử kết nối lại sau 3 giây
        setTimeout(connectWebSocket, 3000);
    };
}

function handleRealtimeFeatures(data) {
    if (!data) return;

    console.log("[WS] Realtime features:", data);

    // Cập nhật các thông số realtime
    updateMetrics(data);

    // Cập nhật trạng thái máy
    if (data.health_state) {
        refreshDashboard();
    }

    // Cập nhật thông tin device
    updateDeviceInfo(data);

    // Cập nhật biểu đồ realtime
    if (typeof appendRealtimeChartPoint === "function") {
        appendRealtimeChartPoint(data);
    }
}


function handleRealtimeStatus(data) {
    if (!data) return;

    console.log("[WS] Realtime status:", data);

    refreshDashboard();
}


function handleRealtimeEvent(data) {
    if (!data) return;

    console.log("[WS] Realtime event:", data);

    // Sau khi nhận event mới,
    // tải lại danh sách event từ REST API
    loadEvents().then(renderEvents).catch(error => console.warn("Không tải được sự kiện:", error));
}

// =========================================================
// DATA FRESHNESS CONFIG
// =========================================================

const DATA_STALE_TIMEOUT = 10000; // 10 giây    


// =========================================================
// DOM HELPER
// =========================================================

function $(id) {
    return document.getElementById(id);
}


// =========================================================
// FORMAT NUMBER
// =========================================================

function formatNumber(value, digits = 3) {

    if (
        value === null ||
        value === undefined ||
        value === "" ||
        Number.isNaN(Number(value))
    ) {
        return "-";
    }

    return Number(value).toFixed(digits);
}


// =========================================================
// FORMAT TIME
// =========================================================

function formatTime(value) {

    if (
        value === null ||
        value === undefined ||
        value === ""
    ) {
        return "-";
    }


    let date;


    // Unix timestamp
    if (
        typeof value === "number" ||
        !isNaN(Number(value))
    ) {

        let timestamp = Number(value);

        // Nếu timestamp tính bằng milliseconds
        if (timestamp > 100000000000) {
            date = new Date(timestamp);
        }

        // Nếu timestamp tính bằng seconds
        else {
            date = new Date(timestamp * 1000);
        }
    }

    // ISO string
    else {

        date = new Date(value);
    }


    if (isNaN(date.getTime())) {
        return "-";
    }


    return date.toLocaleString("vi-VN");
}


// =========================================================
// FORMAT SHORT TIME
// =========================================================

function formatShortTime(value) {

    if (
        value === null ||
        value === undefined ||
        value === ""
    ) {
        return "-";
    }


    let date;


    if (
        typeof value === "number" ||
        !isNaN(Number(value))
    ) {

        let timestamp = Number(value);

        if (timestamp > 100000000000) {
            date = new Date(timestamp);
        }
        else {
            date = new Date(timestamp * 1000);
        }

    }
    else {

        date = new Date(value);
    }


    if (isNaN(date.getTime())) {
        return "-";
    }


    return date.toLocaleTimeString("vi-VN");
}


// =========================================================
// API GET
// =========================================================

async function apiGet(url) {

    const response = await fetch(url);


    if (!response.ok) {

        throw new Error(
            `HTTP ${response.status}: ${response.statusText}`
        );
    }


    return await response.json();
}


// =========================================================
// API URL
// =========================================================

function deviceUrl(path) {

    return (
        `${API_BASE}/api/devices/${encodeURIComponent(DEVICE_ID)}${path}`
    );
}


// =========================================================
// UPDATE CONNECTION STATUS
// =========================================================

function updateConnectionStatus(health) {

    const backendConnected = health?.backend === "ok";
    const connected = backendConnected && health.mqtt_connected === true;

    const dot =
        $("connectionDot");

    const text =
        $("connectionText");


    if (connected) {

        if (dot) {

            dot.classList.remove("offline");

            dot.classList.add("online");
        }


        if (text) {

            text.textContent =
                "MQTT CONNECTED";
        }

    }
    else {

        if (dot) {

            dot.classList.remove("online");

            dot.classList.add("offline");
        }


        if (text) {

            text.textContent = !backendConnected
                ? "BACKEND OFFLINE"
                : health.mqtt_enabled === false
                    ? "MQTT CHƯA CẤU HÌNH"
                    : "MQTT OFFLINE";
        }
    }
}


// =========================================================
// UPDATE MACHINE STATUS
// =========================================================

function updateMachineStatus(
    healthState,
    online = true,
    stale = false
) {

    const element =
        $("machineStatus") ||
        $("mainStatus") ||
        $("deviceStatus") ||
        $("statusValue");


    if (!element) {

        console.warn(
            "Không tìm thấy element machineStatus"
        );

        return;
    }


    let statusText =
        "OFFLINE";

    let statusClass =
        "offline";


    // -----------------------------------------
    // BACKEND OFFLINE
    // -----------------------------------------

    if (!online) {

        statusText =
            "OFFLINE";

        statusClass =
            "offline";
    }


    // -----------------------------------------
    // DATA STALE
    // -----------------------------------------

    else if (stale) {

        statusText =
            "OFFLINE";

        statusClass =
            "offline";
    }


    // -----------------------------------------
    // SENSOR OFF
    // -----------------------------------------

    else if (
        healthState === "OFF" ||
        healthState === "SENSOR_ERROR"
    ) {

        statusText =
            "SENSOR ERROR";

        statusClass =
            "sensor-error";
    }


    // -----------------------------------------
    // FAULT
    // -----------------------------------------

    else if (
        healthState === "FAULT" ||
        healthState === "ABNORMAL"
    ) {

        statusText =
            "ABNORMAL";

        statusClass =
            "abnormal";
    }


    // -----------------------------------------
    // WARNING
    // -----------------------------------------

    else if (
        healthState === "WARNING"
    ) {

        statusText =
            "WARNING";

        statusClass =
            "warning";
    }


    // -----------------------------------------
    // NORMAL
    // -----------------------------------------

    else {

        statusText =
            "NORMAL";

        statusClass =
            "normal";
    }


    element.textContent =
        statusText;


    // Xóa class cũ

    element.classList.remove(
        "normal",
        "warning",
        "abnormal",
        "offline",
        "sensor-error"
    );


    // Thêm class mới

    element.classList.add(
        statusClass
    );
}


// =========================================================
// UPDATE FRESHNESS
// =========================================================

function updateFreshness(latest, status) {

    const element = $("freshness");

    if (!element) {
        return;
    }

    if (!latest) {

        element.textContent = "Chưa có dữ liệu từ thiết bị";

        return;
    }

    const receivedAt = latest.timestamp ?? latest.received_at;

    if (
        receivedAt === null ||
        receivedAt === undefined
    ) {

        element.textContent = "Không rõ thời gian đo";

        return;
    }

    let receivedTime;

    if (!isNaN(Number(receivedAt))) {

        let timestamp = Number(receivedAt);

        if (timestamp < 100000000000) {
            timestamp *= 1000;
        }

        receivedTime = new Date(timestamp);

    } else {

        receivedTime = new Date(receivedAt);
    }

    if (isNaN(receivedTime.getTime())) {

        element.textContent = "Không rõ thời gian đo";

        return;
    }

    const age = Math.max(
        0,
        Math.floor(
            (Date.now() - receivedTime.getTime()) / 1000
        )
    );

    if (age < 5) {

        element.textContent =
            "Updated just now";

    } else if (age < 60) {

        element.textContent =
            `Updated ${age}s ago`;

    } else {

        const minutes =
            Math.floor(age / 60);

        element.textContent =
            `Updated ${minutes} min ago`;
    }

    // Nếu backend trả stale
    if (
        status &&
        status.stale === true
    ) {

        element.textContent +=
            " - STALE";
    }
}

// =========================================================
// CHECK DATA STALE
// =========================================================

function isDataStale(latest) {

    if (!latest) {
        return true;
    }

    const receivedAt =
        latest.timestamp ?? latest.received_at;

    if (
        receivedAt === null ||
        receivedAt === undefined
    ) {
        return true;
    }

    let timestamp;

    if (!isNaN(Number(receivedAt))) {

        timestamp =
            Number(receivedAt);

        // seconds -> milliseconds
        if (timestamp < 100000000000) {
            timestamp *= 1000;
        }

    }
    else {

        timestamp =
            new Date(receivedAt).getTime();
    }

    if (isNaN(timestamp)) {
        return true;
    }

    const age =
        Date.now() - timestamp;

    return age >= DATA_STALE_TIMEOUT;
}

// =========================================================
// UPDATE MAIN METRICS
// =========================================================

function updateMetrics(data) {

    if (!data) {
        return;
    }


    // -----------------------------------------
    // RMS
    // -----------------------------------------

    const rmsElement =
        $("rmsValue");


    if (rmsElement) {

        rmsElement.textContent =
            `${formatNumber(data.rms, 3)} g`;
    }


    // -----------------------------------------
    // ANOMALY SCORE
    // -----------------------------------------

    const anomalyElement =
        $("anomalyValue");


    if (anomalyElement) {

        anomalyElement.textContent =
            formatNumber(
                data.anomaly_score,
                3
            );
    }


    // -----------------------------------------
    // FREQUENCY
    // -----------------------------------------

    const frequencyElement =
        $("frequencyValue");


    if (frequencyElement) {

        frequencyElement.textContent =
            `${formatNumber(
                data.dominant_frequency,
                1
            )} Hz`;
    }


    // -----------------------------------------
    // TEMPERATURE
    // -----------------------------------------

    const temperatureElement =
        $("temperatureValue");


    if (temperatureElement) {

        if (
            data.temperature_c === null ||
            data.temperature_c === undefined
        ) {

            temperatureElement.textContent =
                "- °C";

        }
        else {

            temperatureElement.textContent =
                `${formatNumber(
                    data.temperature_c,
                    1
                )} °C`;
        }
    }
}

function clearMetrics() {
    for (const [id, value] of Object.entries({
        rmsValue: "- g", anomalyValue: "-", frequencyValue: "- Hz",
        temperatureValue: "- °C"
    })) {
        if ($(id)) $(id).textContent = value;
    }
}


// =========================================================
// UPDATE DEVICE INFORMATION
// =========================================================

function updateDeviceInfo(
    latest
) {

    if (!latest) {
        return;
    }


    // -----------------------------------------
    // DEVICE ID
    // -----------------------------------------

    if ($("deviceId")) {

        $("deviceId").textContent =
            latest.device_id ||
            DEVICE_ID;
    }


    // -----------------------------------------
    // FIRMWARE
    // -----------------------------------------

    if ($("firmware")) {

        $("firmware").textContent =
            latest.firmware ||
            latest.firmware_version ||
            "-";
    }


    // -----------------------------------------
    // RSSI
    // -----------------------------------------

    if ($("rssi")) {

        $("rssi").textContent =
            latest.rssi !== undefined &&
            latest.rssi !== null
                ? `${latest.rssi} dBm`
                : "-";
    }


    // -----------------------------------------
    // UPTIME
    // -----------------------------------------

    if ($("uptime")) {

        if (
            latest.uptime_ms !== undefined &&
            latest.uptime_ms !== null
        ) {

            const seconds =
                Math.floor(
                    Number(
                        latest.uptime_ms
                    ) / 1000
                );


            const hours =
                Math.floor(
                    seconds / 3600
                );


            const minutes =
                Math.floor(
                    (seconds % 3600) / 60
                );


            const sec =
                seconds % 60;


            $("uptime").textContent =
                `${hours}h ${minutes}m ${sec}s`;

        }
        else {

            $("uptime").textContent =
                "-";
        }
    }


    // -----------------------------------------
    // QUEUE DEPTH
    // -----------------------------------------

    if ($("queueDepth")) {

        $("queueDepth").textContent =
            latest.queue_depth ??
            latest.queueDepth ??
            "-";
    }


    // -----------------------------------------
    // MODEL VERSION
    // -----------------------------------------

    if ($("modelVersion")) {

        $("modelVersion").textContent =
            latest.model_version ??
            latest.modelVersion ??
            "-";
    }


    // -----------------------------------------
    // CONFIG VERSION
    // -----------------------------------------

    if ($("configVersion")) {

        $("configVersion").textContent =
            latest.config_version ??
            latest.configVersion ??
            "-";
    }


    // -----------------------------------------
    // SAMPLE RATE
    // -----------------------------------------

    if ($("sampleRate")) {

        if (
            latest.sample_rate_hz !== null &&
            latest.sample_rate_hz !== undefined
        ) {

            $("sampleRate").textContent =
                `${latest.sample_rate_hz} Hz`;

        }
        else {

            $("sampleRate").textContent =
                "-";
        }
    }


    // -----------------------------------------
    // SEQUENCE
    // -----------------------------------------

    if ($("sequence")) {

        $("sequence").textContent =
            latest.sequence ??
            "-";
    }
}


// =========================================================
// UPDATE CHARTS
// =========================================================

function updateCharts(
    history
) {

    if (
        !Array.isArray(history)
    ) {

        console.warn(
            "History không phải Array:",
            history
        );

        return;
    }


    if (
        typeof updateAllCharts ===
        "function"
    ) {

        updateAllCharts(history);

    }
    else {

        console.warn(
            "Không tìm thấy updateAllCharts()."
        );
    }
}


// =========================================================
// EVENT TYPE -> DISPLAY TEXT
// =========================================================

function eventTypeText(type) {

    switch (
        String(type || "").toUpperCase()
    ) {

        case "WARNING":
            return "WARNING";

        case "FAULT":
            return "ABNORMAL";

        case "ABNORMAL":
            return "ABNORMAL";

        case "SENSOR_ERROR":
            return "SENSOR ERROR";

        case "OFF":
            return "SENSOR ERROR";

        default:
            return type || "EVENT";
    }
}


// =========================================================
// EVENT CLASS
// =========================================================

function eventClass(type) {

    switch (
        String(type || "").toUpperCase()
    ) {

        case "WARNING":
            return "warning";

        case "FAULT":
        case "ABNORMAL":
            return "abnormal";

        case "SENSOR_ERROR":
        case "OFF":
            return "sensor-error";

        default:
            return "normal";
    }
}


// =========================================================
// RENDER EVENTS
// =========================================================

function renderEvents(events) {

    const container = $("eventList");

    if (!container) {
        console.warn("Không tìm thấy eventList");
        return;
    }

    container.innerHTML = "";

    if (!Array.isArray(events) || events.length === 0) {

        container.innerHTML = `
            <div class="event-item">
                <div class="event-message">
                    Không có sự kiện.
                </div>
            </div>
        `;

        return;
    }

    events.forEach(event => {

        const item = document.createElement("div");

        item.className = `event-item ${eventClass(event.type)}`;

        const eventTime =
            formatTime(event.timestamp || event.received_at);

        const acknowledged =
            event.acknowledged === true;

        item.innerHTML = `

            <div class="event-header">

                <strong>
                    ${eventTypeText(event.type)}
                </strong>

                <span class="event-time">
                    ${eventTime}
                </span>

            </div>

            <div class="event-message">
                ${event.message || "-"}
            </div>

            <div class="event-id">
                Event ID:
                ${event.event_id || "-"}
            </div>

            <div class="event-ack ${
                acknowledged
                    ? "acknowledged"
                    : "pending"
            }">

                ${
                    acknowledged
                        ? "✓ ACKNOWLEDGED"
                        : "⚠ CHƯA ACK"
                }

            </div>
        `;

        container.appendChild(item);
    });
}


// =========================================================
// LOAD LATEST
// =========================================================

async function loadLatest() {

    return await apiGet(
        deviceUrl("/latest")
    );
}


// =========================================================
// LOAD HISTORY
// =========================================================

async function loadHistory() {

    return await apiGet(
        deviceUrl("/history?limit=100")
    );
}


// =========================================================
// LOAD EVENTS
// =========================================================

async function loadEvents() {

    return await apiGet(
        deviceUrl("/events?limit=100")
    );
}


// =========================================================
// LOAD STATUS
// =========================================================

async function loadStatus() {

    return await apiGet(
        deviceUrl("/status")
    );
}

// =========================================================
// HISTORY FILTER
// =========================================================

let allHistoryData = [];


// =========================================================
// GET RECORD TIMESTAMP
// =========================================================

function getRecordTime(record) {

    let value =
        record.timestamp ??
        record.received_at;

    if (
        value === null ||
        value === undefined
    ) {
        return null;
    }

    let timestamp =
        Number(value);

    if (Number.isNaN(timestamp)) {

        const date =
            new Date(value);

        if (isNaN(date.getTime())) {
            return null;
        }

        return date.getTime();
    }

    // seconds -> milliseconds

    if (timestamp < 100000000000) {
        timestamp *= 1000;
    }

    return timestamp;
}


// =========================================================
// FILTER HISTORY
// =========================================================

function filterHistoryData() {

    const timeFilter =
        $("timeFilter")?.value || "all";

    const healthFilter =
        $("healthFilter")?.value || "ALL";


    let filtered =
        [...allHistoryData];


    // -----------------------------------------
    // TIME FILTER
    // -----------------------------------------

    if (timeFilter !== "all") {

        const minutes =
            Number(timeFilter);

        const now =
            Date.now();

        const startTime =
            now - minutes * 60 * 1000;


        filtered =
            filtered.filter(record => {

                const time =
                    getRecordTime(record);

                return (
                    time !== null &&
                    time >= startTime
                );
            });
    }


    // -----------------------------------------
    // HEALTH FILTER
    // -----------------------------------------

    if (healthFilter !== "ALL") {

        filtered =
            filtered.filter(record => {

                return String(
                    record.health_state || ""
                ).toUpperCase() === healthFilter;
            });
    }


    return filtered;
}


// =========================================================
// RENDER HISTORY TABLE
// =========================================================

function renderHistoryTable(data) {

    const tbody =
        $("historyTableBody");

    const count =
        $("historyCount");


    if (!tbody) {
        return;
    }


    if (count) {

        count.textContent =
            Array.isArray(data)
                ? data.length
                : 0;
    }


    if (
        !Array.isArray(data) ||
        data.length === 0
    ) {

        tbody.innerHTML = `
            <tr>
                <td colspan="9"
                    class="history-empty">
                    Không có dữ liệu phù hợp.
                </td>
            </tr>
        `;

        return;
    }


    tbody.innerHTML = "";


    // Hiển thị mới nhất trước

    const records =
        [...data].reverse();


    records.forEach(record => {

        const row =
            document.createElement("tr");


        const health =
            String(
                record.health_state || "-"
            ).toUpperCase();


        let healthClass =
            "normal";


        if (health === "WARNING") {

            healthClass =
                "warning";

        }
        else if (
            health === "ABNORMAL" ||
            health === "FAULT"
        ) {

            healthClass =
                "abnormal";
        }


        row.innerHTML = `

            <td>
                ${formatTime(
                    record.timestamp ||
                    record.received_at
                )}
            </td>

            <td>
                ${formatNumber(
                    record.rms,
                    3
                )}
            </td>

            <td>
                ${formatNumber(
                    record.peak_to_peak,
                    3
                )}
            </td>

            <td>
                ${formatNumber(
                    record.crest_factor,
                    3
                )}
            </td>

            <td>
                ${formatNumber(
                    record.dominant_frequency,
                    1
                )} Hz
            </td>

            <td>
                ${formatNumber(
                    record.band_energy,
                    3
                )}
            </td>

            <td>
                ${formatNumber(
                    record.anomaly_score,
                    3
                )}
            </td>

            <td class="history-status ${healthClass}">
                ${health}
            </td>

            <td>
                ${
                    record.temperature_c !== null &&
                    record.temperature_c !== undefined
                        ? `${formatNumber(
                            record.temperature_c,
                            1
                        )} °C`
                        : "-"
                }
            </td>
        `;


        tbody.appendChild(row);
    });
}


// =========================================================
// APPLY HISTORY FILTER
// =========================================================

function applyHistoryFilter() {

    const filtered =
        filterHistoryData();


    renderHistoryTable(
        filtered
    );
}


// =========================================================
// EXPORT HISTORY CSV
// =========================================================

function exportHistoryCSV() {

    const data =
        filterHistoryData();


    if (
        !Array.isArray(data) ||
        data.length === 0
    ) {

        alert(
            "Không có dữ liệu để xuất CSV."
        );

        return;
    }


    const headers = [

        "Time",
        "Device ID",
        "Sequence",
        "RMS",
        "Peak-to-Peak",
        "Crest Factor",
        "Dominant Frequency",
        "Band Energy",
        "Anomaly Score",
        "Health State",
        "Temperature C"
    ];


    const rows =
        data.map(record => [

            formatTime(
                record.timestamp ||
                record.received_at
            ),

            record.device_id ||
            DEVICE_ID,

            record.sequence ??
            "",

            record.rms ??
            "",

            record.peak_to_peak ??
            "",

            record.crest_factor ??
            "",

            record.dominant_frequency ??
            "",

            record.band_energy ??
            "",

            record.anomaly_score ??
            "",

            record.health_state ??
            "",

            record.temperature_c ??
            ""
        ]);


    const csvRows = [

        headers,

        ...rows

    ];


    const csvContent =
        csvRows
            .map(row =>
                row
                    .map(value => {

                        const text =
                            String(value);

                        return `"${text.replace(
                            /"/g,
                            '""'
                        )}"`;
                    })
                    .join(",")
            )
            .join("\n");


    // BOM để Excel đọc UTF-8

    const blob =
        new Blob(
            [
                "\uFEFF" +
                csvContent
            ],
            {
                type:
                    "text/csv;charset=utf-8;"
            }
        );


    const url =
        URL.createObjectURL(blob);


    const link =
        document.createElement("a");


    link.href =
        url;


    const now =
        new Date();


    const filename =
        `motor_01_history_${
            now.getFullYear()
        }-${
            String(
                now.getMonth() + 1
            ).padStart(2, "0")
        }-${
            String(
                now.getDate()
            ).padStart(2, "0")
        }.csv`;


    link.download =
        filename;


    document.body.appendChild(
        link
    );


    link.click();


    document.body.removeChild(
        link
    );


    URL.revokeObjectURL(
        url
    );
}

// =========================================================
// REFRESH DASHBOARD
// =========================================================

async function refreshDashboard() {

    console.log(
        "🔄 Refresh Dashboard..."
    );


    try {

        let backendHealth = null;
        try {
            backendHealth = await apiGet(`${API_BASE}/api/health`);
        } catch (error) {
            console.warn("Không kết nối được backend:", error.message);
        }

        // -----------------------------------------
        // LATEST
        // -----------------------------------------

        let latest = null;

        try {

            latest =
                await loadLatest();

        }
        catch (error) {

            console.warn(
                "Không lấy được latest:",
                error.message
            );
        }


        // -----------------------------------------
        // HISTORY
        // -----------------------------------------

        let history = [];

        try {

            history =
                await loadHistory();

        }
        catch (error) {

            console.warn(
                "Không lấy được history:",
                error.message
            );
        }


        // -----------------------------------------
        // EVENTS
        // -----------------------------------------

        let events = [];

        try {

            events =
                await loadEvents();

        }
        catch (error) {

            console.warn(
                "Không lấy được events:",
                error.message
            );
        }


        // -----------------------------------------
        // STATUS
        // -----------------------------------------

        let status = null;

        try {

            status =
                await loadStatus();

        }
        catch (error) {

            console.warn(
                "Không lấy được status:",
                error.message
            );
        }


// -----------------------------------------
// CHECK BACKEND
// -----------------------------------------

updateConnectionStatus(backendHealth);


        // -----------------------------------------
        // LATEST DATA
        // -----------------------------------------

        if (latest) {

            updateMetrics(
                latest
            );


            updateDeviceInfo(
                latest
            );
        } else {
            clearMetrics();
        }


        // -----------------------------------------
        // STATUS
        // -----------------------------------------

        if (status) {

    const dataStale =
        isDataStale(latest);


    const machineOnline =
        status.online === true &&
        !dataStale;


    const machineStale =
        status.stale === true ||
        dataStale;


    updateMachineStatus(

        status.health_state ||
        latest?.health_state ||
        "NORMAL",

        machineOnline,

        machineStale
    );


    updateFreshness(
        latest,
        status
    );

}
        else if (latest) {

            updateMachineStatus(

                latest.health_state || "OFF",
                false,
                true
            );


            updateFreshness(
                latest,
                null
            );

        }
        else {

            updateMachineStatus(
                "OFF",
                false,
                true
            );


            updateFreshness(
                null,
                null
            );
        }


// -----------------------------------------
// HISTORY + CHARTS
// -----------------------------------------

allHistoryData =
    Array.isArray(history)
        ? history
        : [];


// Update charts

if (
    allHistoryData.length > 0 &&
    !wsConnected
) {

    updateCharts(
        allHistoryData
    );
} else if (allHistoryData.length === 0 && typeof clearCharts === "function") {
    clearCharts();
}


// Update history table

applyHistoryFilter();


        // -----------------------------------------
        // EVENTS
        // -----------------------------------------

        renderEvents(
            events
        );


        console.log(
            "✅ Dashboard đã cập nhật."
        );


    }
    catch (error) {

        console.error(
            "❌ Dashboard error:",
            error
        );


        updateConnectionStatus(null);


        updateMachineStatus(
            "OFF",
            false,
            true
        );


        if ($("freshness")) {

            $("freshness").textContent =
                "Backend unavailable";
        }
    }
}


// =========================================================
// ACKNOWLEDGE ALL
// =========================================================

async function acknowledgeAll() {
    try {
        const events = await loadEvents();

        if (!Array.isArray(events) || events.length === 0) {
            alert("Không có event để ACK.");
            return;
        }

        const unacknowledgedEvents = events.filter(
            event => event.acknowledged !== true
        );

        if (unacknowledgedEvents.length === 0) {
            alert("Tất cả event đã được ACK.");
            return;
        }

        let successCount = 0;

        for (const event of unacknowledgedEvents) {

            if (!event.event_id) {
                continue;
            }

            const response = await fetch(
                `${API_BASE}/api/events/${encodeURIComponent(event.event_id)}/ack`,
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    }
                }
            );

            if (!response.ok) {
                throw new Error(
                    `ACK thất bại: ${event.event_id}`
                );
            }

            successCount++;
        }

        console.log(`✅ Đã ACK ${successCount} event`);

        // Load lại danh sách event
        const updatedEvents = await loadEvents();
        renderEvents(updatedEvents);

        alert(`Đã xác nhận ${successCount} event.`);

    } catch (error) {

        console.error("❌ Lỗi ACK:", error);

        alert(
            "Không thể ACK event.\n\n" +
            error.message
        );
    }
}


// =========================================================
// EVENT LISTENER
// =========================================================

function setupEventListeners() {

    // -----------------------------------------
    // ACK ALL
    // -----------------------------------------

    const ackButton =
        $("ackAllBtn");


    if (ackButton) {

        ackButton.addEventListener(
            "click",
            acknowledgeAll
        );
    }


    // -----------------------------------------
    // HISTORY TIME FILTER
    // -----------------------------------------

    const timeFilter =
        $("timeFilter");


    if (timeFilter) {

        timeFilter.addEventListener(
            "change",
            applyHistoryFilter
        );
    }


    // -----------------------------------------
    // HISTORY HEALTH FILTER
    // -----------------------------------------

    const healthFilter =
        $("healthFilter");


    if (healthFilter) {

        healthFilter.addEventListener(
            "change",
            applyHistoryFilter
        );
    }


    // -----------------------------------------
    // EXPORT CSV
    // -----------------------------------------

    const exportButton =
        $("exportCsvBtn");


    if (exportButton) {

        exportButton.addEventListener(
            "click",
            exportHistoryCSV
        );
    }
}


// =========================================================
// AUTO REFRESH
// =========================================================

let refreshTimer = null;


function startAutoRefresh() {

    if (refreshTimer) {

        clearInterval(
            refreshTimer
        );
    }


    refreshTimer =
        setInterval(
            refreshDashboard,
            REFRESH_INTERVAL
        );
}


// =========================================================
// INITIALIZE DASHBOARD
// =========================================================

async function initDashboard() {

    console.log(
        "================================"
    );

    console.log(
        "🚀 Edge-IoT Dashboard N5"
    );

    console.log(
        "Backend:",
        API_BASE
    );

    console.log(
        "Device:",
        DEVICE_ID
    );

    console.log(
        "================================"
    );


    setupEventListeners();


    await refreshDashboard();

    connectWebSocket();

    startAutoRefresh();
}


// =========================================================
// START
// =========================================================

document.addEventListener(
    "DOMContentLoaded",
    initDashboard
);
