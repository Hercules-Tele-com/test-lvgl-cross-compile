# Nissan Leaf Telemetry System

Comprehensive telemetry logging, cloud synchronization, and web monitoring system for the Nissan Leaf CAN network.

## Features

- **Real-time CAN Telemetry Logging**: Captures all vehicle data from CAN bus and stores in local InfluxDB
- **Cloud Synchronization**: Automatically syncs data to InfluxDB Cloud when network is available
- **Web Dashboard**: Beautiful web interface for real-time monitoring and historical data analysis
- **Offline Operation**: Works completely offline with local storage and syncs when connection returns
- **Automatic Startup**: All services run automatically on boot via systemd

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Raspberry Pi 4                       │
│                                                         │
│  ┌──────────┐    ┌─────────────────┐                  │
│  │ CAN HAT  │───▶│ Telemetry Logger│                  │
│  │  (can0)  │    │  (Python)       │                  │
│  └──────────┘    └────────┬────────┘                  │
│                            ▼                            │
│                   ┌────────────────┐                   │
│                   │  InfluxDB 2.x  │                   │
│                   │  (Local Time   │                   │
│                   │   Series DB)   │                   │
│                   └───┬────────┬───┘                   │
│                       │        │                        │
│         ┌─────────────┘        └──────────────┐        │
│         ▼                                      ▼        │
│  ┌──────────────┐                    ┌────────────────┐│
│  │  Cloud Sync  │                    │ Web Dashboard  ││
│  │  (Python)    │                    │ (Flask + WS)   ││
│  └──────┬───────┘                    └────────────────┘│
│         │                                               │
└─────────┼───────────────────────────────────────────────┘
          │
          ▼ (when online)
   ┌──────────────┐
   │ InfluxDB     │
   │ Cloud        │
   └──────────────┘
```

## Installation

### Prerequisites

1. **Raspberry Pi 4** with Raspberry Pi OS (64-bit)
2. **Waveshare 2-CH CAN HAT** properly configured (see `Pi Setup.md`)
3. **CAN interface** `can0` up and running at 500 kbps
4. **Python 3.9+** installed
5. **Internet connection** (for initial setup and cloud sync)

### Step 1: Install System Dependencies

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install required packages
sudo apt install -y python3-pip python3-venv git can-utils influxdb2 influxdb2-cli jq
```

### Step 2: Clone Repository (if not already done)

```bash
cd ~
git clone https://github.com/your-repo/test-lvgl-cross-compile.git
cd test-lvgl-cross-compile/telemetry-system
```

### Step 3: Install Python Dependencies

```bash
# Create virtual environment (optional but recommended)
python3 -m venv venv
source venv/bin/activate

# Install dependencies for all services
pip3 install --break-system-packages \
    python-can \
    influxdb-client \
    Flask \
    Flask-CORS \
    Flask-SocketIO \
    python-socketio \
    eventlet \
    requests

# Or install from requirements files
pip3 install --break-system-packages -r services/telemetry-logger/requirements.txt
pip3 install --break-system-packages -r services/cloud-sync/requirements.txt
pip3 install --break-system-packages -r services/web-dashboard/requirements.txt
```

### Step 4: Setup InfluxDB

```bash
# Run the setup script
chmod +x scripts/setup-influxdb.sh
./scripts/setup-influxdb.sh
```

This script will:
- Install and start InfluxDB 2.x
- Create initial admin user and organization
- Create bucket with 30-day retention
- Generate API tokens for each service
- Save configuration to `config/influxdb-local.env`

Follow the prompts to configure:
- Username (default: `admin`)
- Password (min 8 characters)
- Organization (default: `leaf-telemetry`)
- Bucket name (default: `leaf-data`)
- Retention period (default: 30 days)

### Step 5: Configure Cloud Sync (Optional)

If you have an InfluxDB Cloud account, create the cloud configuration:

```bash
nano config/influxdb-cloud.env
```

Add your cloud credentials:

