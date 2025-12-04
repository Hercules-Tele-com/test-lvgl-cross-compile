#!/usr/bin/env python3
"""
Nissan Leaf CAN Telemetry Logger
Reads CAN messages from SocketCAN and writes to local InfluxDB
"""

import os
import sys
import time
import signal
import logging
import socket
from datetime import datetime
from typing import Dict, Any, Optional
import can
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

# CAN message definitions (matching LeafCANMessages.h)
# Nissan Leaf CAN IDs
CAN_ID_INVERTER_TELEMETRY = 0x1F2
CAN_ID_BATTERY_SOC = 0x1DB
CAN_ID_BATTERY_TEMP = 0x1DC
CAN_ID_VEHICLE_SPEED = 0x1D4
CAN_ID_MOTOR_RPM = 0x1DA
CAN_ID_CHARGER_STATUS = 0x390

# EMBOO Battery CAN IDs (Orion BMS)
CAN_ID_EMBOO_PACK_STATUS = 0x6B0
CAN_ID_EMBOO_PACK_STATS = 0x6B1
CAN_ID_EMBOO_STATUS_FLAGS = 0x6B2   # Fault/status flags
CAN_ID_EMBOO_CELL_VOLTAGES = 0x6B3  # Individual cell voltages
CAN_ID_EMBOO_TEMPERATURES = 0x6B4
CAN_ID_EMBOO_PACK_SUMMARY = 0x351
CAN_ID_EMBOO_PACK_DATA1 = 0x355
CAN_ID_EMBOO_PACK_DATA2 = 0x356

# ROAM Motor Controller CAN IDs (RM100)
CAN_ID_ROAM_TEMP_1 = 0x0A0
CAN_ID_ROAM_TEMP_2 = 0x0A1
CAN_ID_ROAM_TEMP_3 = 0x0A2
CAN_ID_ROAM_POSITION = 0x0A5
CAN_ID_ROAM_CURRENT = 0x0A6
CAN_ID_ROAM_VOLTAGE = 0x0A7
CAN_ID_ROAM_TORQUE = 0x0AC

# Custom ESP32 module CAN IDs
CAN_ID_GPS_POSITION = 0x710
CAN_ID_GPS_VELOCITY = 0x711
CAN_ID_GPS_TIME = 0x712
CAN_ID_BODY_TEMP_SENSORS = 0x720
CAN_ID_BODY_VOLTAGE = 0x721

# Victron BMS Protocol CAN IDs (Little-Endian)
CAN_ID_VICTRON_LIMITS = 0x351    # Charge/discharge voltage/current limits
CAN_ID_VICTRON_SOC = 0x355       # State of Charge / State of Health
CAN_ID_VICTRON_PACK = 0x356      # Battery voltage, current, temperature
CAN_ID_VICTRON_ALARMS = 0x35E    # Alarms and warnings
CAN_ID_VICTRON_CHARACTERISTICS = 0x35F  # Battery characteristics
CAN_ID_VICTRON_CELLS_0 = 0x370   # Cell module 0 extrema
CAN_ID_VICTRON_CELLS_1 = 0x371   # Cell module 1 extrema
CAN_ID_VICTRON_CELLS_2 = 0x372   # Cell module 2 extrema
CAN_ID_VICTRON_CELLS_3 = 0x373   # Cell module 3 extrema

# Setup logging (systemd will capture stdout to journal)
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)


