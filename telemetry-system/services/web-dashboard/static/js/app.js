// Nissan Leaf Web Dashboard JavaScript

// WebSocket connection
let socket = null;
let charts = {};
let currentTimeRange = '24h';

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    initWebSocket();
    initTabs();
    initHistoricalView();
    fetchInitialData();
});

// ============================================================================
// WebSocket Connection
// ============================================================================

function initWebSocket() {
    console.log('Connecting to WebSocket...');
    socket = io();

    socket.on('connect', () => {
        console.log('WebSocket connected');
        updateConnectionStatus(true);
        socket.emit('subscribe_realtime');
    });

    socket.on('disconnect', () => {
        console.log('WebSocket disconnected');
        updateConnectionStatus(false);
    });

    socket.on('realtime_update', (data) => {
        updateDashboard(data);
    });

    socket.on('connection_response', (data) => {
        console.log('Connection response:', data);
    });
}

function updateConnectionStatus(connected) {
    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');

    if (connected) {
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';
    } else {
        statusDot.classList.remove('connected');
        statusText.textContent = 'Disconnected';
    }
}

// ============================================================================
// Tab Navigation
// ============================================================================

function initTabs() {
    const tabs = document.querySelectorAll('.tab');
    const tabContents = document.querySelectorAll('.tab-content');

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const targetTab = tab.getAttribute('data-tab');

            // Remove active class from all tabs and contents
            tabs.forEach(t => t.classList.remove('active'));
            tabContents.forEach(content => content.classList.remove('active'));

            // Add active class to clicked tab and corresponding content
            tab.classList.add('active');
            document.getElementById(targetTab).classList.add('active');

            // Load historical data when switching to history tab
            if (targetTab === 'history') {
                loadHistoricalData();
            } else if (targetTab === 'details') {
                loadDetailsData();
            }
        });
    });
}

// ============================================================================
// Dashboard Updates
// ============================================================================

function fetchInitialData() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateDashboard(data);
        })
        .catch(error => {
            console.error('Error fetching initial data:', error);
        });
}

function updateDashboard(data) {
    // Update timestamp
    updateElement('lastUpdate', formatTime(data.timestamp));

    // Battery SOC
    if (data.battery_soc) {
        const soc = data.battery_soc.soc_percent || 0;
        updateElement('socValue', Math.round(soc));
        updateBatteryLevel(soc);
        updateElement('batteryVoltage', (data.battery_soc.pack_voltage || 0).toFixed(1));
        updateElement('batteryCurrent', (data.battery_soc.pack_current || 0).toFixed(1));
    }

    // Speed
    if (data.vehicle_speed) {
        const speed = data.vehicle_speed.speed_kmh || 0;
        updateElement('speedValue', Math.round(speed));
        updateSpeedGauge(speed);
    }

    // Motor RPM
    if (data.motor_rpm) {
        updateElement('motorRpm', data.motor_rpm.rpm || 0);
        updateElement('motorDirection', getDirectionText(data.motor_rpm.direction));
    }

    // Power
    if (data.inverter && data.inverter.power_kw !== undefined) {
        const power = data.inverter.power_kw;
        updateElement('powerValue', Math.abs(power).toFixed(1));
        updateElement('powerType', power > 0 ? 'Discharge' : power < 0 ? 'Regen' : 'Idle');
    } else if (data.battery_soc && data.battery_soc.pack_power_kw !== undefined) {
        const power = data.battery_soc.pack_power_kw;
        updateElement('powerValue', Math.abs(power).toFixed(1));
        updateElement('powerType', power > 0 ? 'Discharge' : power < 0 ? 'Regen' : 'Idle');
    }

    // Temperatures
    if (data.inverter) {
        updateElement('inverterTemp', data.inverter.temp_inverter || '--');
        updateElement('motorTemp', data.inverter.temp_motor || '--');
    }

    if (data.battery_temp) {
        updateElement('batteryTemp', data.battery_temp.temp_avg || '--');
        const min = data.battery_temp.temp_min;
        const max = data.battery_temp.temp_max;
        if (min !== undefined && max !== undefined) {
            updateElement('batteryTempRange', `${min}°C - ${max}°C`);
        }
    }

    // Charging
    if (data.charger) {
        const charging = data.charger.charging_flag === 1;
        const chargingCard = document.getElementById('chargingCard');
        const chargingStatus = document.getElementById('chargingStatus');

        if (charging) {
            chargingCard.classList.add('active');
            chargingStatus.textContent = 'Charging';
            const power = (data.charger.power_kw || 0).toFixed(1);
            const timeRemaining = data.charger.time_remaining_min || 0;
            updateElement('chargingPower', `${power} kW · ${timeRemaining} min`);
        } else {
            chargingCard.classList.remove('active');
            chargingStatus.textContent = 'Not Charging';
            updateElement('chargingPower', '--');
        }
    }

    // GPS
    if (data.gps_velocity) {
        updateElement('gpsSpeed', (data.gps_velocity.speed_kmh || 0).toFixed(1));
        updateElement('gpsHeading', Math.round(data.gps_velocity.heading || 0));
    }

    // Body voltage
    if (data.body_voltage) {
        updateElement('voltage12v', (data.body_voltage.voltage_12v || 0).toFixed(2));
    }
}

