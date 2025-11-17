#!/usr/bin/env python3
"""
Mock CAN Data Generator
Generates realistic CAN messages for testing the telemetry system
"""

import can
import time
import random
import math
import sys
from datetime import datetime

# CAN IDs
CAN_ID_INVERTER_TELEMETRY = 0x1F2
CAN_ID_BATTERY_SOC = 0x1DB
CAN_ID_BATTERY_TEMP = 0x1DC
CAN_ID_VEHICLE_SPEED = 0x1D4
CAN_ID_MOTOR_RPM = 0x1DA
CAN_ID_CHARGER_STATUS = 0x390
CAN_ID_GPS_POSITION = 0x710
CAN_ID_GPS_VELOCITY = 0x711
CAN_ID_BODY_TEMP_SENSORS = 0x720
CAN_ID_BODY_VOLTAGE = 0x721


class MockVehicleState:
    """Simulated vehicle state"""

    def __init__(self):
        # Battery
        self.soc = 85.0  # %
        self.battery_voltage = 360.0  # V
        self.battery_current = 0.0  # A
        self.battery_temp = 25  # °C

        # Motion
        self.speed = 0.0  # km/h
        self.motor_rpm = 0
        self.direction = 0  # 0=stopped, 1=forward, 2=reverse

        # Power
        self.inverter_voltage = 360.0  # V
        self.inverter_current = 0.0  # A
        self.inverter_temp = 30  # °C
        self.motor_temp = 28  # °C

        # Charging
        self.charging = False
        self.charge_power = 0.0  # kW

        # GPS
        self.latitude = 51.5074  # London
        self.longitude = -0.1278
        self.gps_speed = 0.0
        self.heading = 0.0

        # Body
        self.voltage_12v = 13.8  # V
        self.ambient_temp = 22  # °C

        # Simulation state
        self.time = 0.0
        self.drive_mode = "idle"  # idle, accelerating, cruising, decelerating, charging

    def update(self, dt: float):
        """Update vehicle state for simulation"""
        self.time += dt

        # Determine drive mode based on time
        cycle_time = self.time % 60.0

        if cycle_time < 5:
            self.drive_mode = "idle"
        elif cycle_time < 15:
            self.drive_mode = "accelerating"
        elif cycle_time < 35:
            self.drive_mode = "cruising"
        elif cycle_time < 45:
            self.drive_mode = "decelerating"
        elif cycle_time < 50:
            self.drive_mode = "idle"
        else:
            self.drive_mode = "charging"

        # Update based on drive mode
        if self.drive_mode == "idle":
            self.speed = max(0, self.speed - 2 * dt)
            self.motor_rpm = 0
            self.direction = 0
            self.inverter_current = 0
            self.battery_current = 0
            self.charging = False

        elif self.drive_mode == "accelerating":
            self.speed = min(80, self.speed + 10 * dt)
            self.motor_rpm = int(self.speed * 150)
            self.direction = 1
            self.inverter_current = 100 + random.uniform(-10, 10)
            self.battery_current = self.inverter_current
            self.charging = False
            self.soc = max(0, self.soc - 0.01 * dt)

        elif self.drive_mode == "cruising":
            self.speed = 80 + random.uniform(-5, 5)
            self.motor_rpm = int(self.speed * 150)
            self.direction = 1
            self.inverter_current = 50 + random.uniform(-10, 10)
            self.battery_current = self.inverter_current
            self.charging = False
            self.soc = max(0, self.soc - 0.005 * dt)

        elif self.drive_mode == "decelerating":
            self.speed = max(0, self.speed - 15 * dt)
            self.motor_rpm = int(self.speed * 150)
            self.direction = 1
            self.inverter_current = -30 + random.uniform(-5, 5)  # Regen
            self.battery_current = self.inverter_current
            self.charging = False
            self.soc = min(100, self.soc + 0.002 * dt)

        elif self.drive_mode == "charging":
            self.speed = 0
            self.motor_rpm = 0
            self.direction = 0
            self.charging = True
            self.charge_power = 6.6  # kW
            self.inverter_current = 0
            self.battery_current = -18.3  # Charging current
            self.soc = min(100, self.soc + 0.05 * dt)

        # Update temperatures based on load
        load_factor = abs(self.inverter_current) / 100.0
        self.inverter_temp = 30 + load_factor * 40 + random.uniform(-2, 2)
        self.motor_temp = 28 + load_factor * 35 + random.uniform(-2, 2)
        self.battery_temp = 25 + load_factor * 15 + random.uniform(-1, 1)

        # Update GPS
        self.gps_speed = self.speed + random.uniform(-2, 2)
        self.heading = (self.heading + dt * 2) % 360

        # Update voltages
        self.battery_voltage = 360 + random.uniform(-5, 5)
        self.inverter_voltage = self.battery_voltage
        self.voltage_12v = 13.8 + random.uniform(-0.2, 0.2)


