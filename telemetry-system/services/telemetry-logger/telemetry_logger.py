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
from datetime import datetime
from typing import Dict, Any, Optional
import can
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

# CAN message definitions (matching LeafCANMessages.h)
CAN_ID_INVERTER_TELEMETRY = 0x1F2
CAN_ID_BATTERY_SOC = 0x1DB
CAN_ID_BATTERY_TEMP = 0x1DC
CAN_ID_VEHICLE_SPEED = 0x1D4
CAN_ID_MOTOR_RPM = 0x1DA
CAN_ID_CHARGER_STATUS = 0x390
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

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler('/var/log/telemetry-logger.log')
    ]
)
logger = logging.getLogger(__name__)


class CANTelemetryLogger:
    """CAN to InfluxDB telemetry logger"""

    def __init__(self, can_interface: str = "can0"):
        self.can_interface = can_interface
        self.running = False
        self.bus: Optional[can.Bus] = None
        self.influx_client: Optional[InfluxDBClient] = None
        self.write_api = None

        # Load configuration
        self.influx_url = os.getenv("INFLUX_URL", "http://localhost:8086")
        self.influx_org = os.getenv("INFLUX_ORG", "leaf-telemetry")
        self.influx_bucket = os.getenv("INFLUX_BUCKET", "leaf-data")
        self.influx_token = os.getenv("INFLUX_LOGGER_TOKEN", "")

        if not self.influx_token:
            raise ValueError("INFLUX_LOGGER_TOKEN environment variable not set")

        # Statistics
        self.msg_count = 0
        self.write_count = 0
        self.error_count = 0
        self.last_stats_time = time.time()

    def init(self) -> bool:
        """Initialize CAN bus and InfluxDB connection"""
        try:
            # Initialize CAN bus
            logger.info(f"Initializing CAN interface: {self.can_interface}")
            self.bus = can.Bus(
                interface='socketcan',
                channel=self.can_interface,
                bitrate=500000
            )
            logger.info("CAN bus initialized successfully")

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
                logger.error(f"InfluxDB health check failed: {health.message}")
                return False

        except Exception as e:
            logger.error(f"Initialization failed: {e}")
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

        point = Point("inverter") \
            .tag("source", "leaf_ecu") \
            .field("voltage", float(voltage)) \
            .field("current", float(current)) \
            .field("temp_inverter", int(temp_inverter)) \
            .field("temp_motor", int(temp_motor)) \
            .field("status_flags", int(status_flags)) \
            .field("power_kw", float(voltage * current / 1000.0))

        return point

    def parse_battery_soc(self, data: bytes) -> Optional[Point]:
        """Parse battery SOC CAN message"""
        if len(data) < 8:
            return None

        soc_percent = data[0]
        gids = int.from_bytes(data[1:3], 'big')
        pack_voltage = int.from_bytes(data[3:5], 'big') * 0.1  # V
        pack_current = int.from_bytes(data[5:7], 'big', signed=True) * 0.1  # A

        point = Point("battery_soc") \
            .tag("source", "leaf_ecu") \
            .field("soc_percent", int(soc_percent)) \
            .field("gids", int(gids)) \
            .field("pack_voltage", float(pack_voltage)) \
            .field("pack_current", float(pack_current)) \
            .field("pack_power_kw", float(pack_voltage * pack_current / 1000.0))

        return point

    def parse_battery_temp(self, data: bytes) -> Optional[Point]:
        """Parse battery temperature CAN message"""
        if len(data) < 4:
            return None

        temp_max = int.from_bytes(data[0:1], 'big', signed=True)
        temp_min = int.from_bytes(data[1:2], 'big', signed=True)
        temp_avg = int.from_bytes(data[2:3], 'big', signed=True)
        sensor_count = data[3]

        point = Point("battery_temp") \
            .tag("source", "leaf_ecu") \
            .field("temp_max", int(temp_max)) \
            .field("temp_min", int(temp_min)) \
            .field("temp_avg", int(temp_avg)) \
            .field("temp_delta", int(temp_max - temp_min)) \
            .field("sensor_count", int(sensor_count))

        return point

    def parse_vehicle_speed(self, data: bytes) -> Optional[Point]:
        """Parse vehicle speed CAN message"""
        if len(data) < 4:
            return None

        speed_kmh = int.from_bytes(data[0:2], 'big') * 0.01
        speed_mph = int.from_bytes(data[2:4], 'big') * 0.01

        point = Point("vehicle_speed") \
            .tag("source", "leaf_ecu") \
            .field("speed_kmh", float(speed_kmh)) \
            .field("speed_mph", float(speed_mph))

        return point

    def parse_motor_rpm(self, data: bytes) -> Optional[Point]:
        """Parse motor RPM CAN message"""
        if len(data) < 3:
            return None

        rpm = int.from_bytes(data[0:2], 'big', signed=True)
        direction = data[2]

        direction_str = "stopped"
        if direction == 1:
            direction_str = "forward"
        elif direction == 2:
            direction_str = "reverse"

        point = Point("motor_rpm") \
            .tag("source", "leaf_ecu") \
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

        point = Point("charger") \
            .tag("source", "leaf_ecu") \
            .tag("charging", "yes" if charging else "no") \
            .field("charging_flag", int(charging)) \
            .field("current", float(charge_current)) \
            .field("voltage", float(charge_voltage)) \
            .field("time_remaining_min", int(charge_time)) \
            .field("power_kw", float(charge_voltage * charge_current / 1000.0))

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
        point = Point("gps_position") \
            .tag("source", "esp32_gps") \
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

        point = Point("gps_velocity") \
            .tag("source", "esp32_gps") \
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

        point = Point("body_temp") \
            .tag("source", "esp32_temp") \
            .field("temp1", float(temp1)) \
            .field("temp2", float(temp2)) \
            .field("temp3", float(temp3)) \
            .field("temp4", float(temp4))

        return point

    def parse_body_voltage(self, data: bytes) -> Optional[Point]:
        """Parse body voltage monitoring CAN message"""
        if len(data) < 6:
            return None

        voltage_12v = int.from_bytes(data[0:2], 'big') * 0.01
        voltage_5v = int.from_bytes(data[2:4], 'big') * 0.01
        current_12v = int.from_bytes(data[4:6], 'big') * 0.01

        point = Point("body_voltage") \
            .tag("source", "esp32_voltage") \
            .field("voltage_12v", float(voltage_12v)) \
            .field("voltage_5v", float(voltage_5v)) \
            .field("current_12v", float(current_12v))

        return point

    def parse_victron_limits(self, data: bytes) -> Optional[Point]:
        """Parse Victron BMS charge/discharge limits (0x351) - Little-Endian"""
        if len(data) < 8:
            return None

        # Little-Endian decoding
        charge_voltage_max = int.from_bytes(data[0:2], 'little') * 0.1  # V
        charge_current_max = int.from_bytes(data[2:4], 'little', signed=True) * 0.1  # A
        discharge_current_max = int.from_bytes(data[4:6], 'little', signed=True) * 0.1  # A
        discharge_voltage_min = int.from_bytes(data[6:8], 'little') * 0.1  # V

        point = Point("victron_limits") \
            .tag("source", "victron_bms") \
            .field("charge_voltage_max", float(charge_voltage_max)) \
            .field("charge_current_max", float(charge_current_max)) \
            .field("discharge_current_max", float(discharge_current_max)) \
            .field("discharge_voltage_min", float(discharge_voltage_min))

        return point

    def parse_victron_soc(self, data: bytes) -> Optional[Point]:
        """Parse Victron BMS SOC/SOH (0x355) - Little-Endian"""
        if len(data) < 4:
            return None

        # Little-Endian decoding - NO scaling (direct 0-100%)
        soc = int.from_bytes(data[0:2], 'little')  # %
        soh = int.from_bytes(data[2:4], 'little')  # %

        point = Point("victron_soc") \
            .tag("source", "victron_bms") \
            .field("soc_percent", int(soc)) \
            .field("soh_percent", int(soh))

        return point

    def parse_victron_pack(self, data: bytes) -> Optional[Point]:
        """Parse Victron BMS pack measurements (0x356) - Little-Endian"""
        if len(data) < 6:
            return None

        # Little-Endian decoding
        pack_voltage = int.from_bytes(data[0:2], 'little') * 0.01  # 0.01V units
        pack_current = int.from_bytes(data[2:4], 'little', signed=True) * 0.1  # 0.1A units (negative = charge)
        pack_temp = int.from_bytes(data[4:6], 'little') * 0.1  # 0.1°C units

        point = Point("victron_pack") \
            .tag("source", "victron_bms") \
            .field("voltage", float(pack_voltage)) \
            .field("current", float(pack_current)) \
            .field("temperature", float(pack_temp)) \
            .field("power_kw", float(pack_voltage * pack_current / 1000.0))

        return point

    def parse_victron_alarms(self, data: bytes) -> Optional[Point]:
        """Parse Victron BMS alarms (0x35E)"""
        if len(data) < 8:
            return None

        alarms = data[0]  # Alarm flags
        warnings = data[1]  # Warning flags
        num_modules = int.from_bytes(data[2:4], 'little')

        point = Point("victron_alarms") \
            .tag("source", "victron_bms") \
            .field("alarm_low_soc", int((alarms & 0x01) != 0)) \
            .field("alarm_high_temp", int((alarms & 0x04) != 0)) \
            .field("alarm_low_temp", int((alarms & 0x08) != 0)) \
            .field("warning_low_soc", int((warnings & 0x01) != 0)) \
            .field("warning_high_temp", int((warnings & 0x04) != 0)) \
            .field("warning_low_temp", int((warnings & 0x08) != 0)) \
            .field("num_modules", int(num_modules))

        return point

    def parse_victron_characteristics(self, data: bytes) -> Optional[Point]:
        """Parse Victron BMS battery characteristics (0x35F) - Little-Endian"""
        if len(data) < 8:
            return None

        # Little-Endian decoding
        cell_type = data[0]  # 0=Unknown, 1=LiFePO4, 2=NMC
        cell_quantity = data[1]
        fw_major = data[2]
        fw_minor = data[3]
        capacity_ah = int.from_bytes(data[4:6], 'little')  # Ah (NO scaling)
        manufacturer_id = int.from_bytes(data[6:8], 'little')

        cell_type_names = {0: "Unknown", 1: "LiFePO4", 2: "NMC"}
        cell_type_name = cell_type_names.get(cell_type, "Unknown")

        point = Point("victron_characteristics") \
            .tag("source", "victron_bms") \
            .tag("cell_type", cell_type_name) \
            .field("cell_type_code", int(cell_type)) \
            .field("cell_quantity", int(cell_quantity)) \
            .field("firmware_major", int(fw_major)) \
            .field("firmware_minor", int(fw_minor)) \
            .field("capacity_ah", int(capacity_ah)) \
            .field("manufacturer_id", int(manufacturer_id))

        return point

    def parse_victron_cells(self, data: bytes, module_id: int) -> Optional[Point]:
        """Parse Victron BMS cell extrema (0x370-0x373) - Little-Endian"""
        if len(data) < 8:
            return None

        # Little-Endian decoding
        max_temp = int.from_bytes(data[0:2], 'little')  # °C (NO scaling)
        min_temp = int.from_bytes(data[2:4], 'little')  # °C (NO scaling)
        max_voltage = int.from_bytes(data[4:6], 'little')  # mV
        min_voltage = int.from_bytes(data[6:8], 'little')  # mV

        # Skip if all zeros (empty module slot)
        if max_temp == 0 and min_temp == 0 and max_voltage == 0 and min_voltage == 0:
            return None

        point = Point("victron_cells") \
            .tag("source", "victron_bms") \
            .tag("module_id", str(module_id)) \
            .field("max_temp_c", int(max_temp)) \
            .field("min_temp_c", int(min_temp)) \
            .field("max_voltage_mv", int(max_voltage)) \
            .field("min_voltage_mv", int(min_voltage)) \
            .field("temp_delta_c", int(max_temp - min_temp)) \
            .field("voltage_delta_mv", int(max_voltage - min_voltage))

        return point

    def process_can_message(self, msg: can.Message):
        """Process a single CAN message and write to InfluxDB"""
        self.msg_count += 1

        point = None

        # Parse based on CAN ID
        if msg.arbitration_id == CAN_ID_INVERTER_TELEMETRY:
            point = self.parse_inverter_telemetry(msg.data)
        elif msg.arbitration_id == CAN_ID_BATTERY_SOC:
            point = self.parse_battery_soc(msg.data)
        elif msg.arbitration_id == CAN_ID_BATTERY_TEMP:
            point = self.parse_battery_temp(msg.data)
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
        elif msg.arbitration_id == CAN_ID_VICTRON_LIMITS:
            point = self.parse_victron_limits(msg.data)
        elif msg.arbitration_id == CAN_ID_VICTRON_SOC:
            point = self.parse_victron_soc(msg.data)
        elif msg.arbitration_id == CAN_ID_VICTRON_PACK:
            point = self.parse_victron_pack(msg.data)
        elif msg.arbitration_id == CAN_ID_VICTRON_ALARMS:
            point = self.parse_victron_alarms(msg.data)
        elif msg.arbitration_id == CAN_ID_VICTRON_CHARACTERISTICS:
            point = self.parse_victron_characteristics(msg.data)
        elif msg.arbitration_id == CAN_ID_VICTRON_CELLS_0:
            point = self.parse_victron_cells(msg.data, 0)
        elif msg.arbitration_id == CAN_ID_VICTRON_CELLS_1:
            point = self.parse_victron_cells(msg.data, 1)
        elif msg.arbitration_id == CAN_ID_VICTRON_CELLS_2:
            point = self.parse_victron_cells(msg.data, 2)
        elif msg.arbitration_id == CAN_ID_VICTRON_CELLS_3:
            point = self.parse_victron_cells(msg.data, 3)

        # Write to InfluxDB
        if point:
            try:
                self.write_api.write(bucket=self.influx_bucket, record=point)
                self.write_count += 1
            except Exception as e:
                self.error_count += 1
                logger.error(f"Failed to write to InfluxDB: {e}")

    def print_statistics(self):
        """Print periodic statistics"""
        now = time.time()
        elapsed = now - self.last_stats_time

        if elapsed >= 10.0:  # Every 10 seconds
            msg_rate = self.msg_count / elapsed
            write_rate = self.write_count / elapsed

            logger.info(
                f"Stats: {self.msg_count} msgs ({msg_rate:.1f}/s), "
                f"{self.write_count} writes ({write_rate:.1f}/s), "
                f"{self.error_count} errors"
            )

            self.msg_count = 0
            self.write_count = 0
            self.error_count = 0
            self.last_stats_time = now

    def run(self):
        """Main loop: read CAN messages and log to InfluxDB"""
        self.running = True
        logger.info("Starting telemetry logger main loop")

        try:
            while self.running:
                # Read CAN message (blocking with timeout)
                msg = self.bus.recv(timeout=1.0)

                if msg:
                    self.process_can_message(msg)

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
        if self.bus:
            self.bus.shutdown()

        logger.info("Cleanup complete")

    def signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        logger.info(f"Received signal {signum}")
        self.running = False


def main():
    """Main entry point"""
    can_interface = os.getenv("CAN_INTERFACE", "can0")

    logger.info("=== Nissan Leaf CAN Telemetry Logger ===")
    logger.info(f"CAN Interface: {can_interface}")

    try:
        logger_service = CANTelemetryLogger(can_interface)

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
