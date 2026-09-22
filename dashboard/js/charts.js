// =========================================================
// EDGE-IOT DASHBOARD - CHARTS
// Dữ liệu lấy từ REST API của Backend N4
// =========================================================

let rmsChart = null;
let anomalyChart = null;
let frequencyChart = null;
let temperatureChart = null;


// =========================================================
// CHART CONFIG
// =========================================================

const MAX_POINTS = 100;


// =========================================================
// FORMAT TIME LABEL
// Hỗ trợ Unix timestamp và ISO string
// =========================================================

function chartTime(timestamp) {

    if (
        timestamp === null ||
        timestamp === undefined ||
        timestamp === ""
    ) {
        return "";
    }


    let date;


    // Unix timestamp
    if (
        typeof timestamp === "number" ||
        !isNaN(Number(timestamp))
    ) {

        let value = Number(timestamp);


        // milliseconds
        if (value > 100000000000) {

            date = new Date(value);

        }

        // seconds
        else {

            date = new Date(
                value * 1000
            );
        }

    }

    // ISO string
    else {

        date = new Date(timestamp);
    }


    if (isNaN(date.getTime())) {

        return "";
    }


    return date.toLocaleTimeString("vi-VN");
}


// =========================================================
// LẤY LABEL TỪ RECORD
// =========================================================

function getLabels(data) {

    return data.map(item => {

        if (
            item.timestamp !== null &&
            item.timestamp !== undefined
        ) {

            return chartTime(
                item.timestamp
            );
        }


        if (item.received_at) {

            return chartTime(
                item.received_at
            );
        }


        return "";
    });
}


// =========================================================
// DESTROY CHART
// Chỉ dùng khi cần xóa chart hoàn toàn
// =========================================================

function destroyChart(chart) {

    if (chart) {

        chart.destroy();
    }
}


// =========================================================
// TẠO CHART
// =========================================================

function createLineChart(
    canvasId,
    label,
    labels,
    values,
    yTitle
) {

    const canvas =
        document.getElementById(canvasId);


    if (!canvas) {

        console.warn(
            `Không tìm thấy canvas: ${canvasId}`
        );

        return null;
    }


    return new Chart(
        canvas.getContext("2d"),
        {

            type: "line",

            data: {

                labels: labels,

                datasets: [

                    {

                        label: label,

                        data: values,

                        borderWidth: 2,

                        pointRadius: 2,

                        pointHoverRadius: 5,

                        tension: 0.25,

                        fill: false
                    }

                ]
            },


            options: {

                responsive: true,

                maintainAspectRatio: false,

                animation: false,

                interaction: {

                    mode: "index",

                    intersect: false
                },


                plugins: {

                    legend: {

                        display: true
                    },

                    tooltip: {

                        enabled: true
                    }
                },


                scales: {

                    x: {

                        title: {

                            display: true,

                            text: "Thời gian"
                        }
                    },


                    y: {

                        title: {

                            display: true,

                            text: yTitle
                        },

                        beginAtZero: false
                    }
                }
            }
        }
    );
}

// =========================================================
// UPDATE EXISTING CHART
// Nếu chart chưa tồn tại -> tạo mới
// Nếu đã tồn tại -> chỉ cập nhật data
// =========================================================

function updateExistingChart(
    chart,
    canvasId,
    label,
    labels,
    values,
    yTitle
) {

    // -----------------------------------------
    // CHART CHƯA TỒN TẠI
    // -----------------------------------------

    if (!chart) {

        return createLineChart(
            canvasId,
            label,
            labels,
            values,
            yTitle
        );
    }


    // -----------------------------------------
    // CHART ĐÃ TỒN TẠI
    // Chỉ cập nhật dữ liệu
    // -----------------------------------------

    chart.data.labels =
        labels;

    chart.data.datasets[0].data =
        values;


    chart.update(
        "none"
    );


    return chart;
}


// =========================================================
// UPDATE RMS CHART
// =========================================================

function updateRMSChart(data) {

    if (!Array.isArray(data)) {

        return;
    }


    const records =
        data.slice(-MAX_POINTS);


    const labels =
        getLabels(records);


    const values =
        records.map(
            item =>
                Number(item.rms) || 0
        );


    rmsChart =
        updateExistingChart(

            rmsChart,

            "rmsChart",

            "RMS",

            labels,

            values,

            "RMS"
        );
}


// =========================================================
// UPDATE ANOMALY CHART
// =========================================================

