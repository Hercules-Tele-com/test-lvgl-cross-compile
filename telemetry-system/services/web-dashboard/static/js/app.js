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



// ============================================================================
// Energy Flow Diagram (Phase 2)
// ============================================================================

function updateEnergyFlow(data) {
    // Get power data from Victron BMS
    const victronPack = data.victron_pack;
    const charger = data.charger;

    if (!victronPack) return;

    const power = victronPack.power_kw || 0;  // kW (positive = discharge, negative = charge)
    const isCharging = charger && charger.charging_flag === 1;

    // Hide all arrows first
    document.getElementById('flowToMotor').setAttribute('opacity', '0');
    document.getElementById('flowFromCharger').setAttribute('opacity', '0');
    document.getElementById('flowFromMotor').setAttribute('opacity', '0');

    if (isCharging) {
        // Charging: Show charger to battery arrow
        document.getElementById('flowFromCharger').setAttribute('opacity', '1');
        const chargePower = Math.abs(power);
        document.getElementById('powerFromCharger').textContent = chargePower.toFixed(2) + ' kW';
    } else if (power > 0.1) {
        // Discharging: Show battery to motor arrow
        document.getElementById('flowToMotor').setAttribute('opacity', '1');
        document.getElementById('powerToMotor').textContent = power.toFixed(2) + ' kW';
    } else if (power < -0.1) {
        // Regenerating: Show motor to battery arrow
        document.getElementById('flowFromMotor').setAttribute('opacity', '1');
        const regenPower = Math.abs(power);
        document.getElementById('powerFromMotor').textContent = regenPower.toFixed(2) + ' kW';
    }

    // Update energy statistics (placeholder - would need backend calculation)
    // For now, just show current power direction
    updateElement('energyConsumed', '--');
    updateElement('energyRegen', '--');
    updateElement('energyNet', '--');
    updateElement('energyEfficiency', '--');
}

// Hook into existing updateDashboard function for energy flow
const originalUpdateDashboard2 = updateDashboard;
updateDashboard = function(data) {
    // Call original update function
    if (typeof originalUpdateDashboard2 === 'function') {
        originalUpdateDashboard2(data);
    }

    // Update energy flow diagram
    updateEnergyFlow(data);
};

// ============================================================================
// Trip Statistics (Phase 3)
// ============================================================================

let tripRefreshInterval = null;

function initTripsTab() {
    loadCurrentTrip();
    loadRecentTrips();

    // Auto-refresh every 5 seconds when on trips tab
    tripRefreshInterval = setInterval(() => {
        const tripsTab = document.getElementById('trips');
        if (tripsTab && tripsTab.classList.contains('active')) {
            loadCurrentTrip();
            loadRecentTrips();
        }
    }, 5000);
}

function loadCurrentTrip() {
    fetch('/api/trips/current')
        .then(response => response.json())
        .then(data => {
            updateCurrentTripDisplay(data);
        })
        .catch(error => {
            console.error('Error fetching current trip:', error);
        });
}

function updateCurrentTripDisplay(data) {
    const tripStatus = document.getElementById('tripStatus');
    const tripMetrics = document.getElementById('currentTripMetrics');

    if (data.status === 'no_active_trip' || !data.trip_id) {
        tripStatus.textContent = 'No active trip';
        tripStatus.style.display = 'block';
        tripMetrics.style.display = 'none';
    } else {
        tripStatus.style.display = 'none';
        tripMetrics.style.display = 'grid';

        const metrics = data.metrics || {};
        updateElement('tripDuration', (data.duration_minutes || 0).toFixed(1));
        updateElement('tripDistance', (metrics.distance_km || 0).toFixed(2));
        updateElement('tripEnergyConsumed', (metrics.energy_consumed_kwh || 0).toFixed(2));
        updateElement('tripEnergyRegen', (metrics.energy_regen_kwh || 0).toFixed(2));
        updateElement('tripAvgSpeed', (metrics.avg_speed_kmh || 0).toFixed(1));
        updateElement('tripEfficiency', Math.round(metrics.efficiency_wh_per_km || 0));
    }
}

