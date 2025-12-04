# Web Kiosk vs LVGL Dashboard Proposal

**Date:** December 4, 2025
**Project:** Nissan Leaf CAN Network Telemetry System
**Status:** Proposal for Architecture Decision

---

## Executive Summary

This proposal evaluates transitioning from the current **LVGL-based dashboard** (native C++ framebuffer application) to a **web-based kiosk** solution using Chromium in fullscreen mode on the Raspberry Pi.

**Recommendation:** **Adopt web-based kiosk approach** for superior development velocity, cross-platform compatibility, and maintainability, with minimal performance trade-offs for this use case.

---

## Current Architecture: LVGL Dashboard

### Technology Stack
- **UI Framework:** LVGL 8.3.11 (Light and Versatile Graphics Library)
- **Language:** C++17
- **Display:** Direct framebuffer rendering (`/dev/fb0`)
- **Input:** SocketCAN for real-time CAN data
- **Binary Size:** 12MB compiled executable
- **Platform:** Linux/Raspberry Pi 4

### Current Implementation
```
┌─────────────────────────────────────────┐
│   LVGL Dashboard (C++ Native App)       │
│                                          │
│   ┌──────────────────────────────────┐  │
│   │  LVGL UI (Framebuffer Direct)    │  │
│   └──────────────────────────────────┘  │
│                ↕                         │
│   ┌──────────────────────────────────┐  │
│   │  SocketCAN Interface (can0/can1) │  │
│   └──────────────────────────────────┘  │
│                ↕                         │
│   ┌──────────────────────────────────┐  │
│   │  CAN Message Parsers             │  │
│   │  (EMBOO Battery, ROAM Motor)     │  │
│   └──────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

---

## Proposed Architecture: Web Kiosk

### Technology Stack
- **UI Framework:** HTML5 + CSS3 + JavaScript (already implemented!)
- **Browser:** Chromium in fullscreen kiosk mode
- **Backend:** Flask web server (Python, already implemented!)
- **Data Source:** InfluxDB + WebSocket real-time updates
- **Display:** Browser rendering (GPU-accelerated)
- **Auto-start:** systemd service with Chromium kiosk flags

### Proposed Implementation
```
┌─────────────────────────────────────────────────────────┐
│   Chromium Kiosk (Fullscreen Browser)                   │
│                                                           │
│   http://localhost:8080                                  │
│   ┌───────────────────────────────────────────────────┐ │
│   │  Web Dashboard (HTML/CSS/JS + Leaflet Map)        │ │
│   │  - Battery metrics, Motor data, GPS map           │ │
│   │  - Real-time WebSocket updates                    │ │
│   │  - Responsive grid layout                         │ │
│   └───────────────────────────────────────────────────┘ │
│                        ↕ (WebSocket + REST API)          │
│   ┌───────────────────────────────────────────────────┐ │
│   │  Flask Web Server (Port 8080)                     │ │
│   │  - /api/status endpoint                           │ │
│   │  - WebSocket real-time push                       │ │
│   │  - Query InfluxDB for metrics                     │ │
│   └───────────────────────────────────────────────────┘ │
│                        ↕                                  │
│   ┌───────────────────────────────────────────────────┐ │
│   │  InfluxDB (Time-Series Database)                  │ │
│   │  - Battery, Motor, GPS, Inverter measurements     │ │
│   └───────────────────────────────────────────────────┘ │
│                        ↕                                  │
│   ┌───────────────────────────────────────────────────┐ │
│   │  Telemetry Logger (Python)                        │ │
│   │  - Reads CAN (can0/can1)                          │ │
│   │  - Parses EMBOO Battery + ROAM Motor messages     │ │
│   │  - Writes to InfluxDB                             │ │
│   └───────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

---

## Detailed Comparison

### 1. Development & Maintenance

| Aspect | LVGL (Current) | Web Kiosk (Proposed) |
|--------|----------------|----------------------|
| **UI Language** | C++ with LVGL API | HTML/CSS/JavaScript |
| **Development Speed** | ⚠️ Slow (compile cycle, C++ complexity) | ✅ **Fast** (edit, refresh, see changes) |
| **Designer Tools** | SquareLine Studio (commercial, $499/year) | ✅ **Free** (any web IDE, browser DevTools) |
| **Learning Curve** | ⚠️ Steep (C++, LVGL API, framebuffer) | ✅ **Shallow** (standard web technologies) |
| **UI Iteration** | ⚠️ Slow: Edit C++ → Compile → Deploy → Test | ✅ **Instant**: Edit HTML → F5 → See result |
| **Cross-Platform Testing** | ⚠️ Limited (SDL2 simulator for Windows) | ✅ **Universal** (test on any device with browser) |
| **Library Ecosystem** | Limited (LVGL widgets only) | ✅ **Massive** (npm, CDNs, millions of packages) |
| **Code Maintainability** | ⚠️ Complex (manual memory management, pointers) | ✅ **Simple** (garbage collected, high-level) |
| **Team Onboarding** | ⚠️ Hard (requires embedded C++ experience) | ✅ **Easy** (web developers are abundant) |