def pack_inverter_telemetry(state: MockVehicleState) -> bytes:
    """Pack inverter telemetry message"""
    voltage = int(state.inverter_voltage * 10)
    current = int(state.inverter_current * 10)
    temp_inv = int(state.inverter_temp)
    temp_motor = int(state.motor_temp)
    status = 0x01  # Active

    data = bytearray(8)
    data[0:2] = voltage.to_bytes(2, 'big')
    data[2:4] = current.to_bytes(2, 'big', signed=True)
    data[4] = temp_inv.to_bytes(1, 'big', signed=True)[0]
    data[5] = temp_motor.to_bytes(1, 'big', signed=True)[0]
    data[6] = status
    data[7] = 0

    return bytes(data)


def pack_battery_soc(state: MockVehicleState) -> bytes:
    """Pack battery SOC message"""
    soc = int(state.soc)
    gids = int(state.soc * 2.81)  # Approximate GIDS
    voltage = int(state.battery_voltage * 10)
    current = int(state.battery_current * 10)

    data = bytearray(8)
    data[0] = soc
    data[1:3] = gids.to_bytes(2, 'big')
    data[3:5] = voltage.to_bytes(2, 'big')
    data[5:7] = current.to_bytes(2, 'big', signed=True)
    data[7] = 0

    return bytes(data)


def pack_battery_temp(state: MockVehicleState) -> bytes:
    """Pack battery temperature message"""
    temp_avg = int(state.battery_temp)
    temp_max = temp_avg + random.randint(0, 3)
    temp_min = temp_avg - random.randint(0, 2)

    data = bytearray(8)
    data[0] = temp_max.to_bytes(1, 'big', signed=True)[0]
    data[1] = temp_min.to_bytes(1, 'big', signed=True)[0]
    data[2] = temp_avg.to_bytes(1, 'big', signed=True)[0]
    data[3] = 96  # 96 temp sensors

    return bytes(data)


def pack_vehicle_speed(state: MockVehicleState) -> bytes:
    """Pack vehicle speed message"""
    speed_kmh = int(state.speed * 100)
    speed_mph = int(state.speed * 0.621371 * 100)

    data = bytearray(8)
    data[0:2] = speed_kmh.to_bytes(2, 'big')
    data[2:4] = speed_mph.to_bytes(2, 'big')

    return bytes(data)


def pack_motor_rpm(state: MockVehicleState) -> bytes:
    """Pack motor RPM message"""
    rpm = int(state.motor_rpm)

    data = bytearray(8)
    data[0:2] = rpm.to_bytes(2, 'big', signed=True)
    data[2] = state.direction

    return bytes(data)


def pack_charger_status(state: MockVehicleState) -> bytes:
    """Pack charger status message"""
    charging = 1 if state.charging else 0
    current = int(state.charge_power / 0.36) if state.charging else 0
    voltage = 360 if state.charging else 0
    time_remaining = int((100 - state.soc) * 60 / 15) if state.charging else 0

    data = bytearray(8)
    data[0] = charging
    data[1:3] = int(current * 10).to_bytes(2, 'big')
    data[3:5] = int(voltage * 10).to_bytes(2, 'big')
    data[5:7] = time_remaining.to_bytes(2, 'big')

    return bytes(data)