function loadRecentTrips() {
    fetch('/api/trips/recent?limit=20')
        .then(response => response.json())
        .then(data => {
            updateRecentTripsTable(data.trips || []);
        })
        .catch(error => {
            console.error('Error fetching recent trips:', error);
        });
}

function updateRecentTripsTable(trips) {
    const tbody = document.getElementById('recentTripsBody');
    if (!tbody) return;

    if (trips.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="no-data">No trips recorded yet</td></tr>';
        return;
    }

    let html = '';
    trips.forEach(trip => {
        const endTime = new Date(trip.end_time);
        html += `
            <tr>
                <td>${endTime.toLocaleString()}</td>
                <td>${(trip.duration_minutes || 0).toFixed(0)} min</td>
                <td>${(trip.distance_km || 0).toFixed(2)} km</td>
                <td>${(trip.avg_speed_kmh || 0).toFixed(1)} km/h</td>
                <td>${(trip.energy_consumed_kwh || 0).toFixed(2)} kWh</td>
                <td>${(trip.energy_regen_kwh || 0).toFixed(2)} kWh</td>
                <td>${Math.round(trip.efficiency_wh_per_km || 0)} Wh/km</td>
            </tr>
        `;
    });

    tbody.innerHTML = html;
}

// ============================================================================
// GPS Map (Phase 5)
// ============================================================================

let gpsMap = null;
let vehicleMarker = null;
let routePolyline = null;
let lastGpsPosition = null;

function initGPSMap() {
    // Initialize Leaflet map
    if (!gpsMap) {
        gpsMap = L.map('gpsMap').setView([0, 0], 13);

        // Add OpenStreetMap tiles
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '© OpenStreetMap contributors',
            maxZoom: 19
        }).addTo(gpsMap);

        // Create vehicle marker
        vehicleMarker = L.marker([0, 0], {
            icon: L.divIcon({
                className: 'vehicle-marker',
                html: '🚗',
                iconSize: [30, 30]
            })
        }).addTo(gpsMap);

        // Setup map controls
        document.getElementById('centerMapButton').addEventListener('click', centerMapOnVehicle);
        document.getElementById('refreshRouteButton').addEventListener('click', loadGPSRoute);
        document.getElementById('showRouteCheckbox').addEventListener('change', toggleRouteDisplay);
        document.getElementById('routeDurationSelect').addEventListener('change', loadGPSRoute);
    }

    // Load initial GPS data
    updateGPSPosition();
    loadGPSRoute();

    // Auto-update every 5 seconds
    setInterval(() => {
        const mapTab = document.getElementById('map');
        if (mapTab && mapTab.classList.contains('active')) {
            updateGPSPosition();
        }
    }, 5000);
}

function updateGPSPosition() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            if (data.gps_position) {
                const lat = data.gps_position.latitude;
                const lon = data.gps_position.longitude;
                const alt = data.gps_position.altitude_m || 0;
                const sats = data.gps_position.satellites || 0;

                if (lat && lon) {
                    lastGpsPosition = [lat, lon];
                    vehicleMarker.setLatLng([lat, lon]);

                    // Update map info
                    updateElement('mapCurrentPosition', `${lat.toFixed(6)}, ${lon.toFixed(6)}`);
                    updateElement('mapAltitude', alt.toFixed(1));
                    updateElement('mapSatellites', sats);

                    // Center map on first position
                    if (gpsMap.getZoom() === 13) {
                        gpsMap.setView([lat, lon], 15);
                    }
                }
            }
        })
        .catch(error => {
            console.error('Error updating GPS position:', error);
        });
}

function loadGPSRoute() {
    const duration = document.getElementById('routeDurationSelect').value;

    fetch(`/api/gps/track?duration=${duration}`)
        .then(response => response.json())
        .then(data => {
            displayGPSRoute(data.track || []);
        })
        .catch(error => {
            console.error('Error loading GPS route:', error);
        });
}

