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
    const navButtons = document.querySelectorAll('.nav-button');

    navButtons.forEach(button => {
        button.addEventListener('click', () => {
            const targetPage = button.getAttribute('data-page');
            switchPage(targetPage);
        });
    });
}

function switchPage(pageName) {
    // Hide all pages
    const pages = document.querySelectorAll('.page');
    pages.forEach(page => page.classList.remove('active'));

    // Remove active from all nav buttons
    const navButtons = document.querySelectorAll('.nav-button');
    navButtons.forEach(btn => btn.classList.remove('active'));

    // Show target page
    const targetPage = document.getElementById(`page-${pageName}`);
    if (targetPage) {
        targetPage.classList.add('active');
    }

    // Activate nav button
    const targetButton = document.querySelector(`[data-page="${pageName}"]`);
    if (targetButton) {
        targetButton.classList.add('active');
    }

    currentPage = pageName;

    // Refresh map if switching to GPS page
    if (pageName === 'gps' && map) {
        setTimeout(() => {
            map.invalidateSize();
        }, 100);
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

function updateDashboard(data) {
    // Battery data (EMBOO/Orion BMS - Schema V2)
    if (data.battery) {
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

        // Temperature
        const temp = b.temp_avg || b.temp_high || b.temperature || 0;
        updateElement('batteryTemp', Math.round(temp));
        updateElement('temp', Math.round(temp));

        // Cell voltages
        if (b.cell_voltage_min !== undefined && b.cell_voltage_max !== undefined) {
            const minV = Math.round((b.cell_voltage_min || 0) * 1000);
            const maxV = Math.round((b.cell_voltage_max || 0) * 1000);
            const delta = maxV - minV;
            updateElement('cellRange', `${minV} - ${maxV}`);
            updateElement('cellDelta', delta);
        }
    }

    // Motor data
    if (data.motor) {
        const m = data.motor;
        updateElement('rpm', m.rpm || 0);
        updateElement('torque', m.torque_actual || 0);
        updateElement('motorTemp', m.temp_stator ? Math.round(m.temp_stator) : '--');
        updateElement('dcBus', m.voltage_dc_bus || '--');

        // Direction
        let direction = '--';
        if (m.direction === 1) direction = 'Forward';
        else if (m.direction === 2) direction = 'Reverse';
        else if (m.direction === 0) direction = 'Neutral';
        updateElement('direction', direction);
    }

    // Inverter data
    if (data.inverter) {
        updateElement('inverterTemp', Math.round(data.inverter.temp_inverter || 0));
        if (!data.motor || !data.motor.temp_stator) {
            updateElement('motorTemp', Math.round(data.inverter.temp_motor || 0));
        }
    }

    // GPS data
    if (data.gps) {
        const g = data.gps;

        // Speed
        if (g.speed_kmh !== undefined) {
            updateElement('speed', g.speed_kmh.toFixed(1));
            updateElement('gpsSpeed', g.speed_kmh.toFixed(1));
        }

        // Altitude
        if (g.altitude !== undefined) {
            updateElement('altitude', Math.round(g.altitude));
        }

        // Satellites
        if (g.satellites !== undefined) {
            updateElement('satellites', g.satellites);
        }

        // Heading
        if (g.heading !== undefined) {
            updateElement('heading', Math.round(g.heading));
        }

        // Update map
        if (g.latitude !== undefined && g.longitude !== undefined) {
            updateMapPosition(g.latitude, g.longitude, g.heading || 0);
        }
    } else {
        updateElement('speed', '--');
        updateElement('gpsSpeed', '--');
    }

    // Charger data
    const chargingStatusDiv = document.getElementById('chargingStatus');
    if (data.charger && data.status_indicators && data.status_indicators.charger === 'online') {
        const c = data.charger;
        const isCharging = c.charging_state === 1;

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
        // Charger offline
        if (chargingStatusDiv) {
            chargingStatusDiv.classList.remove('active');
        }
        updateElement('chargingText', 'Charger Offline');
        updateElement('chargingPower', '--');
        updateElement('chargerVoltage', '--');
        updateElement('chargerCurrent', '--');
        updateElement('chargerStatusText', 'Offline');
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
