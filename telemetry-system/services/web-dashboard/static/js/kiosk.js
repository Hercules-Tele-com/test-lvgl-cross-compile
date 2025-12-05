// Kiosk Mode Dashboard JavaScript
// Optimized for Victron Cerbo 50 (1280x720)

let socket = null;
let map = null;
let marker = null;
let currentPage = 'overview';

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    initClock();
    initWebSocket();
    initNavigation();
    initBatteryToggle();
    initGPSMap();
    fetchInitialData();
});

// ============================================================================
// Clock
// ============================================================================

function initClock() {
    updateClock();
    // Update clock every second
    setInterval(updateClock, 1000);
}

function updateClock() {
    const now = new Date();
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');

    const clockElement = document.getElementById('clock');
    if (clockElement) {
        clockElement.textContent = `${hours}:${minutes}:${seconds}`;
    }
}

// ============================================================================
// WebSocket Connection
// ============================================================================

function initWebSocket() {
    console.log('Connecting to WebSocket...');
    socket = io();

    socket.on('connect', () => {
        console.log('WebSocket connected');
        socket.emit('subscribe_realtime');
    });

    socket.on('disconnect', () => {
        console.log('WebSocket disconnected');
    });

    socket.on('realtime_update', (data) => {
        updateDashboard(data);
    });
}

// ============================================================================
// Navigation
// ============================================================================

function initNavigation() {
    console.log('Initializing navigation...');
    const navButtons = document.querySelectorAll('.nav-button');
    console.log(`Found ${navButtons.length} navigation buttons`);

    navButtons.forEach((button, index) => {
        const pageName = button.getAttribute('data-page');
        console.log(`Attaching click handler to button ${index}: ${pageName}`);

        button.addEventListener('click', (event) => {
            console.log(`Button clicked: ${pageName}`);
            event.preventDefault();
            event.stopPropagation();
            switchPage(pageName);
        });
    });

    console.log('Navigation initialization complete');
}

function switchPage(pageName) {
    console.log(`Switching to page: ${pageName}`);

    // Hide all pages
    const pages = document.querySelectorAll('.page');
    console.log(`Found ${pages.length} pages`);
    pages.forEach(page => {
        page.classList.remove('active');
    });

    // Remove active from all nav buttons
    const navButtons = document.querySelectorAll('.nav-button');
    navButtons.forEach(btn => {
        btn.classList.remove('active');
    });

    // Show target page
    const targetPage = document.getElementById(`page-${pageName}`);
    if (targetPage) {
        targetPage.classList.add('active');
        console.log(`Activated page: page-${pageName}`);
    } else {
        console.error(`Page not found: page-${pageName}`);
    }

    // Activate nav button
    const targetButton = document.querySelector(`[data-page="${pageName}"]`);
    if (targetButton) {
        targetButton.classList.add('active');
        console.log(`Activated button for: ${pageName}`);
    } else {
        console.error(`Button not found for page: ${pageName}`);
    }

    currentPage = pageName;

    // Refresh map if switching to GPS page
    if (pageName === 'gps' && map) {
        console.log('Refreshing GPS map...');
        setTimeout(() => {
            map.invalidateSize();
        }, 100);
    }
}

// ============================================================================
// Battery View Toggle
// ============================================================================

let batteryViewMode = 'summary'; // 'summary' or 'cells'

function initBatteryToggle() {
    const toggleBtn = document.getElementById('batteryViewToggle');
    if (toggleBtn) {
        toggleBtn.addEventListener('click', () => {
            if (batteryViewMode === 'summary') {
                // Switch to cell view
                batteryViewMode = 'cells';
                document.getElementById('batterySummaryView').classList.remove('active');
                document.getElementById('batteryCellsView').classList.add('active');
                toggleBtn.textContent = '📋 Summary';
            } else {
                // Switch to summary view
                batteryViewMode = 'summary';
                document.getElementById('batteryCellsView').classList.remove('active');
                document.getElementById('batterySummaryView').classList.add('active');
                toggleBtn.textContent = '📊 Cell Chart';
            }
        });
    }
}

// ============================================================================
// Data Updates
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

function isDataFresh(dataObject, maxAgeSeconds = 60) {
    /**
     * Check if data is fresh (less than maxAgeSeconds old)
     * Returns true if fresh, false if stale or no timestamp
     */
    if (!dataObject || !dataObject.time) {
        return false;
    }

    try {
        const dataTime = new Date(dataObject.time);
        const now = new Date();
        const ageSeconds = (now - dataTime) / 1000;

        return ageSeconds <= maxAgeSeconds;
    } catch (e) {
        console.error('Error checking data freshness:', e);
        return false;
    }
}