**Winner:** 🏆 **Web Kiosk** - Dramatically faster development and easier maintenance

---

### 2. Performance

| Metric | LVGL (Current) | Web Kiosk (Proposed) |
|--------|----------------|----------------------|
| **Frame Rate** | 60 FPS (direct framebuffer) | 50-60 FPS (Chromium GPU acceleration) |
| **CPU Usage (Idle)** | ~3-5% | ~8-12% (browser overhead) |
| **Memory Usage** | ~30 MB (native app) | ~150-200 MB (Chromium + web app) |
| **Boot Time** | ✅ **2-3 seconds** | ⚠️ 5-8 seconds (Chromium startup) |
| **Binary Size** | 12 MB | N/A (browser is pre-installed) |
| **GPU Acceleration** | ⚠️ Software rendering (unless configured) | ✅ **Hardware accelerated** (built-in) |
| **Rendering Quality** | Good (framebuffer direct) | ✅ **Excellent** (anti-aliasing, smooth fonts) |
| **Map Rendering** | ⚠️ Requires custom implementation | ✅ **Native** (Leaflet.js, WebGL) |
| **Animations** | Manual implementation | ✅ **CSS3/WebGL** (smooth, GPU-accelerated) |

**Winner:** ⚖️ **Slight edge to LVGL** for raw performance, but difference is negligible for dashboard use case

**Performance Impact:** On Raspberry Pi 4 (1.5GHz quad-core, 4GB RAM), the 100-150MB extra memory and 5% CPU overhead is **acceptable** given the 4GB RAM and typical dashboard refresh rate (1-2 Hz).

---

### 3. Features & Capabilities

| Feature | LVGL (Current) | Web Kiosk (Proposed) |
|---------|----------------|----------------------|
| **Real-Time Updates** | ✅ Direct CAN read (microseconds latency) | ✅ WebSocket push (milliseconds latency) |
| **Interactive Maps** | ⚠️ Requires custom tile renderer | ✅ **Leaflet.js** (already implemented!) |
| **Charting** | ⚠️ LVGL chart widget (limited) | ✅ **Chart.js, Plotly** (full-featured) |
| **Responsive Design** | ⚠️ Manual layout code | ✅ **CSS Grid/Flexbox** (automatic) |
| **Themes/Styling** | ⚠️ LVGL theme system | ✅ **CSS** (infinite possibilities) |
| **Touch Gestures** | ✅ LVGL input handling | ✅ Browser touch events |
| **Remote Access** | ❌ Only on physical screen | ✅ **Access from phone/laptop** |
| **Multi-Screen** | ❌ Single display | ✅ **Any device** on network |
| **Data Export** | ⚠️ Must implement in C++ | ✅ **Browser download APIs** |
| **3rd Party Widgets** | Limited LVGL ecosystem | ✅ **Massive npm/CDN ecosystem** |
| **Video/Multimedia** | ⚠️ Manual implementation | ✅ **HTML5 video/audio** (native) |

**Winner:** 🏆 **Web Kiosk** - More features, better ecosystem, remote access

---

### 4. Real-Time Requirements

| Requirement | LVGL (Current) | Web Kiosk (Proposed) | Notes |
|-------------|----------------|----------------------|-------|
| **Update Frequency** | Unlimited (microseconds) | ~60 Hz (16ms per frame) | Dashboard doesn't need >60 Hz |
| **CAN Message Latency** | ~1ms (direct read) | ~10-50ms (Python → InfluxDB → WebSocket) | Acceptable for monitoring |
| **Screen Refresh** | 16ms (60 FPS) | 16ms (60 FPS) | Same |
| **User Input Latency** | <1ms | ~16ms (browser event loop) | Negligible for dashboard clicks |
| **Critical Real-Time?** | ❌ No (this is monitoring, not control) | ❌ No | **Neither needs hard real-time** |

**Winner:** ⚖️ **Tie** - Dashboard use case doesn't require hard real-time performance

**Note:** This is a **monitoring dashboard**, not a safety-critical control system. Both solutions provide adequate real-time performance for displaying metrics at human-perceivable rates (1-60 Hz).

---

### 5. Deployment & Operations