class CANTelemetryLogger:
    """CAN to InfluxDB telemetry logger - supports multiple CAN interfaces"""

    def __init__(self):
        self.running = False
        self.buses: Dict[str, can.Bus] = {}
        self.influx_client: Optional[InfluxDBClient] = None
        self.write_api = None

        # Load configuration (optional - can run without InfluxDB)
        self.influx_url = os.getenv("INFLUX_URL", "http://localhost:8086")
        self.influx_org = os.getenv("INFLUX_ORG", "leaf-telemetry")
        self.influx_bucket = os.getenv("INFLUX_BUCKET", "leaf-data")
        self.influx_token = os.getenv("INFLUX_LOGGER_TOKEN", "")
        self.influx_enabled = bool(self.influx_token)

        # Parse CAN interface configuration
        # Format: CAN_INTERFACES=can0,can1  CAN_BITRATES=250000,250000
        can_interfaces = os.getenv("CAN_INTERFACES", os.getenv("CAN_INTERFACE", "can0"))
        can_bitrates = os.getenv("CAN_BITRATES", os.getenv("CAN_BITRATE", "250000"))

        self.can_configs = []
        interfaces = [i.strip() for i in can_interfaces.split(',')]
        bitrates = [int(b.strip()) for b in can_bitrates.split(',')]

        # If only one bitrate specified but multiple interfaces, use same bitrate for all
        if len(bitrates) == 1 and len(interfaces) > 1:
            bitrates = bitrates * len(interfaces)

        # Validate configuration
        if len(interfaces) != len(bitrates):
            raise ValueError(f"Number of CAN interfaces ({len(interfaces)}) must match number of bitrates ({len(bitrates)})")

        for iface, bitrate in zip(interfaces, bitrates):
            self.can_configs.append({'interface': iface, 'bitrate': bitrate})

        # Statistics (per interface)
        self.stats = {}
        for config in self.can_configs:
            iface = config['interface']
            self.stats[iface] = {
                'msg_count': 0,
                'write_count': 0,
                'error_count': 0,
                'last_stats_time': time.time()
            }

        # Cell voltage tracking (for multi-message cell data)
        self.cell_voltages = {}  # {cell_id: voltage}
        self.last_cell_write = 0

        # Fault tracking
        self.active_faults = set()
        self.fault_counts = {}

        # CAN ID tracking for debugging
        self.can_id_counts = {}

        # Get hostname for serial numbers
        self.hostname = socket.gethostname()
        logger.info(f"Vehicle hostname: {self.hostname}")

    def _get_serial_number(self, device_family: str, device_type: str) -> str:
        """Generate serial number based on hostname and device type"""
        return f"{self.hostname}-{device_family}-{device_type}"

    def init(self) -> bool:
        """Initialize CAN buses and InfluxDB connection"""
        try:
            # Initialize all CAN buses
            logger.info(f"Initializing {len(self.can_configs)} CAN interface(s)...")
            for config in self.can_configs:
                iface = config['interface']
                bitrate = config['bitrate']
                logger.info(f"  - {iface} at {bitrate} bps")

                bus = can.Bus(
                    interface='socketcan',
                    channel=iface,
                    bitrate=bitrate
                )
                self.buses[iface] = bus

            logger.info("All CAN buses initialized successfully")

            # Initialize InfluxDB client
            logger.info(f"Connecting to InfluxDB: {self.influx_url}")
            self.influx_client = InfluxDBClient(
                url=self.influx_url,
                token=self.influx_token,
                org=self.influx_org
            )
            self.write_api = self.influx_client.write_api(write_options=SYNCHRONOUS)

            # Test connection
            health = self.influx_client.health()
            if health.status == "pass":
                logger.info("InfluxDB connection successful")
                return True
            else:
                logger.info("InfluxDB disabled (no token configured) - CAN messages will be received but not logged")

            return True  # Success if CAN buses initialized

        except Exception as e:
            logger.error(f"CAN bus initialization failed: {e}")
            return False

    def parse_inverter_telemetry(self, data: bytes) -> Optional[Point]:
        """Parse inverter telemetry CAN message"""
        if len(data) < 8:
            return None

        voltage = int.from_bytes(data[0:2], 'big') * 0.1  # V
        current = int.from_bytes(data[2:4], 'big', signed=True) * 0.1  # A
        temp_inverter = int.from_bytes(data[4:5], 'big', signed=True)  # °C
        temp_motor = int.from_bytes(data[5:6], 'big', signed=True)  # °C
        status_flags = data[6]

        point = Point("Inverter") \
            .tag("serial_number", self._get_serial_number("Inverter", "LeafEM57")) \
            .tag("source", "leaf_ecu") \
            .tag("device_type", "EM57") \
            .field("voltage", float(voltage)) \
            .field("current", float(current)) \
            .field("temp_inverter", float(temp_inverter)) \
            .field("temp_motor", float(temp_motor)) \
            .field("status_flags", int(status_flags)) \
            .field("power_kw", float(voltage * current / 1000.0))

        return point

    def parse_battery_soc(self, data: bytes) -> Optional[Point]:
        """Parse battery SOC CAN message (Nissan Leaf)"""
        if len(data) < 8:
            return None

        # Nissan Leaf 2012+ CAN format
        soc_raw = data[0]  # SOC in 0.5% units (200 = 100%)
        soc_percent = soc_raw / 2.0

        gids = int.from_bytes(data[1:3], 'big')  # No scaling

        # Voltage in 0.5V units
        pack_voltage_raw = int.from_bytes(data[3:5], 'big')
        pack_voltage = pack_voltage_raw * 0.5

        # Current in 0.5A units, signed
        pack_current_raw = int.from_bytes(data[5:7], 'big', signed=True)
        pack_current = pack_current_raw * 0.5

        point = Point("Battery") \
            .tag("serial_number", self._get_serial_number("Battery", "Leaf24kWh")) \
            .tag("source", "leaf_ecu") \
            .tag("device_type", "Leaf_24kWh") \
            .field("soc_percent", int(soc_percent)) \
            .field("gids", int(gids)) \
            .field("voltage", float(pack_voltage)) \
            .field("current", float(pack_current)) \
            .field("power_kw", float(pack_voltage * pack_current / 1000.0))

        return point

    def parse_battery_temp(self, data: bytes) -> Optional[Point]:
        """Parse battery temperature CAN message (Nissan Leaf)"""
        if len(data) < 4:
            return None

        # Nissan Leaf temperature format (unsigned bytes, direct Celsius values)
        # Temperatures are typically in range 0-80°C
        temp1 = data[0]  # Temperature sensor 1
        temp2 = data[1]  # Temperature sensor 2
        temp3 = data[2]  # Temperature sensor 3
        temp4 = data[3]  # Temperature sensor 4

        # Calculate statistics from available sensors
        temps = [t for t in [temp1, temp2, temp3, temp4] if 0 < t < 100]  # Filter valid temps

        if not temps:
            return None

        temp_max = max(temps)
        temp_min = min(temps)
        temp_avg = sum(temps) / len(temps)
        sensor_count = len(temps)

        point = Point("Battery") \
            .tag("serial_number", self._get_serial_number("Battery", "Leaf24kWh")) \
            .tag("source", "leaf_ecu") \
            .tag("device_type", "Leaf_24kWh") \
            .field("temp_max", float(temp_max)) \
            .field("temp_min", float(temp_min)) \
            .field("temp_avg", float(temp_avg)) \
            .field("temp_delta", float(temp_max - temp_min)) \
            .field("temp_sensor_count", int(sensor_count))

        return point

    def parse_vehicle_speed(self, data: bytes) -> Optional[Point]:
        """Parse vehicle speed CAN message"""
        if len(data) < 4:
            return None

        speed_kmh = int.from_bytes(data[0:2], 'big') * 0.01
        speed_mph = int.from_bytes(data[2:4], 'big') * 0.01

        point = Point("Vehicle") \
            .tag("serial_number", self._get_serial_number("Vehicle", "LeafZE0")) \
            .tag("source", "leaf_ecu") \
            .tag("device_type", "Leaf_ZE0") \
            .field("speed_kmh", float(speed_kmh)) \
            .field("speed_mph", float(speed_mph))

        return point

    def parse_motor_rpm(self, data: bytes) -> Optional[Point]:
        """Parse motor RPM CAN message (Nissan Leaf EM57)"""
        if len(data) < 3:
            return None

        rpm = int.from_bytes(data[0:2], 'big', signed=True)
        direction = data[2]

        direction_str = "stopped"
        if direction == 1:
            direction_str = "forward"
        elif direction == 2:
            direction_str = "reverse"

        point = Point("Motor") \
            .tag("serial_number", self._get_serial_number("Motor", "LeafEM57")) \
            .tag("source", "leaf_ecu") \
            .tag("device_type", "EM57") \
            .tag("direction", direction_str) \
            .field("rpm", int(rpm))

        return point

    def parse_charger_status(self, data: bytes) -> Optional[Point]:
        """Parse charger status CAN message"""
        if len(data) < 7:
            return None

        charging = data[0]
        charge_current = int.from_bytes(data[1:3], 'big') * 0.1
        charge_voltage = int.from_bytes(data[3:5], 'big') * 0.1
        charge_time = int.from_bytes(data[5:7], 'big')

        point = Point("Battery") \
            .tag("serial_number", self._get_serial_number("Battery", "Leaf24kWh")) \
            .tag("source", "leaf_ecu") \
            .tag("device_type", "Leaf_24kWh") \
            .tag("charging", "yes" if charging else "no") \
            .field("charging_flag", int(charging)) \
            .field("charge_current", float(charge_current)) \
            .field("charge_voltage", float(charge_voltage)) \
            .field("charge_time_remaining_min", int(charge_time)) \
            .field("charge_power_kw", float(charge_voltage * charge_current / 1000.0))

        return point

    def parse_gps_position(self, data: bytes) -> Optional[Point]:
        """Parse GPS position CAN message"""
        if len(data) < 8:
            return None

        # Latitude/longitude stored as int32 * 1e7
        lat_raw = int.from_bytes(data[0:4], 'big', signed=True)
        lon_raw = int.from_bytes(data[4:8], 'big', signed=True)

        latitude = lat_raw / 1e7
        longitude = lon_raw / 1e7

        # Additional data might be in subsequent messages
        point = Point("GPS") \
            .tag("serial_number", self._get_serial_number("GPS", "ESP32")) \
            .tag("source", "esp32_gps") \
            .tag("device_type", "NEO_6M") \
            .field("latitude", float(latitude)) \
            .field("longitude", float(longitude))

        return point

    def parse_gps_velocity(self, data: bytes) -> Optional[Point]:
        """Parse GPS velocity CAN message"""
        if len(data) < 6:
            return None

        speed_kmh = int.from_bytes(data[0:2], 'big') * 0.01
        heading = int.from_bytes(data[2:4], 'big') * 0.01
        pdop = int.from_bytes(data[4:6], 'big') * 0.01

        point = Point("GPS") \
            .tag("serial_number", self._get_serial_number("GPS", "ESP32")) \
            .tag("source", "esp32_gps") \
            .tag("device_type", "NEO_6M") \
            .field("speed_kmh", float(speed_kmh)) \
            .field("heading", float(heading)) \
            .field("pdop", float(pdop))

        return point

    def parse_body_temp(self, data: bytes) -> Optional[Point]:
        """Parse body temperature sensors CAN message"""
        if len(data) < 8:
            return None

        temp1 = int.from_bytes(data[0:2], 'big', signed=True) / 10.0
        temp2 = int.from_bytes(data[2:4], 'big', signed=True) / 10.0
        temp3 = int.from_bytes(data[4:6], 'big', signed=True) / 10.0
        temp4 = int.from_bytes(data[6:8], 'big', signed=True) / 10.0

        point = Point("Vehicle") \
            .tag("serial_number", self._get_serial_number("Vehicle", "LeafZE0")) \
            .tag("source", "esp32_body") \
            .tag("device_type", "BodySensors") \
            .field("body_temp1", float(temp1)) \
            .field("body_temp2", float(temp2)) \
            .field("body_temp3", float(temp3)) \
            .field("body_temp4", float(temp4))

        return point

    def parse_body_voltage(self, data: bytes) -> Optional[Point]:
        """Parse body voltage monitoring CAN message"""
        if len(data) < 6:
            return None

        voltage_12v = int.from_bytes(data[0:2], 'big') * 0.01
        voltage_5v = int.from_bytes(data[2:4], 'big') * 0.01
        current_12v = int.from_bytes(data[4:6], 'big') * 0.01

        point = Point("Vehicle") \
            .tag("serial_number", self._get_serial_number("Vehicle", "LeafZE0")) \
            .tag("source", "esp32_body") \
            .tag("device_type", "BodySensors") \
            .field("aux_voltage_12v", float(voltage_12v)) \
            .field("aux_voltage_5v", float(voltage_5v)) \
            .field("aux_current_12v", float(current_12v))

        return point

    # ============================================================================
    # EMBOO Battery Parsers
    # ============================================================================

    def parse_emboo_pack_status(self, data: bytes) -> Optional[Point]:
        """Parse EMBOO pack status (0x6B0)"""
        if len(data) < 8:
            return None

        # Big-endian format
        pack_current = int.from_bytes(data[0:2], 'big', signed=True) * 0.1  # A
        pack_voltage = int.from_bytes(data[2:4], 'big') * 0.1  # V
        pack_soc = data[6] * 0.5  # %

        point = Point("Battery") \
            .tag("serial_number", self._get_serial_number("Battery", "EMBOO")) \
            .tag("source", "emboo_bms") \
            .tag("device_type", "Orion_BMS") \
            .field("soc_percent", float(pack_soc)) \
            .field("voltage", float(pack_voltage)) \
            .field("current", float(pack_current)) \
            .field("power_kw", float(pack_voltage * pack_current / 1000.0))

        return point

    def parse_emboo_temperatures(self, data: bytes) -> Optional[Point]:
        """Parse EMBOO temperatures (0x6B4)"""
        if len(data) < 6:
            return None

        # Orion BMS format: single-byte temperatures at bytes 4-5
        high_temp = int.from_bytes([data[4]], 'big', signed=True)  # °C (signed byte)
        low_temp = int.from_bytes([data[5]], 'big', signed=True)   # °C (signed byte)

        point = Point("Battery") \
            .tag("serial_number", self._get_serial_number("Battery", "EMBOO")) \
            .tag("source", "emboo_bms") \
            .tag("device_type", "Orion_BMS") \
            .field("temp_max", float(high_temp)) \
            .field("temp_min", float(low_temp)) \
            .field("temp_avg", float((high_temp + low_temp) / 2.0)) \
            .field("temp_delta", float(high_temp - low_temp))

        return point

    def parse_emboo_cell_voltages(self, data: bytes) -> Optional[Point]:
        """Parse EMBOO cell voltages (0x6B3) - Orion BMS format"""
        if len(data) < 8:
            return None

        # Orion BMS sends cell voltages in multiple messages
        # Format: [cell_id, voltage_low, voltage_high, cell_id+1, voltage_low, voltage_high, ...]
        # Voltage resolution: 0.0001V, little-endian 16-bit

        try:
            # First cell pair
            cell_id_1 = data[0]
            voltage_1 = int.from_bytes([data[1], data[2]], 'little') * 0.0001  # V

            # Second cell pair (if present)
            if len(data) >= 6:
                cell_id_2 = data[3]
                voltage_2 = int.from_bytes([data[4], data[5]], 'little') * 0.0001  # V

                # Store in tracking dict
                self.cell_voltages[cell_id_1] = voltage_1
                self.cell_voltages[cell_id_2] = voltage_2
            else:
                self.cell_voltages[cell_id_1] = voltage_1

            # Write all cells periodically (every 2 seconds)
            now = time.time()
            if now - self.last_cell_write >= 2.0 and len(self.cell_voltages) > 0:
                # Calculate min/max/avg
                voltages = list(self.cell_voltages.values())
                min_voltage = min(voltages)
                max_voltage = max(voltages)
                avg_voltage = sum(voltages) / len(voltages)

                point = Point("Battery") \
                    .tag("serial_number", self._get_serial_number("Battery", "EMBOO")) \
                    .tag("source", "emboo_bms") \
                    .tag("device_type", "Orion_BMS") \
                    .field("cell_count", len(voltages)) \
                    .field("cell_voltage_min", float(min_voltage)) \
                    .field("cell_voltage_max", float(max_voltage)) \
                    .field("cell_voltage_avg", float(avg_voltage)) \
                    .field("cell_voltage_delta", float(max_voltage - min_voltage))

                # Add individual cell voltages (up to 144 cells)
                for cell_id, voltage in sorted(self.cell_voltages.items())[:144]:
                    point.field(f"cell_{cell_id:03d}", float(voltage))

                self.last_cell_write = now
                return point

        except Exception as e:
            logger.error(f"Error parsing cell voltages: {e}")

        return None

    def parse_emboo_status_flags(self, data: bytes) -> Optional[Point]:
        """Parse EMBOO status flags (0x6B2) - Fault detection"""
        if len(data) < 8:
            return None

        # Orion BMS status flags (specific bits indicate faults)
        # Bytes 0-7 contain various status and fault flags
        status_byte_0 = data[0]
        status_byte_1 = data[1]
        status_byte_2 = data[2]
        status_byte_3 = data[3]

        # Decode fault flags (Orion BMS specific)
        faults = []
        fault_bits = (status_byte_3 << 24) | (status_byte_2 << 16) | (status_byte_1 << 8) | status_byte_0

        # Common Orion BMS faults
        if status_byte_0 & 0x01:
            faults.append("discharge_overcurrent")
        if status_byte_0 & 0x02:
            faults.append("charge_overcurrent")
        if status_byte_0 & 0x04:
            faults.append("cell_overvoltage")
        if status_byte_0 & 0x08:
            faults.append("cell_undervoltage")
        if status_byte_0 & 0x10:
            faults.append("over_temperature")
        if status_byte_0 & 0x20:
            faults.append("under_temperature")
        if status_byte_1 & 0x01:
            faults.append("communication_fault")
        if status_byte_1 & 0x02:
            faults.append("internal_fault")
        if status_byte_3 & 0x04:
            faults.append("cell_imbalance")

        # Log new faults
        current_faults = set(faults)
        new_faults = current_faults - self.active_faults
        cleared_faults = self.active_faults - current_faults

        for fault in new_faults:
            logger.warning(f"⚠️  NEW FAULT: {fault}")
            self.fault_counts[fault] = self.fault_counts.get(fault, 0) + 1

        for fault in cleared_faults:
            logger.info(f"✓ CLEARED: {fault}")

        self.active_faults = current_faults

        # Create InfluxDB point
        point = Point("Battery") \
            .tag("serial_number", self._get_serial_number("Battery", "EMBOO")) \
            .tag("source", "emboo_bms") \
            .tag("device_type", "Orion_BMS") \
            .field("fault_count", len(faults)) \
            .field("fault_status_raw", int(fault_bits))

        # Add individual fault flags
        for fault in faults:
            point.field(f"fault_{fault}", 1)

        # Add total fault occurrences
        for fault, count in self.fault_counts.items():
            point.field(f"fault_total_{fault}", int(count))

        return point

    # ============================================================================
    # ROAM Motor Controller Parsers
    # ============================================================================

    def parse_roam_torque(self, data: bytes) -> Optional[Point]:
        """Parse ROAM motor torque (0x0AC)"""
        if len(data) < 4:
            return None

        # Little-endian format
        torque_request = int.from_bytes(data[0:2], 'little', signed=True)  # Nm
        torque_actual = int.from_bytes(data[2:4], 'little', signed=True)  # Nm

        point = Point("Motor") \
            .tag("serial_number", self._get_serial_number("Motor", "ROAM")) \
            .tag("source", "roam_motor") \
            .tag("device_type", "RM100") \
            .field("torque_request", int(torque_request)) \
            .field("torque_actual", int(torque_actual))

        return point

    def parse_roam_position(self, data: bytes) -> Optional[Point]:
        """Parse ROAM motor position/RPM (0x0A5)"""
        if len(data) < 8:
            return None

        # Big-endian pairs
        motor_angle = (data[0] << 8) | data[1]  # degrees
        motor_rpm = int.from_bytes([data[2], data[3]], 'big', signed=True)
        electrical_freq = (data[4] << 8) | data[5]  # Hz

        point = Point("Motor") \
            .tag("serial_number", self._get_serial_number("Motor", "ROAM")) \
            .tag("source", "roam_motor") \
            .tag("device_type", "RM100") \
            .field("rpm", int(motor_rpm)) \
            .field("position_angle", int(motor_angle)) \
            .field("electrical_freq", int(electrical_freq))

        return point

    def parse_roam_voltage(self, data: bytes) -> Optional[Point]:
        """Parse ROAM motor voltage (0x0A7)"""
        if len(data) < 8:
            return None

        # Little-endian pairs, 0.1V resolution
        dc_bus_voltage = int.from_bytes([data[0], data[1]], 'little')  # 0.1V units
        output_voltage = int.from_bytes([data[2], data[3]], 'little')  # 0.1V units

        point = Point("Motor") \
            .tag("serial_number", self._get_serial_number("Motor", "ROAM")) \
            .tag("source", "roam_motor") \
            .tag("device_type", "RM100") \
            .field("voltage_dc_bus", int(dc_bus_voltage)) \
            .field("voltage_output", int(output_voltage))

        return point

    def parse_roam_current(self, data: bytes) -> Optional[Point]:
        """Parse ROAM motor current (0x0A6)"""
        if len(data) < 8:
            return None

        # Little-endian pairs, signed, 1A resolution
        phase_a = int.from_bytes([data[0], data[1]], 'little', signed=True)
        phase_b = int.from_bytes([data[2], data[3]], 'little', signed=True)
        phase_c = int.from_bytes([data[4], data[5]], 'little', signed=True)
        dc_bus_current = int.from_bytes([data[6], data[7]], 'little', signed=True)

        point = Point("Motor") \
            .tag("serial_number", self._get_serial_number("Motor", "ROAM")) \
            .tag("source", "roam_motor") \
            .tag("device_type", "RM100") \
            .field("current_phase_a", int(phase_a)) \
            .field("current_phase_b", int(phase_b)) \
            .field("current_phase_c", int(phase_c)) \
            .field("current_dc_bus", int(dc_bus_current))

        return point

    def parse_roam_temp_2(self, data: bytes) -> Optional[Point]:
        """Parse ROAM motor temperatures #2 (0x0A1) - Control board"""
        if len(data) < 8:
            return None

        # Little-endian pairs, °C * 10
        control_board_temp = int.from_bytes([data[0], data[1]], 'little', signed=True) / 10.0

        point = Point("Inverter") \
            .tag("serial_number", self._get_serial_number("Inverter", "ROAM")) \
            .tag("source", "roam_motor") \
            .tag("device_type", "RM100") \
            .field("temp_inverter", float(control_board_temp))

        return point

    def parse_roam_temp_3(self, data: bytes) -> Optional[Point]:
        """Parse ROAM motor temperatures #3 (0x0A2) - Stator"""
        if len(data) < 8:
            return None

        # Little-endian pairs, °C * 10
        stator_temp = int.from_bytes([data[4], data[5]], 'little', signed=True) / 10.0

        point = Point("Motor") \
            .tag("serial_number", self._get_serial_number("Motor", "ROAM")) \
            .tag("source", "roam_motor") \
            .tag("device_type", "RM100") \
            .field("temp_stator", float(stator_temp))

        return point

    def process_can_message(self, msg: can.Message, source_interface: str = "unknown"):
        """Process a single CAN message and write to InfluxDB"""
        if source_interface in self.stats:
            self.stats[source_interface]['msg_count'] += 1

        # Track CAN IDs for debugging
        if msg.arbitration_id not in self.can_id_counts:
            self.can_id_counts[msg.arbitration_id] = 0
        self.can_id_counts[msg.arbitration_id] += 1

        point = None

        # Parse based on CAN ID
        # Inverter telemetry enabled
        if msg.arbitration_id == CAN_ID_INVERTER_TELEMETRY:
            point = self.parse_inverter_telemetry(msg.data)
        # Leaf BMS battery data disabled (incorrect parsing - to be fixed later)
        # elif msg.arbitration_id == CAN_ID_BATTERY_SOC:
        #     point = self.parse_battery_soc(msg.data)
        # elif msg.arbitration_id == CAN_ID_BATTERY_TEMP:
        #     point = self.parse_battery_temp(msg.data)
        elif msg.arbitration_id == CAN_ID_VEHICLE_SPEED:
            point = self.parse_vehicle_speed(msg.data)
        elif msg.arbitration_id == CAN_ID_MOTOR_RPM:
            point = self.parse_motor_rpm(msg.data)
        elif msg.arbitration_id == CAN_ID_CHARGER_STATUS:
            point = self.parse_charger_status(msg.data)
        elif msg.arbitration_id == CAN_ID_GPS_POSITION:
            point = self.parse_gps_position(msg.data)
        elif msg.arbitration_id == CAN_ID_GPS_VELOCITY:
            point = self.parse_gps_velocity(msg.data)
        elif msg.arbitration_id == CAN_ID_BODY_TEMP_SENSORS:
            point = self.parse_body_temp(msg.data)
        elif msg.arbitration_id == CAN_ID_BODY_VOLTAGE:
            point = self.parse_body_voltage(msg.data)
        # EMBOO Battery CAN IDs
        elif msg.arbitration_id == CAN_ID_EMBOO_PACK_STATUS:
            point = self.parse_emboo_pack_status(msg.data)
        elif msg.arbitration_id == CAN_ID_EMBOO_TEMPERATURES:
            point = self.parse_emboo_temperatures(msg.data)
        elif msg.arbitration_id == CAN_ID_EMBOO_CELL_VOLTAGES:
            point = self.parse_emboo_cell_voltages(msg.data)
        elif msg.arbitration_id == CAN_ID_EMBOO_STATUS_FLAGS:
            point = self.parse_emboo_status_flags(msg.data)
        # ROAM Motor CAN IDs
        elif msg.arbitration_id == CAN_ID_ROAM_TORQUE:
            point = self.parse_roam_torque(msg.data)
        elif msg.arbitration_id == CAN_ID_ROAM_POSITION:
            point = self.parse_roam_position(msg.data)
        elif msg.arbitration_id == CAN_ID_ROAM_VOLTAGE:
            point = self.parse_roam_voltage(msg.data)
        elif msg.arbitration_id == CAN_ID_ROAM_CURRENT:
            point = self.parse_roam_current(msg.data)
        elif msg.arbitration_id == CAN_ID_ROAM_TEMP_2:
            point = self.parse_roam_temp_2(msg.data)
        elif msg.arbitration_id == CAN_ID_ROAM_TEMP_3:
            point = self.parse_roam_temp_3(msg.data)

        # Write to InfluxDB
        if point:
            try:
                self.write_api.write(bucket=self.influx_bucket, record=point)
                if source_interface in self.stats:
                    self.stats[source_interface]['write_count'] += 1
            except Exception as e:
                if source_interface in self.stats:
                    self.stats[source_interface]['error_count'] += 1
                logger.error(f"Failed to write to InfluxDB: {e}")

    def print_statistics(self):
        """Print periodic statistics for all interfaces"""
        now = time.time()

        for iface, stats in self.stats.items():
            elapsed = now - stats['last_stats_time']

            if elapsed >= 10.0:  # Every 10 seconds
                msg_rate = stats['msg_count'] / elapsed
                write_rate = stats['write_count'] / elapsed

                logger.info(
                    f"[{iface}] Stats: {stats['msg_count']} msgs ({msg_rate:.1f}/s), "
                    f"{stats['write_count']} writes ({write_rate:.1f}/s), "
                    f"{stats['error_count']} errors"
                )

                stats['msg_count'] = 0
                stats['write_count'] = 0
                stats['error_count'] = 0
                stats['last_stats_time'] = now

    def run(self):
        """Main loop: read CAN messages from all interfaces and log to InfluxDB"""
        self.running = True
        logger.info("Starting telemetry logger main loop")

        try:
            while self.running:
                # Read from all CAN buses
                for iface, bus in self.buses.items():
                    msg = bus.recv(timeout=0.01)  # Short timeout to check all buses frequently

                    if msg:
                        self.process_can_message(msg, source_interface=iface)

                # Print periodic statistics
                self.print_statistics()

        except KeyboardInterrupt:
            logger.info("Keyboard interrupt received")
        except Exception as e:
            logger.error(f"Error in main loop: {e}")
        finally:
            self.cleanup()

    def cleanup(self):
        """Cleanup resources"""
        logger.info("Cleaning up...")
        self.running = False

        if self.write_api:
            self.write_api.close()
        if self.influx_client:
            self.influx_client.close()

        # Shutdown all CAN buses
        for iface, bus in self.buses.items():
            logger.info(f"Shutting down {iface}")
            bus.shutdown()

        logger.info("Cleanup complete")

    def signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        logger.info(f"Received signal {signum}")
        self.running = False


def main():
    """Main entry point"""
    logger.info("=== Nissan Leaf CAN Telemetry Logger ===")

    try:
        logger_service = CANTelemetryLogger()

        # Setup signal handlers
        signal.signal(signal.SIGINT, logger_service.signal_handler)
        signal.signal(signal.SIGTERM, logger_service.signal_handler)

        # Initialize
        if not logger_service.init():
            logger.error("Failed to initialize")
            return 1

        # Run main loop
        logger_service.run()

        return 0

    except Exception as e:
        logger.error(f"Fatal error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