```bash
INFLUX_CLOUD_URL=https://us-west-2-1.aws.cloud2.influxdata.com
INFLUX_CLOUD_ORG=your-organization-name
INFLUX_CLOUD_BUCKET=leaf-telemetry
INFLUX_CLOUD_TOKEN=your-write-token-here
```

Save and secure the file:

```bash
chmod 600 config/influxdb-cloud.env
```

### Step 6: Install Systemd Services

```bash
# Update username in service files if not using 'emboo'
# Edit the service files and replace 'emboo' with your username
sed -i "s/emboo/$USER/g" systemd/*.service

# Install service files
sudo cp systemd/*.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable services
sudo systemctl enable telemetry-logger.service
sudo systemctl enable cloud-sync.service
sudo systemctl enable web-dashboard.service

# Start services
sudo systemctl start telemetry-logger.service
sudo systemctl start cloud-sync.service
sudo systemctl start web-dashboard.service
```

### Step 7: Verify Installation

```bash
# Check service status
sudo systemctl status telemetry-logger.service
sudo systemctl status cloud-sync.service
sudo systemctl status web-dashboard.service

# Run automated tests
chmod +x tests/test_system.sh
./tests/test_system.sh
```

## Usage

### Web Dashboard

Access the web dashboard from any device on the same network:

```
http://<raspberry-pi-ip>:8080
```

Find your Pi's IP address:

```bash
hostname -I
```

The dashboard provides:
- **Real-time metrics**: Speed, battery SOC, power, temperatures
- **Historical trends**: Charts similar to Victron monitoring
- **Detailed views**: Raw JSON data for all measurements

### Service Management

```bash
# View logs
sudo journalctl -u telemetry-logger.service -f
sudo journalctl -u cloud-sync.service -f
sudo journalctl -u web-dashboard.service -f

# Restart services
sudo systemctl restart telemetry-logger.service
sudo systemctl restart cloud-sync.service
sudo systemctl restart web-dashboard.service

# Stop services
sudo systemctl stop telemetry-logger.service
sudo systemctl stop cloud-sync.service
sudo systemctl stop web-dashboard.service
```

### InfluxDB Management

Access local InfluxDB UI:

```
http://<raspberry-pi-ip>:8086
```

Query data via CLI:

```bash
# Source configuration
source config/influxdb-local.env

# Query recent battery SOC
influx query 'from(bucket:"'$INFLUX_BUCKET'")
  |> range(start: -1h)
  |> filter(fn: (r) => r["_measurement"] == "battery_soc")
  |> limit(n:10)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN"
```

## Testing

### Mock CAN Data Generator

For testing without a real vehicle, use the mock CAN data generator:

```bash
# Make sure can0 is up
sudo ip link set can0 up type can bitrate 500000

# Run mock generator
python3 tests/mock_can_generator.py can0
```

This simulates realistic vehicle behavior including:
- Acceleration and deceleration cycles
- Battery discharge during driving
- Regenerative braking
- Charging sessions
- Temperature changes based on load

### System Tests

Run the automated test suite:

```bash
./tests/test_system.sh
```

This checks:
- CAN interface status
- InfluxDB health
- Configuration files
- Python dependencies
- Service status
- CAN communication
- Data flow
- Web dashboard API

## Configuration

### Environment Variables

All services use environment variables for configuration. These are set in:
- `config/influxdb-local.env` - Local InfluxDB connection
- `config/influxdb-cloud.env` - Cloud InfluxDB connection
- Systemd service files - Service-specific settings

Key variables:

| Variable | Description | Default |
|----------|-------------|---------|
| `CAN_INTERFACE` | CAN interface name | `can0` |
| `INFLUX_URL` | Local InfluxDB URL | `http://localhost:8086` |
| `INFLUX_ORG` | Organization name | `leaf-telemetry` |
| `INFLUX_BUCKET` | Bucket name | `leaf-data` |
| `SYNC_INTERVAL_SECONDS` | Cloud sync interval | `300` (5 min) |
| `WEB_PORT` | Web dashboard port | `8080` |

### Customization

**Change sync interval:**

Edit `/etc/systemd/system/cloud-sync.service`:

```ini
Environment=SYNC_INTERVAL_SECONDS=600  # 10 minutes
```

**Change web dashboard port:**

Edit `/etc/systemd/system/web-dashboard.service`:

```ini
Environment=WEB_PORT=8888
```

After changes:

```bash
sudo systemctl daemon-reload
sudo systemctl restart <service-name>
```

## Data Storage

### Local InfluxDB

- **Location**: `/var/lib/influxdb2/`
- **Retention**: 30 days (configurable)
- **Measurements**: All CAN messages parsed into separate measurements
- **Tags**: Source device, direction, charging status
- **Fields**: Numeric values (voltage, current, temperature, etc.)

### Cloud InfluxDB

- **Sync frequency**: Every 5 minutes (when online)
- **Batch size**: 1 hour of data per sync
- **Retry logic**: Exponential backoff on failures
- **Network detection**: Checks connectivity before attempting sync

## Troubleshooting

### Services not starting

```bash
# Check logs for errors
sudo journalctl -u telemetry-logger.service -n 50

# Common issues:
# 1. CAN interface not up
sudo ip link set can0 up type can bitrate 500000

# 2. InfluxDB not running
sudo systemctl start influxdb

# 3. Permissions issues
sudo chown -R $USER:$USER ~/test-lvgl-cross-compile/telemetry-system
```

### No data in InfluxDB

```bash
# 1. Check if telemetry logger is receiving CAN messages
sudo journalctl -u telemetry-logger.service -f

# 2. Send test CAN message
cansend can0 1F2#0E1000640A1E1C01

# 3. Check InfluxDB write permissions
source config/influxdb-local.env
influx auth list --org "$INFLUX_ORG"
```

### Cloud sync not working

```bash
# 1. Check cloud-sync logs
sudo journalctl -u cloud-sync.service -f

# 2. Verify cloud credentials
source config/influxdb-cloud.env
curl -I "$INFLUX_CLOUD_URL/health"

# 3. Test network connectivity
ping -c 3 1.1.1.1
```

### Web dashboard not accessible

```bash
# 1. Check if service is running
sudo systemctl status web-dashboard.service

# 2. Check if port is listening
sudo netstat -tlnp | grep 8080

# 3. Check firewall
sudo ufw allow 8080/tcp

# 4. Access from Pi itself
curl http://localhost:8080/api/status
```

## Performance

Expected resource usage on Raspberry Pi 4:

- **CPU**: <5% average per service
- **Memory**: ~100 MB total for all services
- **Disk I/O**: ~1-2 MB/hour in InfluxDB
- **Network**: ~10-50 KB/s during cloud sync

## Security Considerations

1. **No authentication on web UI** - Only use on trusted local networks
2. **API tokens** - Stored in config files with 600 permissions
3. **Local-only by default** - Services bind to all interfaces; use firewall if needed
4. **HTTPS** - Not configured; add reverse proxy (nginx) for external access

To restrict web dashboard to local network only:

Edit `/etc/systemd/system/web-dashboard.service`:

```ini
# Change from 0.0.0.0 to 127.0.0.1 in app.py or use firewall
```

## Backup

### Backup InfluxDB

```bash
# Create backup
influx backup /path/to/backup --org leaf-telemetry

# Restore backup
influx restore /path/to/backup --org leaf-telemetry
```

### Backup Configuration

```bash
# Backup config files (exclude tokens from git!)
tar -czf telemetry-config-backup.tar.gz config/
```

## Contributing

To add new CAN messages:

1. Define CAN ID and state structure in `lib/LeafCANBus/src/LeafCANMessages.h`
2. Add parser function in `services/telemetry-logger/telemetry_logger.py`
3. Update web dashboard to display new metric
4. Update mock generator to include new message

## License

See main repository LICENSE file.

## Support

For issues and questions:
- Check logs: `sudo journalctl -u <service> -f`
- Run tests: `./tests/test_system.sh`
- Consult `CLAUDE.md` for project architecture
- See `Pi Setup.md` for CAN HAT configuration