function updateElement(id, value) {
    const element = document.getElementById(id);
    if (element) {
        element.textContent = value;
    }
}

function updateBatteryLevel(soc) {
    const batteryLevel = document.getElementById('batteryLevel');
    if (batteryLevel) {
        batteryLevel.style.width = `${Math.min(Math.max(soc, 0), 100)}%`;
    }
}

function updateSpeedGauge(speed) {
    const maxSpeed = 160; // km/h
    const percentage = Math.min(speed / maxSpeed, 1);
    const totalLength = 251.2;
    const offset = totalLength - (totalLength * percentage);

    const speedArc = document.getElementById('speedArc');
    if (speedArc) {
        speedArc.style.strokeDashoffset = offset;
    }
}

function getDirectionText(direction) {
    if (direction === 0) return 'Stopped';
    if (direction === 1) return 'Forward';
    if (direction === 2) return 'Reverse';
    return '--';
}

function formatTime(isoString) {
    if (!isoString) return '--';
    const date = new Date(isoString);
    return date.toLocaleTimeString();
}

// ============================================================================
// Historical Data & Charts
// ============================================================================

function initHistoricalView() {
    const rangeButtons = document.querySelectorAll('.range-btn');
    rangeButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            rangeButtons.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            currentTimeRange = btn.getAttribute('data-range');
            loadHistoricalData();
        });
    });

    // Initialize Chart.js default settings
    Chart.defaults.color = '#b0b0b0';
    Chart.defaults.borderColor = '#3a3a3a';
}

function loadHistoricalData() {
    console.log(`Loading historical data for range: ${currentTimeRange}`);

    // Determine window size based on time range
    let window = '1m';
    if (currentTimeRange === '6h') window = '5m';
    else if (currentTimeRange === '24h') window = '10m';
    else if (currentTimeRange === '7d') window = '1h';

    // Load all charts
    loadChart('battery_soc', 'soc_percent', 'chartBatterySoc', '%', '#00ff88');
    loadChart('vehicle_speed', 'speed_kmh', 'chartSpeed', 'km/h', '#00b4d8');
    loadChart('battery_soc', 'pack_power_kw', 'chartPower', 'kW', '#ff8800');
    loadChart('battery_temp', 'temp_avg', 'chartBatteryTemp', '°C', '#ff3333');
    loadMultiLineChart();
    loadBatteryVIChart();
}

function loadChart(measurement, field, canvasId, unit, color) {
    const window = getWindowSize();

    fetch(`/api/historical/${measurement}/${field}?duration=${currentTimeRange}&window=${window}`)
        .then(response => response.json())
        .then(data => {
            renderChart(canvasId, data.data, field, unit, color);
        })
        .catch(error => {
            console.error(`Error loading chart ${canvasId}:`, error);
        });
}

function loadMultiLineChart() {
    const window = getWindowSize();

    Promise.all([
        fetch(`/api/historical/inverter/temp_inverter?duration=${currentTimeRange}&window=${window}`).then(r => r.json()),
        fetch(`/api/historical/inverter/temp_motor?duration=${currentTimeRange}&window=${window}`).then(r => r.json())
    ]).then(([inverterData, motorData]) => {
        renderMultiLineChart('chartTemps', [
            { label: 'Inverter', data: inverterData.data, color: '#ff8800' },
            { label: 'Motor', data: motorData.data, color: '#ff3333' }
        ], '°C');
    }).catch(error => {
        console.error('Error loading temperature chart:', error);
    });
}

function loadBatteryVIChart() {
    const window = getWindowSize();

    Promise.all([
        fetch(`/api/historical/battery_soc/pack_voltage?duration=${currentTimeRange}&window=${window}`).then(r => r.json()),
        fetch(`/api/historical/battery_soc/pack_current?duration=${currentTimeRange}&window=${window}`).then(r => r.json())
    ]).then(([voltageData, currentData]) => {
        renderDualAxisChart('chartBatteryVI', voltageData.data, currentData.data);
    }).catch(error => {
        console.error('Error loading battery V/I chart:', error);
    });
}

