#!/usr/bin/env python3
"""
USB GPS Reader Service
Reads NMEA data from U-Blox USB GPS and writes to InfluxDB
"""

import os
import sys
import serial
import time
import logging
from datetime import datetime
from typing import Optional
import pynmea2
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)


class USBGPSReader:
    """USB GPS to InfluxDB reader"""

    def __init__(self, device_path: str = "/dev/ttyACM0"):
        self.device_path = device_path
        self.serial_port: Optional[serial.Serial] = None
        self.running = False

        # InfluxDB configuration (optional)
        self.influx_url = os.getenv("INFLUX_URL", "http://localhost:8086")
        self.influx_org = os.getenv("INFLUX_ORG", "leaf-telemetry")
        self.influx_bucket = os.getenv("INFLUX_BUCKET", "leaf-data")
        self.influx_token = os.getenv("INFLUX_LOGGER_TOKEN", "")
        self.influx_enabled = bool(self.influx_token)

        self.influx_client: Optional[InfluxDBClient] = None
        self.write_api = None

        # Statistics
        self.msg_count = 0
        self.valid_fix_count = 0
        self.write_count = 0
        self.last_stats_time = time.time()

    def init(self) -> bool:
        """Initialize GPS serial port and InfluxDB connection"""
        try:
            # Initialize serial port
            logger.info(f"Opening GPS device: {self.device_path}")
            self.serial_port = serial.Serial(
                port=self.device_path,
                baudrate=9600,
                timeout=1.0
            )
            logger.info("USB GPS serial port opened successfully")

            # Initialize InfluxDB (optional)
            if self.influx_enabled:
                try:
                    logger.info(f"Connecting to InfluxDB: {self.influx_url}")
                    self.influx_client = InfluxDBClient(
                        url=self.influx_url,
                        token=self.influx_token,
                        org=self.influx_org
                    )
                    self.write_api = self.influx_client.write_api(write_options=SYNCHRONOUS)

                    health = self.influx_client.health()
                    if health.status == "pass":
                        logger.info("InfluxDB connection successful")
                    else:
                        logger.warning(f"InfluxDB health check failed: {health.message}")
                        self.influx_enabled = False
                except Exception as e:
                    logger.warning(f"InfluxDB connection failed: {e}")
                    logger.info("Continuing without InfluxDB - GPS data will not be logged")
                    self.influx_enabled = False
            else:
                logger.info("InfluxDB disabled (no token configured)")

            return True

        except Exception as e:
            logger.error(f"Initialization failed: {e}")
            return False

    def parse_nmea_sentence(self, line: str):
        """Parse NMEA sentence and write to InfluxDB"""
        try:
            msg = pynmea2.parse(line)
            self.msg_count += 1

            # Process GGA (position) sentences
            if isinstance(msg, pynmea2.types.talker.GGA):
                if msg.gps_qual > 0:  # Valid fix
                    self.valid_fix_count += 1

                    point = Point("usb_gps_position") \
                        .tag("source", "usb_gps") \
                        .field("latitude", float(msg.latitude)) \
                        .field("longitude", float(msg.longitude)) \
                        .field("altitude", float(msg.altitude)) \
                        .field("num_sats", int(msg.num_sats)) \
                        .field("gps_qual", int(msg.gps_qual)) \
                        .field("horizontal_dil", float(msg.horizontal_dil) if msg.horizontal_dil else 0)

                    if self.influx_enabled and self.write_api:
                        self.write_api.write(bucket=self.influx_bucket, record=point)
                        self.write_count += 1

                    # Log position periodically
                    if self.valid_fix_count % 10 == 0:
                        logger.info(f"GPS Position: {msg.latitude:.6f}, {msg.longitude:.6f}, "
                                  f"Alt: {msg.altitude}m, Sats: {msg.num_sats}")

            # Process RMC (velocity) sentences
            elif isinstance(msg, pynmea2.types.talker.RMC):
                if msg.status == 'A':  # Active/valid
                    point = Point("usb_gps_velocity") \
                        .tag("source", "usb_gps") \
                        .field("speed_kmh", float(msg.spd_over_grnd * 1.852) if msg.spd_over_grnd else 0) \
                        .field("true_course", float(msg.true_course) if msg.true_course else 0)

                    if self.influx_enabled and self.write_api:
                        self.write_api.write(bucket=self.influx_bucket, record=point)

        except pynmea2.ParseError as e:
            # Ignore parse errors (incomplete sentences, etc.)
            pass
        except Exception as e:
            logger.error(f"Error parsing NMEA: {e}")

    def print_statistics(self):
        """Print periodic statistics"""
        now = time.time()
        elapsed = now - self.last_stats_time

        if elapsed >= 30.0:  # Every 30 seconds
            msg_rate = self.msg_count / elapsed
            fix_rate = self.valid_fix_count / elapsed

            logger.info(
                f"Stats: {self.msg_count} NMEA messages ({msg_rate:.1f}/s), "
                f"{self.valid_fix_count} valid fixes ({fix_rate:.1f}/s), "
                f"{self.write_count} InfluxDB writes"
            )

            self.msg_count = 0
            self.valid_fix_count = 0
            self.write_count = 0
            self.last_stats_time = now

    def run(self):
        """Main loop: read NMEA sentences from USB GPS"""
        self.running = True
        logger.info("Starting USB GPS reader main loop")

        try:
            while self.running:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    try:
                        line = self.serial_port.readline().decode('ascii', errors='ignore').strip()
                        if line.startswith('$'):
                            self.parse_nmea_sentence(line)
                    except Exception as e:
                        logger.error(f"Error reading serial data: {e}")

                self.print_statistics()
                time.sleep(0.01)  # Small delay to prevent CPU spinning

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

        if self.serial_port:
            self.serial_port.close()

        if self.influx_enabled:
            if self.write_api:
                self.write_api.close()
            if self.influx_client:
                self.influx_client.close()

        logger.info("Cleanup complete")


def main():
    """Main entry point"""
    device_path = os.getenv("GPS_DEVICE", "/dev/ttyACM0")

    logger.info("=== USB GPS Reader Service ===")
    logger.info(f"GPS Device: {device_path}")

    try:
        gps_reader = USBGPSReader(device_path)

        if not gps_reader.init():
            logger.error("Failed to initialize USB GPS reader")
            return 1

        gps_reader.run()

        return 0

    except Exception as e:
        logger.error(f"Fatal error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