function updateAnomalyChart(data) {

    if (!Array.isArray(data)) {

        return;
    }


    const records =
        data.slice(-MAX_POINTS);


    const labels =
        getLabels(records);


    const values =
        records.map(item => {

            if (
                item.anomaly_score === null ||
                item.anomaly_score === undefined
            ) {

                return null;
            }


            return Number(
                item.anomaly_score
            );
        });


    anomalyChart =
        updateExistingChart(

            anomalyChart,

            "anomalyChart",

            "Anomaly Score",

            labels,

            values,

            "Score"
        );
}


// =========================================================
// UPDATE FREQUENCY CHART
// =========================================================

function updateFrequencyChart(data) {

    if (!Array.isArray(data)) {

        return;
    }


    const records =
        data.slice(-MAX_POINTS);


    const labels =
        getLabels(records);


    const values =
        records.map(

            item =>
                Number(
                    item.dominant_frequency
                ) || 0
        );


    frequencyChart =
        updateExistingChart(

            frequencyChart,

            "frequencyChart",

            "Dominant Frequency",

            labels,

            values,

            "Frequency (Hz)"
        );
}


// =========================================================
// UPDATE TEMPERATURE CHART
// =========================================================

function updateTemperatureChart(data) {

    if (!Array.isArray(data)) {

        return;
    }


    const records =
        data.slice(-MAX_POINTS);


    const labels =
        getLabels(records);


    const values =
        records.map(item => {

            if (
                item.temperature_c === null ||
                item.temperature_c === undefined
            ) {

                return null;
            }


            return Number(
                item.temperature_c
            );
        });


    temperatureChart =
        updateExistingChart(

            temperatureChart,

            "temperatureChart",

            "Temperature",

            labels,

            values,

            "Temperature (°C)"
        );
}


// =========================================================
// UPDATE TẤT CẢ CHART
// =========================================================

function updateAllCharts(data) {

    if (!Array.isArray(data)) {

        console.warn(
            "updateAllCharts: dữ liệu không hợp lệ"
        );

        return;
    }


    updateRMSChart(
        data
    );


    updateAnomalyChart(
        data
    );


    updateFrequencyChart(
        data
    );


    updateTemperatureChart(
        data
    );
}


// =========================================================
// CLEAR CHART
// =========================================================

function clearCharts() {

    destroyChart(
        rmsChart
    );

    destroyChart(
        anomalyChart
    );

    destroyChart(
        frequencyChart
    );

    destroyChart(
        temperatureChart
    );


    rmsChart =
        null;

    anomalyChart =
        null;

    frequencyChart =
        null;

    temperatureChart =
        null;
}


// =========================================================
// EXPORT GLOBAL
// =========================================================

window.updateRMSChart =
    updateRMSChart;


window.updateAnomalyChart =
    updateAnomalyChart;


window.updateFrequencyChart =
    updateFrequencyChart;


window.updateTemperatureChart =
    updateTemperatureChart;


window.updateAllCharts =
    updateAllCharts;


window.clearCharts =
    clearCharts;


// =========================================================
// REALTIME CHART APPEND
// =========================================================

function appendRealtimeChartPoint(data) {
    if (!data) return;

    const timestamp =
        data.timestamp ??
        data.received_at ??
        Date.now();

    const label = chartTime(timestamp);

    if (rmsChart) {
        rmsChart.data.labels.push(label);
        rmsChart.data.datasets[0].data.push(Number(data.rms) || 0);
        trimChart(rmsChart);
        rmsChart.update("none");
    }

    if (anomalyChart) {
        anomalyChart.data.labels.push(label);
        anomalyChart.data.datasets[0].data.push(Number(data.anomaly_score) || 0);
        trimChart(anomalyChart);
        anomalyChart.update("none");
    }

    if (frequencyChart) {
        frequencyChart.data.labels.push(label);
        frequencyChart.data.datasets[0].data.push(
            Number(data.dominant_frequency) || 0
        );
        trimChart(frequencyChart);
        frequencyChart.update("none");
    }

    if (temperatureChart) {
        temperatureChart.data.labels.push(label);
        temperatureChart.data.datasets[0].data.push(
            Number(data.temperature_c) || 0
        );
        trimChart(temperatureChart);
        temperatureChart.update("none");
    }
}


// =========================================================
// LIMIT CHART POINTS
// =========================================================

function trimChart(chart) {

    while (
        chart.data.labels.length >
        MAX_POINTS
    ) {

        chart.data.labels.shift();

        chart.data.datasets.forEach(
            dataset => {
                dataset.data.shift();
            }
        );
    }
}

window.appendRealtimeChartPoint = appendRealtimeChartPoint;