function renderChart(canvasId, dataPoints, label, unit, color) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;

    // Destroy existing chart
    if (charts[canvasId]) {
        charts[canvasId].destroy();
    }

    const ctx = canvas.getContext('2d');
    charts[canvasId] = new Chart(ctx, {
        type: 'line',
        data: {
            labels: dataPoints.map(d => new Date(d.time)),
            datasets: [{
                label: label,
                data: dataPoints.map(d => d.value),
                borderColor: color,
                backgroundColor: color + '20',
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                intersect: false,
                mode: 'index'
            },
            plugins: {
                legend: {
                    display: false
                },
                tooltip: {
                    backgroundColor: '#1a1a1a',
                    borderColor: color,
                    borderWidth: 1
                }
            },
            scales: {
                x: {
                    type: 'time',
                    time: {
                        displayFormats: {
                            hour: 'HH:mm',
                            day: 'MMM dd'
                        }
                    },
                    grid: {
                        color: '#2a2a2a'
                    }
                },
                y: {
                    grid: {
                        color: '#2a2a2a'
                    },
                    ticks: {
                        callback: function(value) {
                            return value + ' ' + unit;
                        }
                    }
                }
            }
        }
    });
}

function renderMultiLineChart(canvasId, datasets, unit) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;

    if (charts[canvasId]) {
        charts[canvasId].destroy();
    }

    const ctx = canvas.getContext('2d');
    charts[canvasId] = new Chart(ctx, {
        type: 'line',
        data: {
            labels: datasets[0].data.map(d => new Date(d.time)),
            datasets: datasets.map(ds => ({
                label: ds.label,
                data: ds.data.map(d => d.value),
                borderColor: ds.color,
                backgroundColor: ds.color + '20',
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 0
            }))
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                intersect: false,
                mode: 'index'
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'top'
                },
                tooltip: {
                    backgroundColor: '#1a1a1a',
                    borderColor: '#00ff88',
                    borderWidth: 1
                }
            },
            scales: {
                x: {
                    type: 'time',
                    time: {
                        displayFormats: {
                            hour: 'HH:mm',
                            day: 'MMM dd'
                        }
                    },
                    grid: {
                        color: '#2a2a2a'
                    }
                },
                y: {
                    grid: {
                        color: '#2a2a2a'
                    },
                    ticks: {
                        callback: function(value) {
                            return value + ' ' + unit;
                        }
                    }
                }
            }
        }
    });
}

function renderDualAxisChart(canvasId, voltageData, currentData) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;

    if (charts[canvasId]) {
        charts[canvasId].destroy();
    }

    const ctx = canvas.getContext('2d');
    charts[canvasId] = new Chart(ctx, {
        type: 'line',
        data: {
            labels: voltageData.map(d => new Date(d.time)),
            datasets: [
                {
                    label: 'Voltage (V)',
                    data: voltageData.map(d => d.value),
                    borderColor: '#00ff88',
                    backgroundColor: '#00ff8820',
                    borderWidth: 2,
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    yAxisID: 'y'
                },
                {
                    label: 'Current (A)',
                    data: currentData.map(d => d.value),
                    borderColor: '#00b4d8',
                    backgroundColor: '#00b4d820',
                    borderWidth: 2,
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    yAxisID: 'y1'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                intersect: false,
                mode: 'index'
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'top'
                }
            },
            scales: {
                x: {
                    type: 'time',
                    grid: {
                        color: '#2a2a2a'
                    }
                },
                y: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    grid: {
                        color: '#2a2a2a'
                    },
                    ticks: {
                        callback: function(value) {
                            return value + ' V';
                        }
                    }
                },
                y1: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    grid: {
                        drawOnChartArea: false
                    },
                    ticks: {
                        callback: function(value) {
                            return value + ' A';
                        }
                    }
                }
            }
        }
    });
}

function getWindowSize() {
    if (currentTimeRange === '1h') return '1m';
    if (currentTimeRange === '6h') return '5m';
    if (currentTimeRange === '24h') return '10m';
    if (currentTimeRange === '7d') return '1h';
    return '5m';
}

// ============================================================================
// Details View
// ============================================================================