def pack_gps_position(state: MockVehicleState) -> bytes:
    """Pack GPS position message"""
    lat = int(state.latitude * 1e7)
    lon = int(state.longitude * 1e7)

    data = bytearray(8)
    data[0:4] = lat.to_bytes(4, 'big', signed=True)
    data[4:8] = lon.to_bytes(4, 'big', signed=True)

    return bytes(data)


def pack_gps_velocity(state: MockVehicleState) -> bytes:
    """Pack GPS velocity message"""
    speed = int(state.gps_speed * 100)
    heading = int(state.heading * 100)
    pdop = 150  # 1.5

    data = bytearray(8)
    data[0:2] = speed.to_bytes(2, 'big')
    data[2:4] = heading.to_bytes(2, 'big')
    data[4:6] = pdop.to_bytes(2, 'big')

    return bytes(data)


def pack_body_temp(state: MockVehicleState) -> bytes:
    """Pack body temperature sensors message"""
    temp = int(state.ambient_temp * 10)

    data = bytearray(8)
    data[0:2] = temp.to_bytes(2, 'big', signed=True)
    data[2:4] = temp.to_bytes(2, 'big', signed=True)
    data[4:6] = temp.to_bytes(2, 'big', signed=True)
    data[6:8] = temp.to_bytes(2, 'big', signed=True)

    return bytes(data)


def pack_body_voltage(state: MockVehicleState) -> bytes:
    """Pack body voltage message"""
    v12 = int(state.voltage_12v * 100)
    v5 = 500  # 5.0V
    i12 = 250  # 2.5A

    data = bytearray(8)
    data[0:2] = v12.to_bytes(2, 'big')
    data[2:4] = v5.to_bytes(2, 'big')
    data[4:6] = i12.to_bytes(2, 'big')

    return bytes(data)


def main():
    """Main entry point"""
    can_interface = sys.argv[1] if len(sys.argv) > 1 else "can0"

    print(f"=== Mock CAN Data Generator ===")
    print(f"CAN Interface: {can_interface}")
    print(f"Press Ctrl+C to stop\n")

    try:
        # Create CAN bus
        bus = can.Bus(interface='socketcan', channel=can_interface, bitrate=500000)
        print(f"Connected to {can_interface}")

        # Create vehicle state
        state = MockVehicleState()

        last_time = time.time()

        while True:
            now = time.time()
            dt = now - last_time
            last_time = now

            # Update state
            state.update(dt)

            # Send messages
            messages = [
                (CAN_ID_INVERTER_TELEMETRY, pack_inverter_telemetry(state)),
                (CAN_ID_BATTERY_SOC, pack_battery_soc(state)),
                (CAN_ID_BATTERY_TEMP, pack_battery_temp(state)),
                (CAN_ID_VEHICLE_SPEED, pack_vehicle_speed(state)),
                (CAN_ID_MOTOR_RPM, pack_motor_rpm(state)),
                (CAN_ID_CHARGER_STATUS, pack_charger_status(state)),
                (CAN_ID_GPS_POSITION, pack_gps_position(state)),
                (CAN_ID_GPS_VELOCITY, pack_gps_velocity(state)),
                (CAN_ID_BODY_TEMP_SENSORS, pack_body_temp(state)),
                (CAN_ID_BODY_VOLTAGE, pack_body_voltage(state)),
            ]

            for can_id, data in messages:
                msg = can.Message(
                    arbitration_id=can_id,
                    data=data,
                    is_extended_id=False
                )
                bus.send(msg)

            # Print status
            print(f"\r[{datetime.now().strftime('%H:%M:%S')}] "
                  f"Mode: {state.drive_mode:12s} | "
                  f"Speed: {state.speed:5.1f} km/h | "
                  f"SOC: {state.soc:5.1f}% | "
                  f"Power: {state.battery_voltage * state.battery_current / 1000:6.1f} kW",
                  end='', flush=True)

            # Send every 100ms
            time.sleep(0.1)

    except KeyboardInterrupt:
        print("\n\nStopped by user")
    except Exception as e:
        print(f"\nError: {e}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