function displayGPSRoute(trackPoints) {
    // Remove existing route
    if (routePolyline) {
        gpsMap.removeLayer(routePolyline);
        routePolyline = null;
    }

    if (trackPoints.length === 0) return;

    // Create polyline from track points
    const latLngs = trackPoints.map(point => [point.latitude, point.longitude]);

    routePolyline = L.polyline(latLngs, {
        color: '#00ff88',
        weight: 4,
        opacity: 0.7
    }).addTo(gpsMap);

    // Fit map to route bounds if we have multiple points
    if (latLngs.length > 1) {
        gpsMap.fitBounds(routePolyline.getBounds(), { padding: [50, 50] });
    }
}

function centerMapOnVehicle() {
    if (lastGpsPosition) {
        gpsMap.setView(lastGpsPosition, 15);
    }
}

function toggleRouteDisplay() {
    const showRoute = document.getElementById('showRouteCheckbox').checked;
    if (routePolyline) {
        routePolyline.setStyle({ opacity: showRoute ? 0.7 : 0 });
    }
}

// ============================================================================
// Data Export (Phase 6)
// ============================================================================

function initExportTab() {
    // Set default time range (last 24 hours)
    const now = new Date();
    const yesterday = new Date(now.getTime() - 24 * 60 * 60 * 1000);

    document.getElementById('exportStartTime').value = formatDateTimeLocal(yesterday);
    document.getElementById('exportEndTime').value = formatDateTimeLocal(now);

    // Quick range buttons
    const quickRangeButtons = document.querySelectorAll('.quick-range-buttons .range-btn');
    quickRangeButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const hours = parseInt(btn.getAttribute('data-hours'));
            const endTime = new Date();
            const startTime = new Date(endTime.getTime() - hours * 60 * 60 * 1000);

            document.getElementById('exportStartTime').value = formatDateTimeLocal(startTime);
            document.getElementById('exportEndTime').value = formatDateTimeLocal(endTime);
        });
    });

    // Export buttons
    document.getElementById('exportCsvButton').addEventListener('click', () => exportData('csv'));
    document.getElementById('exportJsonButton').addEventListener('click', () => exportData('json'));
}

function formatDateTimeLocal(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');

    return `${year}-${month}-${day}T${hours}:${minutes}`;
}

function exportData(format) {
    const measurement = document.getElementById('exportMeasurement').value;
    const startTime = document.getElementById('exportStartTime').value;
    const endTime = document.getElementById('exportEndTime').value;
    const statusDiv = document.getElementById('exportStatus');

    if (!startTime || !endTime) {
        statusDiv.textContent = 'Please select both start and end times';
        statusDiv.className = 'export-status error';
        return;
    }

    // Show loading status
    statusDiv.textContent = 'Preparing export...';
    statusDiv.className = 'export-status info';

    // Build API URL
    const params = new URLSearchParams({
        measurement: measurement,
        start: startTime,
        end: endTime
    });

    const apiUrl = `/api/export/${format}?${params.toString()}`;

    // Download file
    fetch(apiUrl)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            return response.blob();
        })
        .then(blob => {
            // Create download link
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `telemetry_${measurement}_${Date.now()}.${format}`;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);

            // Show success
            statusDiv.textContent = `Export successful! File downloaded.`;
            statusDiv.className = 'export-status success';
        })
        .catch(error => {
            console.error('Export error:', error);
            statusDiv.textContent = `Export failed: ${error.message}`;
            statusDiv.className = 'export-status error';
        });
}

// ============================================================================
// Enhanced Tab Initialization
// ============================================================================

// Update initTabs to handle new tabs
const originalInitTabs = initTabs;
initTabs = function() {
    originalInitTabs();

    const tabs = document.querySelectorAll('.tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const targetTab = tab.getAttribute('data-tab');

            // Load data for specific tabs
            if (targetTab === 'trips') {
                initTripsTab();
            } else if (targetTab === 'map') {
                initGPSMap();
            } else if (targetTab === 'export') {
                initExportTab();
            }
        });
    });
};