| Aspect | LVGL (Current) | Web Kiosk (Proposed) |
|--------|----------------|----------------------|
| **Installation** | Copy binary + systemd service | ✅ **Browser already installed** + systemd service |
| **Updates** | ⚠️ Recompile + redeploy binary | ✅ **Edit files, restart browser** (or live reload!) |
| **Remote Updates** | ⚠️ Requires SSH + binary transfer | ✅ **Git pull, restart** (or push from dev machine) |
| **Debugging** | ⚠️ gdb, logs, serial console | ✅ **Browser DevTools** (inspect, console, network) |
| **Hot Reload** | ❌ Must restart app | ✅ **F5 or live reload** (see changes instantly) |
| **Configuration** | ⚠️ Recompile for config changes | ✅ **Environment variables** or web UI |
| **Multi-User Access** | ❌ Only local display | ✅ **Any device** on LAN (phone, tablet, laptop) |
| **Crash Recovery** | systemd restart | systemd restart (same) |
| **Logs** | journalctl (C++ logs) | journalctl (Python logs) + browser console |

**Winner:** 🏆 **Web Kiosk** - Easier deployment, better debugging, remote access

---

### 6. Cost Analysis

| Cost Factor | LVGL (Current) | Web Kiosk (Proposed) |
|-------------|----------------|----------------------|
| **Development Tools** | SquareLine Studio: $499/year | ✅ **Free** (VS Code, browser DevTools) |
| **Developer Hourly Rate** | ~$100/hr (embedded C++) | ~$60/hr (web developer) | **40% cheaper** |
| **Development Time** | ⚠️ 2-3x slower (C++ compile cycle) | ✅ **Faster** (instant feedback) |
| **Maintenance Cost** | ⚠️ Higher (C++ expertise required) | ✅ **Lower** (abundant web developers) |
| **Hardware Cost** | Same (RPi 4) | Same (RPi 4) | No difference |
| **Licensing** | ✅ LVGL is MIT (free) | ✅ All open-source (free) | Both free |

**Winner:** 🏆 **Web Kiosk** - Lower development cost, no commercial tool licenses

**Annual Savings Estimate:** $5,000-$10,000 per year (eliminated SquareLine license + faster development)

---

### 7. Reliability & Robustness

| Factor | LVGL (Current) | Web Kiosk (Proposed) |
|--------|----------------|----------------------|
| **Crash Frequency** | ⚠️ Segfaults possible (C++ memory bugs) | ✅ **Lower** (Python/JS are memory-safe) |
| **Memory Leaks** | ⚠️ Manual management (possible leaks) | ✅ **Garbage collected** (automatic) |
| **Recovery Time** | systemd restarts in ~2s | systemd restarts in ~5s |
| **Watchdog** | Can implement | Can implement (same) |
| **Error Handling** | ⚠️ Manual (C++ exceptions) | ✅ **Automatic** (try/catch, browser sandbox) |
| **Security** | ✅ Runs as root (direct framebuffer) | ⚠️ Runs in browser sandbox (more layers) |
| **Attack Surface** | ✅ Small (native app) | ⚠️ Larger (browser, network) |

**Winner:** ⚖️ **Tie** - LVGL has fewer layers (more secure), web has better memory safety

---

### 8. Future Extensibility

| Future Need | LVGL (Current) | Web Kiosk (Proposed) |
|-------------|----------------|----------------------|
| **Add New Charts** | ⚠️ Implement in C++ | ✅ **Import Chart.js** (1 line) |
| **Add Camera Feed** | ⚠️ Complex (MJPEG decoder) | ✅ **`<video>` tag** |
| **Add Navigation Routes** | ⚠️ Custom map renderer | ✅ **Leaflet routing plugin** |
| **Add Voice Alerts** | ⚠️ Text-to-speech library | ✅ **Web Speech API** |
| **Add Mobile App** | ❌ Must develop separately | ✅ **Already works** (same URL) |
| **Add Cloud Dashboard** | ❌ Must develop separately | ✅ **Same codebase** (responsive design) |
| **Machine Learning** | ⚠️ C++ TensorFlow (complex) | ✅ **TensorFlow.js** or Python backend |
| **3rd Party Integrations** | ⚠️ Write C++ bindings | ✅ **REST APIs** (standard) |

**Winner:** 🏆 **Web Kiosk** - Far easier to extend with modern features

---

## Implementation Plan

### Phase 1: Kiosk Setup (1-2 hours)
```bash
# 1. Install Chromium (if not already installed)
sudo apt install chromium-browser unclutter

# 2. Create kiosk systemd service
sudo nano /etc/systemd/system/dashboard-kiosk.service
```

