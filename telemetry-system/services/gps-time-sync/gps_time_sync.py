#!/usr/bin/env python3
"""
GPS Time Sync Service
Reads time from GPS and syncs system clock (no network needed)
"""

import serial
import pynmea2
import subprocess
import time
import logging
from datetime import datetime, timezone

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)
logger = logging.getLogger(__name__)

GPS_DEVICE = "/dev/ttyACM0"
CHECK_INTERVAL = 60  # Check every 60 seconds

def set_system_time(gps_datetime):
    """Set system time from GPS datetime"""
    try:
        # Format: YYYY-MM-DD HH:MM:SS
        time_str = gps_datetime.strftime('%Y-%m-%d %H:%M:%S')

        # Use timedatectl to set time
        subprocess.run(
            ['sudo', 'timedatectl', 'set-time', time_str],
            check=True,
            capture_output=True
        )
        logger.info(f"System time set to: {time_str} (from GPS)")
        return True
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to set system time: {e}")
        return False

def read_gps_time(device_path, timeout=10):
    """Read time from GPS device"""
    try:
        with serial.Serial(device_path, baudrate=9600, timeout=1) as ser:
            logger.info(f"Reading GPS time from {device_path}")
            start_time = time.time()

            while (time.time() - start_time) < timeout:
                try:
                    line = ser.readline().decode('ascii', errors='ignore').strip()

                    if line.startswith('$'):
                        try:
                            msg = pynmea2.parse(line)

                            # Look for RMC or GGA messages with valid time
                            if hasattr(msg, 'timestamp') and msg.timestamp:
                                if hasattr(msg, 'datestamp') and msg.datestamp:
                                    # Combine date and time
                                    gps_datetime = datetime.combine(
                                        msg.datestamp,
                                        msg.timestamp,
                                        tzinfo=timezone.utc
                                    )
                                    return gps_datetime
                        except pynmea2.ParseError:
                            continue

                except UnicodeDecodeError:
                    continue

    except serial.SerialException as e:
        logger.error(f"GPS device error: {e}")

    return None

def is_time_accurate():
    """Check if system time is reasonably accurate (not default/old)"""
    current_time = datetime.now(timezone.utc)

    # If system thinks it's before 2024, time is definitely wrong
    if current_time.year < 2024:
        return False

    # If systemd-timesyncd has synced via NTP, trust it
    try:
        result = subprocess.run(
            ['timedatectl', 'show', '--property=NTPSynchronized', '--value'],
            capture_output=True,
            text=True
        )
        if result.returncode == 0 and result.stdout.strip() == 'yes':
            logger.info("Time already synced via NTP")
            return True
    except:
        pass

    return False

def main():
    logger.info("GPS Time Sync Service starting...")

    # Wait for GPS device to be available
    max_wait = 30
    wait_count = 0
    while not subprocess.run(['test', '-e', GPS_DEVICE], capture_output=True).returncode == 0:
        if wait_count >= max_wait:
            logger.error(f"GPS device {GPS_DEVICE} not found after {max_wait}s, exiting")
            return
        time.sleep(1)
        wait_count += 1

    logger.info(f"GPS device {GPS_DEVICE} found")

    sync_count = 0

    while True:
        try:
            # Check if time is already accurate (via NTP)
            if is_time_accurate():
                logger.info(f"System time is accurate, sleeping {CHECK_INTERVAL}s")
                time.sleep(CHECK_INTERVAL)
                continue

            # Try to get time from GPS
            gps_time = read_gps_time(GPS_DEVICE, timeout=30)

            if gps_time:
                # Check if GPS time is reasonable
                if gps_time.year >= 2024:
                    if set_system_time(gps_time):
                        sync_count += 1
                        logger.info(f"Time sync successful (count: {sync_count})")
                else:
                    logger.warning(f"GPS time seems wrong: {gps_time}")
            else:
                logger.warning("Could not read valid time from GPS")

        except KeyboardInterrupt:
            logger.info("Shutting down...")
            break
        except Exception as e:
            logger.error(f"Error in main loop: {e}")

        # Sleep before next check
        time.sleep(CHECK_INTERVAL)

if __name__ == "__main__":
    main()
