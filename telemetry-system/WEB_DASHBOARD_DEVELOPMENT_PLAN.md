# Web Dashboard Development Plan

## Overview
Comprehensive plan to enhance the Nissan Leaf telemetry web dashboard with Victron-style monitoring, advanced analytics, and improved user experience.

## Current State Analysis

### ✅ What's Working
- **Backend (Flask + SocketIO)**
  - REST API endpoints for current status and historical data
  - WebSocket real-time updates (2-second interval)
  - InfluxDB integration with query functions
  - Basic systemd service setup

- **Frontend (HTML/CSS/JS)**
  - Real-time dashboard with speed gauge, battery display
  - Historical charts (Chart.js)
  - Tab navigation (Dashboard, History, Details)
  - WebSocket connectivity indicator
  - Responsive design with dark theme

- **Data Sources**
  - Existing support for: inverter, battery SOC, temperature, speed, motor RPM, charger, GPS

### ⚠️ Gaps Identified
1. **No Victron BMS protocol support** (0x351, 0x355, 0x356, 0x35F, 0x370-0x373)
2. **Limited battery visualization** (no cell-level monitoring)
3. **No energy flow diagram** (can't see power flow direction)
4. **No trip statistics or efficiency tracking**
5. **No alerting system** for critical conditions
6. **No GPS map visualization** (only numeric lat/lon)
7. **No data export** functionality
8. **Mock data only** - needs real CAN integration testing

---

## Development Phases

## Phase 1: Victron BMS Integration (Priority: HIGH)
**Goal:** Add support for Victron BMS protocol data and display battery health metrics

### Backend Tasks
- [ ] **1.1** Update `telemetry_logger.py` to parse Victron CAN messages
  - Add parsers for 0x351 (charge/discharge limits)
  - Add parsers for 0x355 (SOC/SOH)
  - Add parsers for 0x356 (voltage/current/temp)
  - Add parsers for 0x35F (battery characteristics)
  - Add parsers for 0x370-0x373 (cell extrema)

- [ ] **1.2** Create new InfluxDB measurements for Victron data
  - `victron_limits` (charge/discharge voltage/current limits)
  - `victron_soc` (SOC/SOH in %)
  - `victron_pack` (pack voltage, current, temperature)
  - `victron_characteristics` (cell count, chemistry, capacity, firmware)
  - `victron_cells` (min/max cell voltages and temps per module)

- [ ] **1.3** Add REST API endpoints in `app.py`
  - `GET /api/victron/status` - Current Victron BMS status
  - `GET /api/victron/cells` - Cell voltage/temp extrema
  - `GET /api/victron/history/<metric>` - Historical trends

### Frontend Tasks
- [ ] **1.4** Create Victron BMS display panel
  - Battery pack diagram with voltage, current, SOC, SOH
  - Charge/discharge limit indicators
  - Cell count, chemistry type, capacity display
  - Firmware version indicator

- [ ] **1.5** Add cell voltage monitoring widget
  - Display min/max cell voltages with delta
  - Color coding: green (balanced), yellow (imbalance >50mV), red (>100mV)
  - Show which module (0x370-0x373) has extrema

- [ ] **1.6** Add temperature extrema display
  - Min/max cell temperatures across all modules
  - Visual warning if any cell >45°C or <0°C

**Estimated Time:** 4-6 hours
**Dependencies:** None
**Testing:** Use real Victron BMS hardware on Raspberry Pi

---

## Phase 2: Energy Flow Visualization (Priority: HIGH)
**Goal:** Create intuitive power flow diagram showing energy movement

### Backend Tasks
- [ ] **2.1** Add power calculation endpoint
  - Calculate instantaneous power (V × I)
  - Determine flow direction (charging, discharging, idle)
  - Track accumulated energy (kWh in/out)

### Frontend Tasks
- [ ] **2.2** Design SVG energy flow diagram
  - Components: Battery → Inverter → Motor
  - Components: Grid → Charger → Battery
  - Animated arrows showing power flow direction
  - Power values displayed on arrows

- [ ] **2.3** Add energy statistics panel
  - Energy consumed today (kWh)
  - Energy regenerated today (kWh)
  - Net energy (kWh)
  - Efficiency percentage (regen / consumed)

**Estimated Time:** 3-4 hours
**Dependencies:** Phase 1 (need accurate voltage/current data)
**Testing:** Drive vehicle and verify power flow matches reality

---

## Phase 3: Trip Statistics & Analytics (Priority: MEDIUM)
**Goal:** Track driving sessions and provide efficiency metrics

### Backend Tasks
- [ ] **3.1** Implement trip detection logic
  - Start trip: vehicle speed > 0 and key ON
  - End trip: vehicle stopped for >5 minutes
  - Store trip metadata in InfluxDB

- [ ] **3.2** Calculate trip metrics
  - Distance traveled (km) - integrate GPS speed
  - Energy consumed (kWh)
  - Average speed (km/h)
  - Efficiency (Wh/km)
  - Duration (minutes)

- [ ] **3.3** Add trip history API endpoints
  - `GET /api/trips/current` - Ongoing trip stats
  - `GET /api/trips/recent` - Last 10 trips
  - `GET /api/trips/<trip_id>` - Specific trip details

### Frontend Tasks
- [ ] **3.4** Create trip statistics dashboard
  - Current trip: distance, duration, energy, efficiency
  - Trip history table with sortable columns
  - Best/worst efficiency trips highlighted

- [ ] **3.5** Add efficiency charts
  - Wh/km over time
  - Speed vs efficiency scatter plot
  - Temperature impact on efficiency

**Estimated Time:** 5-7 hours
**Dependencies:** GPS speed data, accurate power measurements
**Testing:** Record multiple test drives and verify calculations

---

## Phase 4: Alert & Notification System (Priority: MEDIUM)
**Goal:** Warn users of critical vehicle conditions in real-time

### Backend Tasks
- [ ] **4.1** Define alert rules
  - Battery: SOC < 10%, temp > 45°C, voltage < 300V
  - Charging: Charge current > limit, voltage > max
  - Motor: temp > 100°C, RPM > redline
  - System: 12V < 11.5V, CAN errors

- [ ] **4.2** Implement alert engine
  - Continuously monitor incoming data
  - Trigger alerts when thresholds exceeded
  - Debounce to prevent alert spam (5-second delay)

- [ ] **4.3** Add alert API and WebSocket events
  - `GET /api/alerts/active` - Current active alerts
  - `GET /api/alerts/history` - Past alerts (last 24h)
  - WebSocket event: `alert_triggered`, `alert_cleared`

### Frontend Tasks
- [ ] **4.4** Create alert notification UI
  - Toast notifications for new alerts
  - Alert banner at top of dashboard
  - Alert history panel with timestamps

- [ ] **4.5** Add alert configuration page
  - User-adjustable thresholds
  - Enable/disable specific alerts
  - Alert sound preferences

**Estimated Time:** 4-5 hours
**Dependencies:** Reliable data from all sensors
**Testing:** Simulate critical conditions (low SOC, high temp)

---

## Phase 5: GPS Map Integration (Priority: LOW)
**Goal:** Visualize vehicle location and route history on a map

### Backend Tasks
- [ ] **5.1** Store GPS track points in InfluxDB
  - Timestamp, latitude, longitude, heading, speed
  - Downsample to 1 point per 5 seconds to save space

- [ ] **5.2** Add GPS data API endpoints
  - `GET /api/gps/current` - Latest position
  - `GET /api/gps/track?start=<time>&end=<time>` - Route history

### Frontend Tasks
- [ ] **5.3** Integrate Leaflet.js or Mapbox GL
  - Display current vehicle position with heading arrow
  - Show route track as colored polyline
  - Color code by speed (green=slow, red=fast)

- [ ] **5.4** Add map controls
  - Toggle layers: satellite, street, terrain
  - Show charging stations nearby (if API available)
  - Zoom to fit full route

**Estimated Time:** 6-8 hours
**Dependencies:** GPS data from 0x710/0x711 CAN messages
**Testing:** Drive around block and verify route accuracy

---

## Phase 6: Data Export & Reporting (Priority: LOW)
**Goal:** Allow users to download telemetry data for external analysis

### Backend Tasks
- [ ] **6.1** Add export API endpoints
  - `GET /api/export/csv?start=<time>&end=<time>&metrics=<list>` - CSV format
  - `GET /api/export/json?start=<time>&end=<time>` - JSON format
  - `GET /api/export/trip/<trip_id>` - Single trip export

- [ ] **6.2** Generate summary reports
  - Weekly driving report (PDF)
  - Battery health report (PDF)
  - Include charts and statistics

### Frontend Tasks
- [ ] **6.3** Add export UI
  - Date range picker
  - Metric selector (checkboxes)
  - Download button (triggers API call)

- [ ] **6.4** Create report viewer
  - Preview reports before download
  - Email report option (if SMTP configured)

**Estimated Time:** 4-5 hours
**Dependencies:** All data collection working
**Testing:** Export various date ranges and verify data integrity

---

## Phase 7: Advanced Features (Priority: NICE-TO-HAVE)
**Goal:** Polish and add convenience features

### Additional Ideas
- [ ] **7.1** Mobile responsive optimization
  - Test on phones/tablets
  - Touch-friendly controls
  - Reduce data for mobile networks

- [ ] **7.2** Dark/light theme toggle
  - User preference saved in localStorage
  - CSS variables for easy theming

- [ ] **7.3** User settings page
  - Units (km vs miles, °C vs °F)
  - Refresh rate (1s, 2s, 5s)
  - Data retention period

- [ ] **7.4** Comparison mode
  - Compare two trips side-by-side
  - Compare today vs yesterday stats

- [ ] **7.5** Voice alerts (Web Speech API)
  - Announce critical alerts
  - Enable/disable in settings

**Estimated Time:** 8-10 hours
**Dependencies:** All core features complete
**Testing:** User acceptance testing with actual drivers

---

## Testing Strategy

### Unit Testing
- Backend: pytest for API endpoints
- Frontend: Jest for JavaScript functions

### Integration Testing
- End-to-end WebSocket data flow
- InfluxDB query correctness
- Chart rendering performance

### Hardware Testing
- Deploy to Raspberry Pi
- Connect to real CAN bus
- Drive vehicle and verify accuracy

### Performance Testing
- Load testing: 10+ simultaneous clients
- InfluxDB query optimization
- WebSocket message rate tuning

---

## Deployment Checklist

### Prerequisites
- InfluxDB 2.x installed and configured
- Python 3.8+ with dependencies installed
- Systemd services configured
- Firewall rules (port 8080 open)

### Installation Steps
```bash
# Install dependencies
pip3 install -r requirements.txt

# Setup InfluxDB
./scripts/setup-influxdb.sh

# Configure environment
cp config/influxdb-local.env.template config/influxdb-local.env
nano config/influxdb-local.env

# Install services
sudo cp systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now telemetry-logger cloud-sync web-dashboard

# Verify
systemctl status web-dashboard.service
curl http://localhost:8080/api/status
```

### Monitoring
```bash
# View logs
sudo journalctl -u web-dashboard.service -f

# Check InfluxDB stats
influx query 'from(bucket:"leaf-data") |> range(start: -1h) |> count()'

# Test WebSocket
# Open browser dev tools, check WS connection in Network tab
```

---

## Success Metrics

### Technical KPIs
- Dashboard load time: < 2 seconds
- WebSocket latency: < 100ms
- InfluxDB query time: < 500ms
- Uptime: > 99% (systemd auto-restart)

### User Experience
- All Victron BMS metrics visible and accurate
- Historical charts load within 3 seconds
- Alerts trigger within 2 seconds of condition
- GPS route displays accurately (< 10m error)

### Data Quality
- No data loss (all CAN messages logged)
- 30-day local retention maintained
- Cloud sync success rate > 95%

---

## Priority Summary

### Must-Have (v1.0)
1. ✅ Victron BMS integration (Phase 1)
2. ✅ Energy flow visualization (Phase 2)
3. ✅ Real-time dashboard improvements

### Should-Have (v1.1)
4. Trip statistics (Phase 3)
5. Alert system (Phase 4)

### Nice-to-Have (v2.0)
6. GPS map (Phase 5)
7. Data export (Phase 6)
8. Advanced features (Phase 7)

---

## Current Status: Phase 1 Ready to Begin

**Next Action:** Update `telemetry_logger.py` to add Victron BMS message parsers (Task 1.1)