function updateDashboard(data) {
    // Update hostname in title
    if (data.hostname) {
        const title = document.getElementById('page-title');
        if (title) {
            title.textContent = `⚡ EMBOO - ${data.hostname}`;
        }
    }

    // Battery data (EMBOO/Orion BMS - Schema V2)
    // Only show data if it's less than 60 seconds old
    if (data.battery && isDataFresh(data.battery, 60)) {
        const b = data.battery;

        // SOC
        const soc = Math.round(b.soc_percent || 0);
        updateElement('soc', soc + '%');
        updateElement('soc-detail', soc);

        // Color code SOC
        const socElements = [document.getElementById('soc'), document.getElementById('soc-detail')];
        socElements.forEach(el => {
            if (el) {
                el.classList.remove('warning', 'error');
                if (soc < 20) {
                    el.classList.add('error');
                } else if (soc < 40) {
                    el.classList.add('warning');
                }
            }
        });

        // Voltage, Current, Power
        updateElement('voltage', (b.voltage || 0).toFixed(1));
        updateElement('current', (b.current || 0).toFixed(1));

        const power = b.power_kw || ((b.voltage || 0) * (b.current || 0) / 1000);
        updateElement('power', power.toFixed(2));
        updateElement('power-detail', power.toFixed(2));

        // Battery Temperature
        const batteryTemp = b.temp_avg || b.temp_high || b.temperature || 0;
        updateElement('batteryTemp', Math.round(batteryTemp));
        updateElement('temp', Math.round(batteryTemp));

        // Color code battery temperature
        updateTemperatureColor('batteryTempItem', batteryTemp, 20, 50, 60);

        // Update battery cell data (if available)
        if (b.cell_voltages && Array.isArray(b.cell_voltages)) {
            updateBatteryCells(b.cell_voltages);
        }

        // Cell range and delta
        if (b.cell_max_voltage !== undefined && b.cell_min_voltage !== undefined) {
            const cellRange = b.cell_max_voltage - b.cell_min_voltage;
            updateElement('cellRange', cellRange.toFixed(0));
            updateElement('cellDelta', cellRange.toFixed(0));
            updateElement('cellDeltaChart', cellRange.toFixed(0));
        }
    } else {
        // Battery data is stale or unavailable - show "--"
        updateElement('soc', '--');
        updateElement('soc-detail', '--');
        updateElement('batteryTemp', '--');
        updateElement('temp', '--');
        updateElement('voltage', '--');
        updateElement('current', '--');
        updateElement('power', '--');
        updateElement('power-detail', '--');
        updateElement('cellRange', '--');
        updateElement('cellDelta', '--');
        updateElement('cellDeltaChart', '--');
    }

    // Motor data - only show if less than 60 seconds old
    if (data.motor && isDataFresh(data.motor, 60)) {
        const m = data.motor;
        updateElement('rpm', m.rpm || 0);
        updateElement('torque', m.torque_actual || 0);

        // Motor temperature
        const motorTemp = m.temp_stator || 0;
        updateElement('motorTemp', motorTemp ? Math.round(motorTemp) : '--');
        updateElement('motorTempOverview', motorTemp ? Math.round(motorTemp) : '--');
        updateTemperatureColor('motorTempItem', motorTemp, 40, 80, 100);

        // DC Bus Voltage - scale down by 10
        const dcBusVoltage = m.voltage_dc_bus || 0;
        updateElement('dcBus', dcBusVoltage ? (dcBusVoltage / 10).toFixed(1) : '--');

        // DNR Selector - Update based on direction
        const dnrD = document.getElementById('dnr-d');
        const dnrN = document.getElementById('dnr-n');
        const dnrR = document.getElementById('dnr-r');

        if (dnrD && dnrN && dnrR) {
            // Remove all active classes
            dnrD.classList.remove('active', 'drive', 'reverse', 'neutral');
            dnrN.classList.remove('active', 'drive', 'reverse', 'neutral');
            dnrR.classList.remove('active', 'drive', 'reverse', 'neutral');

            // Set active based on direction
            if (m.direction === 1) {
                // Drive
                dnrD.classList.add('active', 'drive');
            } else if (m.direction === 2) {
                // Reverse
                dnrR.classList.add('active', 'reverse');
            } else {
                // Neutral (default)
                dnrN.classList.add('active', 'neutral');
            }
        }

        // Direction (for motor page)
        let direction = '--';
        if (m.direction === 1) direction = 'Forward';
        else if (m.direction === 2) direction = 'Reverse';
        else if (m.direction === 0) direction = 'Neutral';
        updateElement('direction', direction);
    } else {
        // Motor data is stale or unavailable - show "--"
        updateElement('rpm', '--');
        updateElement('torque', '--');
        updateElement('motorTemp', '--');
        updateElement('motorTempOverview', '--');
        updateElement('dcBus', '--');
        updateElement('direction', '--');

        // Reset DNR selector to neutral when data is stale
        const dnrD = document.getElementById('dnr-d');
        const dnrN = document.getElementById('dnr-n');
        const dnrR = document.getElementById('dnr-r');
        if (dnrD && dnrN && dnrR) {
            dnrD.classList.remove('active', 'drive', 'reverse', 'neutral');
            dnrN.classList.remove('active', 'drive', 'reverse', 'neutral');
            dnrR.classList.remove('active', 'drive', 'reverse', 'neutral');
            dnrN.classList.add('active', 'neutral'); // Default to neutral when stale
        }
    }

    // Inverter data - only show if less than 60 seconds old
    if (data.inverter && isDataFresh(data.inverter, 60)) {
        const inverterTemp = data.inverter.temp_inverter || 0;
        updateElement('inverterTemp', Math.round(inverterTemp));
        updateElement('inverterTempOverview', Math.round(inverterTemp));
        updateTemperatureColor('inverterTempItem', inverterTemp, 40, 80, 100);

        // Fallback motor temp from inverter if motor data doesn't have it
        if (!data.motor || !data.motor.temp_stator) {
            const motorTempFromInverter = data.inverter.temp_motor || 0;
            updateElement('motorTemp', Math.round(motorTempFromInverter));
            updateElement('motorTempOverview', Math.round(motorTempFromInverter));
            updateTemperatureColor('motorTempItem', motorTempFromInverter, 40, 80, 100);
        }
    }

    // GPS data - distinguish between service not running and no fix
    if (data.gps && isDataFresh(data.gps, 60)) {
        const g = data.gps;

        // Check if we have a valid GPS fix
        const hasValidFix = g.latitude !== undefined &&
                          g.longitude !== undefined &&
                          g.latitude !== 0 &&
                          g.longitude !== 0 &&
                          (g.fix_quality === undefined || g.fix_quality > 0);

        if (hasValidFix) {
            // GPS has a fix - show all data
            // Speed
            if (g.speed_kmh !== undefined) {
                updateElement('speed', g.speed_kmh.toFixed(1));
                updateElement('gpsSpeed', g.speed_kmh.toFixed(1));
            }

            // Altitude
            if (g.altitude !== undefined) {
                updateElement('altitude', Math.round(g.altitude));
            } else {
                updateElement('altitude', '--');
            }

            // Satellites
            if (g.satellites !== undefined) {
                updateElement('satellites', g.satellites);
            } else {
                updateElement('satellites', '--');
            }

            // Heading
            if (g.heading !== undefined) {
                updateElement('heading', Math.round(g.heading));
            } else {
                updateElement('heading', '--');
            }

            // Update map
            updateMapPosition(g.latitude, g.longitude, g.heading || 0);

            // Update GPS status to indicate active fix
            if (marker) {
                marker.bindPopup('<b>GPS Fix Active</b><br>' +
                    `Lat: ${g.latitude.toFixed(6)}<br>` +
                    `Lon: ${g.longitude.toFixed(6)}<br>` +
                    `Satellites: ${g.satellites || 'N/A'}`);
            }
        } else {
            // GPS service is running but no fix
            updateElement('speed', '--');
            updateElement('gpsSpeed', '--');
            updateElement('altitude', '--');
            updateElement('satellites', g.satellites || '0');
            updateElement('heading', '--');

            // Show "searching for fix" message on map
            if (marker) {
                marker.bindPopup('<b>GPS Service Active</b><br>Searching for fix...<br>' +
                    `Satellites: ${g.satellites || 0}`);
            }
        }
    } else {
        // GPS service is not running or data is stale
        updateElement('speed', '--');
        updateElement('gpsSpeed', '--');
        updateElement('altitude', '--');
        updateElement('satellites', '--');
        updateElement('heading', '--');

        // Show "service offline" message on map
        if (marker) {
            marker.bindPopup('<b>GPS Service Offline</b><br>No recent data');
        }
    }

    // Charger data
    const chargingStatusDiv = document.getElementById('chargingStatus');
    const chargerOnline = data.charger && data.status_indicators && data.status_indicators.charger === 'online';

    // Track previous charger state for auto-switch
    if (!window.previousChargerState) {
        window.previousChargerState = false;
    }

    if (chargerOnline) {
        const c = data.charger;
        const isCharging = c.charging_state === 1;

        // Auto-switch to charger page when charger comes online
        if (!window.previousChargerState && currentPage !== 'charger') {
            console.log('Charger connected - auto-switching to charger page');
            switchPage('charger');
        }
        window.previousChargerState = true;

        // Update status display
        if (chargingStatusDiv) {
            if (isCharging) {
                chargingStatusDiv.classList.add('active');
            } else {
                chargingStatusDiv.classList.remove('active');
            }
        }

        // Update values
        updateElement('chargerVoltage', (c.output_voltage || 0).toFixed(1));
        updateElement('chargerCurrent', (c.output_current || 0).toFixed(1));
        updateElement('chargingPower', (c.charge_power_kw || 0).toFixed(2));

        // Status text
        let statusText = isCharging ? '🔋 Charging' : '⏸️ Idle';
        if (c.hw_status === 1) statusText += ' [HW Fault]';
        if (c.temp_status === 1) statusText += ' [Over-Temp]';
        if (c.input_voltage_status === 1) statusText += ' [Input Fault]';
        updateElement('chargerStatusText', statusText);
        updateElement('chargingText', statusText);

        // Time remaining (if available)
        if (c.time_remaining_min !== undefined) {
            updateElement('chargerTimeRemaining', c.time_remaining_min);
        } else {
            updateElement('chargerTimeRemaining', '--');
        }
    } else {
        // Charger disconnected
        window.previousChargerState = false;

        if (chargingStatusDiv) {
            chargingStatusDiv.classList.remove('active');
        }
        updateElement('chargingText', 'Charger Disconnected');
        updateElement('chargingPower', '--');
        updateElement('chargerVoltage', '--');
        updateElement('chargerCurrent', '--');
        updateElement('chargerStatusText', 'Disconnected');
        updateElement('chargerTimeRemaining', '--');
    }

    // Update status indicators
    if (data.status_indicators) {
        updateStatusIndicator('battery-status', data.status_indicators.battery);
        updateStatusIndicator('motor-status', data.status_indicators.motor);
        updateStatusIndicator('gps-status', data.status_indicators.gps);
        updateStatusIndicator('charger-status', data.status_indicators.charger);
    }
}