function loadDetailsData() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateElement('detailsInverter', JSON.stringify(data.inverter || {}, null, 2));
            updateElement('detailsBattery', JSON.stringify({
                soc: data.battery_soc || {},
                temp: data.battery_temp || {}
            }, null, 2));
            updateElement('detailsMotor', JSON.stringify(data.motor_rpm || {}, null, 2));
            updateElement('detailsGps', JSON.stringify({
                position: data.gps_position || {},
                velocity: data.gps_velocity || {}
            }, null, 2));
        })
        .catch(error => {
            console.error('Error loading details:', error);
        });
}

// ============================================================================
// Victron BMS Updates
// ============================================================================

function updateVictronBMS(data) {
    // Update Victron Pack Status
    if (data.victron_pack) {
        const pack = data.victron_pack;
        updateElement('victronVoltage', pack.voltage?.toFixed(2));
        updateElement('victronCurrent', pack.current?.toFixed(1));
        updateElement('victronPower', pack.power_kw?.toFixed(2));
        updateElement('victronTemp', pack.temperature?.toFixed(1));
    }

    // Update SOC/SOH
    if (data.victron_soc) {
        const soc = data.victron_soc;
        updateElement('victronSOC', soc.soc_percent);
        updateElement('victronSOH', soc.soh_percent);
    }

    // Update Limits
    if (data.victron_limits) {
        const limits = data.victron_limits;
        updateElement('victronChargeVoltageMax', limits.charge_voltage_max?.toFixed(1));
        updateElement('victronChargeCurrentMax', limits.charge_current_max?.toFixed(1));
        updateElement('victronDischargeCurrentMax', limits.discharge_current_max?.toFixed(1));
        updateElement('victronDischargeVoltageMin', limits.discharge_voltage_min?.toFixed(1));
    }

    // Update Battery Info
    if (data.victron_characteristics) {
        const chars = data.victron_characteristics;
        const cellTypeName = getCellTypeName(chars.cell_type_code);
        updateElement('victronCellType', cellTypeName);
        updateElement('victronCellQty', chars.cell_quantity);
        updateElement('victronCapacity', chars.capacity_ah);
        updateElement('victronFirmware', `${chars.firmware_major}.${chars.firmware_minor}`);
    }
}

function getCellTypeName(code) {
    const types = {0: 'Unknown', 1: 'LiFePO4', 2: 'NMC'};
    return types[code] || 'Unknown';
}

function fetchVictronCells() {
    fetch('/api/victron/cells')
        .then(response => response.json())
        .then(data => {
            updateCellVoltageDisplay(data.cells);
        })
        .catch(error => {
            console.error('Error fetching cell data:', error);
        });
}

function updateCellVoltageDisplay(cells) {
    const container = document.getElementById('cellVoltageDisplay');
    if (!container || !cells) return;

    let html = '';
    for (let i = 0; i < 4; i++) {
        const moduleKey = `module_${i}`;
        const module = cells[moduleKey];
        
        if (module) {
            const voltageDelta = module.voltage_delta_mv;
            const tempDelta = module.temp_delta_c;
            
            // Color coding based on voltage imbalance
            let voltageClass = 'cell-balanced';
            if (voltageDelta > 100) voltageClass = 'cell-warning';
            else if (voltageDelta > 50) voltageClass = 'cell-caution';
            
            html += `
                <div class="cell-module ${voltageClass}">
                    <div class="module-header">Module ${i}</div>
                    <div class="cell-stats">
                        <div class="cell-stat">
                            <span class="stat-label">V Max:</span>
                            <span class="stat-value">${module.max_voltage_mv}mV</span>
                        </div>
                        <div class="cell-stat">
                            <span class="stat-label">V Min:</span>
                            <span class="stat-value">${module.min_voltage_mv}mV</span>
                        </div>
                        <div class="cell-stat delta">
                            <span class="stat-label">Δ:</span>
                            <span class="stat-value">${voltageDelta}mV</span>
                        </div>
                    </div>
                    <div class="cell-temps">
                        <span class="temp-max">${module.max_temp_c}°C</span>
                        <span class="temp-sep">|</span>
                        <span class="temp-min">${module.min_temp_c}°C</span>
                    </div>
                </div>
            `;
        }
    }
    
    container.innerHTML = html || '<p>No cell data available</p>';
}

// Call cell data fetch periodically (every 5 seconds)
setInterval(fetchVictronCells, 5000);

// Update the main updateDashboard function to include Victron updates
const originalUpdateDashboard = updateDashboard;
updateDashboard = function(data) {
    // Call original update function
    if (typeof originalUpdateDashboard === 'function') {
        originalUpdateDashboard(data);
    }
    
    // Update Victron BMS displays
    updateVictronBMS(data);
};

// Initial cell data fetch
setTimeout(fetchVictronCells, 2000);

