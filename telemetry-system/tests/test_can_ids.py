#!/usr/bin/env python3
"""
Debug script to show what CAN IDs are actually being received on can1.
This helps identify if the CAN IDs match what we expect in the telemetry logger.
"""
import can
import sys
from collections import Counter
import time

def main():
    interface = sys.argv[1] if len(sys.argv) > 1 else 'can1'

    print(f"Monitoring {interface} for CAN messages...")
    print("Collecting data for 5 seconds...\n")

    try:
        bus = can.Bus(interface='socketcan', channel=interface, bitrate=250000)
    except Exception as e:
        print(f"Error opening CAN bus: {e}")
        return 1

    ids = Counter()
    start_time = time.time()

    # Collect for 5 seconds
    while time.time() - start_time < 5.0:
        msg = bus.recv(timeout=0.1)
        if msg is not None:
            ids[msg.arbitration_id] += 1

    print(f"\nReceived {sum(ids.values())} total messages")
    print(f"Unique CAN IDs: {len(ids)}\n")
    print("Top 20 CAN IDs by frequency:")
    print("-" * 40)

    for can_id, count in ids.most_common(20):
        print(f"  0x{can_id:03X}: {count:4d} messages  ({count/5:.1f} msg/sec)")

    bus.shutdown()
    return 0

if __name__ == "__main__":
    sys.exit(main())
