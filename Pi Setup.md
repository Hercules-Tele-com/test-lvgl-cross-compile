Perfect — here’s your **complete README.md** as one continuous Markdown file (no breakouts, just a single cohesive document). You can copy-paste this directly into your repo or save it as `README.md` for your Raspberry Pi CAN setup.

---

````markdown
# Raspberry Pi 4 – Waveshare 2-CH CAN HAT Plus Setup (MCP2515 ×2 on SPI1)

This guide explains how to install Raspberry Pi OS 64-bit and configure the Waveshare 2-CH CAN HAT Plus so both `can0` and `can1` interfaces start automatically at boot. It covers everything from flashing the OS to verifying communication and creating persistent systemd services.

---

## Installation and Configuration Steps

### Step 1: Install Raspberry Pi OS 64-bit (Lite recommended)
1. Flash **Raspberry Pi OS (64-bit) Lite** using Raspberry Pi Imager.
2. In “Advanced Options” enable:
   - Hostname (e.g. `raspberry`)
   - SSH
   - Wi-Fi credentials
3. Boot the Pi 4 and update:
   ```bash
   sudo apt update && sudo apt full-upgrade -y
````

---

### Step 2: Attach the Waveshare 2-CH CAN HAT Plus

Plug the HAT into the 40-pin header. Confirm both controller ICs are **MCP2515** (each with a 16 MHz crystal). The board uses **SPI1** by default with the following pins:

| Function    | GPIO | Description    |
| ----------- | ---- | -------------- |
| MISO        | 19   | SPI1 MISO      |
| MOSI        | 20   | SPI1 MOSI      |
| SCK         | 21   | SPI1 SCLK      |
| CS₀ (CAN0)  | 17   | SPI1 CE1       |
| INT₀ (CAN0) | 22   | Interrupt      |
| CS₁ (CAN1)  | 16   | SPI1 CE2       |
| INT₁ (CAN1) | 13   | Interrupt      |
| 5 V / GND   | —    | Power & Ground |

---

### Step 3: Enable SPI1 and Configure Overlays

Edit the boot configuration file:

```bash
sudo nano /boot/firmware/config.txt
```

Append the following lines:

```ini
# === Waveshare 2-CH CAN HAT Plus (MCP2515 ×2 on SPI1) ===
dtparam=spi=on
dtoverlay=spi1-3cs
dtoverlay=mcp2515,spi1-1,oscillator=16000000,interrupt=22
dtoverlay=mcp2515,spi1-2,oscillator=16000000,interrupt=13
```

Save and reboot:

```bash
sudo reboot
```

---

### Step 4: Verify Hardware Detection

After reboot, check driver messages:

```bash
dmesg | grep -i mcp
```

Expected output:

```
mcp251x spi1.2 can0: MCP2515 successfully initialized.
mcp251x spi1.1 can1: MCP2515 successfully initialized.
```

Confirm network interfaces exist:

```bash
ip link show
```

You should see `can0` and `can1` listed (in DOWN state).

---

### Step 5: Install CAN Utilities and Test Interfaces

Install required tools:

```bash
sudo apt install -y can-utils python3-pip
```

Bring interfaces up manually (classic CAN at 500 kbps):

```bash
sudo ip link set can0 up type can bitrate 500000 sample-point 0.8
sudo ip link set can1 up type can bitrate 500000 sample-point 0.8
```

Check status:

```bash
ip -details link show can0
ip -details link show can1
```

To view bus traffic:

```bash
candump -L can0
```

Use `Ctrl +C` to stop.

---

### Step 6: Create Auto-Start Script and Service

#### 6.1 Create bring-up script

```bash
sudo tee /usr/local/sbin/setup-can.sh >/dev/null <<'SH'
#!/bin/bash
set -e
ip link set can0 down 2>/dev/null || true
ip link set can1 down 2>/dev/null || true
ip link set can0 up type can bitrate 500000 sample-point 0.8
ip link set can1 up type can bitrate 500000 sample-point 0.8
SH
sudo chmod +x /usr/local/sbin/setup-can.sh
```

#### 6.2 Create systemd service

```bash
sudo tee /etc/systemd/system/can-setup.service >/dev/null <<'UNIT'
[Unit]
Description=Setup CAN interfaces (MCP2515 on SPI1)
DefaultDependencies=no
After=multi-user.target network-pre.target local-fs.target dev-spidev1.0.device
Before=network-online.target

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/setup-can.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
UNIT
```

Enable and start it:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now can-setup.service
systemctl status can-setup.service
```

It should show **active (exited)**.

---

### Step 7: Optional – Python CAN Logger

Install dependency:

```bash
pip3 install --break-system-packages python-can
```

Create `~/log_can.py`:

```python
#!/usr/bin/env python3
import csv, os, sys, datetime, can
IFACE = sys.argv[1] if len(sys.argv) > 1 else "can0"
OUTDIR = os.path.expanduser("~/can_logs")
os.makedirs(OUTDIR, exist_ok=True)
fname = os.path.join(OUTDIR, f"{IFACE}-{datetime.datetime.now():%Y%m%d-%H%M%S}.csv")
with open(fname, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["utc", "id", "dlc", "data"])
    bus = can.ThreadSafeBus(interface="socketcan", channel=IFACE)
    for msg in bus:
        w.writerow([
            datetime.datetime.utcnow().isoformat()+"Z",
            f"{msg.arbitration_id:03X}",
            msg.dlc,
            msg.data.hex().upper()
        ])
        f.flush()
```

Make it executable:

```bash
chmod +x ~/log_can.py
```

#### 7.1 Systemd logger template

```bash
sudo tee /etc/systemd/system/can-logger@.service >/dev/null <<'UNIT'
[Unit]
Description=CAN CSV logger on %i
After=can-setup.service
Requires=can-setup.service

[Service]
Type=simple
User=emboo
Environment=PYTHONUNBUFFERED=1
ExecStart=/usr/bin/python3 /home/emboo/log_can.py %i
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
UNIT
```

Enable logging for `can0` (and optionally `can1`):

```bash
sudo systemctl enable --now can-logger@can0.service
# sudo systemctl enable --now can-logger@can1.service
```

---

### Step 8: Reboot and Verify Auto-Start

Reboot:

```bash
sudo reboot
```

After logging back in:

```bash
ip -details link show can0
systemctl status can-setup.service
```

Both CAN interfaces should be **UP**, and log files appear in `~/can_logs` if the logger is enabled.

---

### Step 9: Loopback Test (optional)

Connect CAN0 ↔ CAN1 (H to H, L to L) with proper 120 Ω termination at each end.

**Terminal 1:**

```bash
candump -L can1
```

**Terminal 2:**

```bash
cangen can0 -I 123 -L 8 -g 5
```

You should see frames appearing on `can1`.

---

### Step 10: Next Steps

* Build and deploy your LVGL dashboard (`test-lvgl-cross-compile`) to visualize CAN data.
* Connect and configure the Victron 7″ DSI screen.
* Optionally add InfluxDB, Node-RED, or Grafana for logging and monitoring.

---

### References

* [Waveshare 2-CH CAN HAT Plus](https://www.waveshare.com/2-ch-can-hat-plus.htm)
* [linux-can / can-utils](https://github.com/linux-can/can-utils)
* [python-can Documentation](https://python-can.readthedocs.io/en/stable/)

---

**Tested on:** Raspberry Pi 4 Model B (8 GB)
**OS:** Raspberry Pi OS 64-bit (Bookworm/Trixie)
**Kernel:** 6.12.47
**Result:** `can0` and `can1` initialize automatically and log successfully.

```