function updateElement(id, value) {
    const el = document.getElementById(id);
    if (el) {
        el.textContent = value;
    }
}

function updateStatusIndicator(elementId, status) {
    const element = document.getElementById(elementId);
    if (element) {
        element.classList.remove('online', 'offline', 'unknown');
        element.classList.add(status || 'unknown');
    }
}

function updateTemperatureColor(elementId, temperature, normalMax, warmMax, hotMax) {
    /**
     * Color code temperature based on thresholds
     * elementId: DOM element ID of the temp item
     * temperature: Current temperature value
     * normalMax: Max temp for normal (green)
     * warmMax: Max temp for warm (yellow/orange)
     * hotMax: Max temp for hot (red)
     */
    const element = document.getElementById(elementId);
    if (!element) return;

    element.classList.remove('temp-normal', 'temp-warm', 'temp-hot');

    if (temperature >= hotMax) {
        element.classList.add('temp-hot');
    } else if (temperature >= warmMax) {
        element.classList.add('temp-warm');
    } else {
        element.classList.add('temp-normal');
    }
}

function updateBatteryCells(cellVoltages) {
    /**
     * Update battery cell chart with individual cell voltages
     * cellVoltages: Array of cell voltage values in millivolts
     */
    if (!cellVoltages || cellVoltages.length === 0) {
        return;
    }

    // Calculate statistics
    const validCells = cellVoltages.filter(v => v > 0);
    if (validCells.length === 0) return;

    const maxVoltage = Math.max(...validCells);
    const minVoltage = Math.min(...validCells);
    const avgVoltage = validCells.reduce((a, b) => a + b, 0) / validCells.length;
    const maxIndex = cellVoltages.indexOf(maxVoltage);
    const minIndex = cellVoltages.indexOf(minVoltage);

    // Update header stats
    updateElement('highestCell', maxVoltage.toFixed(0));
    updateElement('lowestCell', minVoltage.toFixed(0));
    updateElement('avgCell', avgVoltage.toFixed(0));
    updateElement('highestCellNum', `#${maxIndex + 1}`);
    updateElement('lowestCellNum', `#${minIndex + 1}`);

    // Update cell grid
    const cellGrid = document.getElementById('cellGrid');
    if (!cellGrid) return;

    // Only rebuild grid if number of cells changed
    if (cellGrid.children.length !== validCells.length) {
        cellGrid.innerHTML = '';

        cellVoltages.forEach((voltage, index) => {
            if (voltage <= 0) return; // Skip invalid cells

            const cellItem = document.createElement('div');
            cellItem.className = 'cell-item';
            cellItem.id = `cell-${index}`;

            // Highlight highest and lowest
            if (voltage === maxVoltage) {
                cellItem.classList.add('highest');
            } else if (voltage === minVoltage) {
                cellItem.classList.add('lowest');
            }

            cellItem.innerHTML = `
                <div class="cell-number">Cell ${index + 1}</div>
                <div class="cell-voltage">${voltage.toFixed(0)}</div>
            `;

            cellGrid.appendChild(cellItem);
        });
    } else {
        // Just update existing cells
        cellVoltages.forEach((voltage, index) => {
            if (voltage <= 0) return;

            const cellItem = document.getElementById(`cell-${index}`);
            if (!cellItem) return;

            // Update classes
            cellItem.classList.remove('highest', 'lowest');
            if (voltage === maxVoltage) {
                cellItem.classList.add('highest');
            } else if (voltage === minVoltage) {
                cellItem.classList.add('lowest');
            }

            // Update voltage display
            const voltageEl = cellItem.querySelector('.cell-voltage');
            if (voltageEl) {
                voltageEl.textContent = voltage.toFixed(0);
            }
        });
    }
}