**Service File:**
```ini
[Unit]
Description=Leaf Dashboard Kiosk
After=web-dashboard.service graphical.target
Wants=web-dashboard.service graphical.target

[Service]
Type=simple
User=emboo
Environment=DISPLAY=:0
Environment=XAUTHORITY=/home/emboo/.Xauthority
ExecStartPre=/bin/sleep 5
ExecStart=/usr/bin/chromium-browser \
    --kiosk \
    --noerrdialogs \
    --disable-infobars \
    --disable-session-crashed-bubble \
    --disable-features=TranslateUI \
    --check-for-update-interval=31536000 \
    --app=http://localhost:8080
Restart=always
RestartSec=5

[Install]
WantedBy=graphical.target
```

```bash
# 3. Enable and start
sudo systemctl daemon-reload
sudo systemctl enable dashboard-kiosk.service
sudo systemctl start dashboard-kiosk.service

# 4. Hide mouse cursor (optional)
unclutter -idle 0.1 -root &
```

### Phase 2: Web Dashboard Optimization (2-4 hours)
1. Add fullscreen CSS optimizations
2. Remove unnecessary UI elements for kiosk mode
3. Add touch-friendly button sizes
4. Optimize for HDMI display resolution (1920x1080 or 1280x720)
5. Add keyboard shortcuts (F11 fullscreen, R refresh, etc.)

### Phase 3: Testing & Validation (1-2 hours)
1. Verify real-time updates
2. Test automatic reconnection on network drop
3. Test crash recovery (kill browser, ensure restart)
4. Validate performance (CPU, memory, frame rate)

### Phase 4: Decommission LVGL (optional, 1 hour)
1. Keep LVGL dashboard as backup
2. Document how to switch back if needed
3. Archive LVGL code (don't delete)

**Total Implementation Time:** 4-9 hours

---

## Risk Analysis

### Risks of Web Kiosk Approach

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| **Browser crash** | Low | Medium | systemd auto-restart (5s recovery) |
| **Higher memory usage** | High | Low | RPi 4 has 4GB RAM (150MB is 3.75% usage) |
| **Network dependency** | Medium | Low | Web server is localhost (no network needed) |
| **Touch input lag** | Low | Low | Chromium touch events are <16ms latency |
| **Security vulnerabilities** | Medium | Low | Dashboard is LAN-only, no internet exposure |
| **Browser updates breaking UI** | Low | Low | Pin Chromium version, test before updating |

### Risks of Staying with LVGL

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| **Slow feature development** | High | High | Hire specialized embedded C++ developer |
| **SquareLine Studio dependency** | Medium | Medium | Pay $499/year perpetually |
| **Limited contributor pool** | High | Medium | Hard to find LVGL developers |
| **Maintenance complexity** | High | Medium | Extensive C++ expertise required |
| **No remote access** | High | Medium | Develop separate mobile app |

---

## Recommendations

### Immediate Action: **Adopt Web Kiosk** ✅

**Rationale:**
1. **Web dashboard already exists and works well** - minimal effort to deploy as kiosk
2. **GPS map already implemented** - Leaflet.js is far superior to any LVGL map solution
3. **Real-time updates already working** - WebSocket provides <50ms latency (adequate for dashboard)
4. **Development velocity** - 3-10x faster to add features in HTML/JS vs C++
5. **Cost savings** - No SquareLine Studio license, cheaper developers
6. **Future-proof** - Web ecosystem is massive and constantly improving

### Transition Strategy: **Parallel Deployment**

Keep both dashboards running during transition:
- **Physical Display (LVGL):** systemd service on framebuffer (port 0)
- **Web Kiosk (Browser):** systemd service on display :0 (http://localhost:8080)
- **Mobile Access (Browser):** Any device on LAN can access web dashboard

After 30 days of validation, make web kiosk primary and archive LVGL as backup.

### Architecture Decision Record

**Decision:** Migrate to web-based kiosk for primary dashboard interface
**Status:** Proposed
**Date:** 2025-12-04
**Context:** Need faster development, remote access, and modern UI features
**Consequences:** +100MB memory usage, -50% development time, +remote access capability
**Reversibility:** High - LVGL dashboard can be restored in minutes if needed

---

## Conclusion

The web kiosk approach offers **substantial advantages** in development speed, maintainability, feature richness, and cost, with **minimal performance trade-offs** that are negligible for a monitoring dashboard use case.

**Recommendation:** ✅ **Proceed with web kiosk implementation**

The web dashboard is already built, tested, and working. Deploying it in kiosk mode is a 2-hour task that immediately unlocks:
- Remote access from any device
- Instant UI updates (no recompile)
- Modern web features (maps, charts, animations)
- Lower development costs
- Easier maintenance

The 150MB extra memory and 5% CPU overhead are **acceptable costs** for the dramatic improvement in development velocity and user experience.

---

**Next Steps:**
1. Create dashboard-kiosk.service (see Implementation Plan)
2. Test kiosk mode for 1 week
3. Gather user feedback
4. Make final decision on LVGL deprecation

**Questions?** Contact the development team or review this proposal in project meetings.