// ============================================================================
// GPS Map
// ============================================================================

function initGPSMap() {
    // Default to Nairobi, Kenya
    const defaultLat = -1.4149;
    const defaultLon = 35.1244;

    map = L.map('gps-map', {
        zoomControl: false,  // Remove zoom controls for cleaner look
        attributionControl: false
    }).setView([defaultLat, defaultLon], 15);

    // Add OpenStreetMap tiles
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19
    }).addTo(map);

    // Create vehicle marker
    marker = L.marker([defaultLat, defaultLon], {
        icon: L.divIcon({
            className: 'vehicle-marker',
            html: '<div style="font-size: 30px;">🚗</div>',
            iconSize: [30, 30]
        })
    }).addTo(map);

    marker.bindPopup('Waiting for GPS fix...').openPopup();
}

function updateMapPosition(lat, lon, heading) {
    if (!map || !marker) return;

    // Update marker position
    marker.setLatLng([lat, lon]);

    // Update popup
    const popupText = `
        <b>Vehicle Position</b><br>
        Lat: ${lat.toFixed(6)}<br>
        Lon: ${lon.toFixed(6)}<br>
        Heading: ${heading}°
    `;
    marker.bindPopup(popupText);

    // Center map on new position (only if on GPS page)
    if (currentPage === 'gps') {
        map.setView([lat, lon], map.getZoom());
    }
}

// ============================================================================
// Auto-refresh
// ============================================================================

// Refresh data every 2 seconds as fallback (WebSocket is primary)
setInterval(() => {
    if (!socket || !socket.connected) {
        fetchInitialData();
    }
}, 2000);

// Page auto-rotation (optional - can be enabled by user)
let autoRotateEnabled = false;
let autoRotateInterval = null;

function enableAutoRotate() {
    if (autoRotateInterval) return;

    const pages = ['overview', 'battery', 'motor', 'gps', 'charger'];
    let currentIndex = 0;

    autoRotateInterval = setInterval(() => {
        currentIndex = (currentIndex + 1) % pages.length;
        switchPage(pages[currentIndex]);
    }, 10000); // Switch every 10 seconds
}

function disableAutoRotate() {
    if (autoRotateInterval) {
        clearInterval(autoRotateInterval);
        autoRotateInterval = null;
    }
}

console.log('Kiosk mode initialized');
console.log('Auto-rotate disabled by default. Call enableAutoRotate() to enable.');
