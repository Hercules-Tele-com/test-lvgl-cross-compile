#!/usr/bin/env python3
"""
Nissan Leaf Web Dashboard
Flask web server with REST API and WebSocket support for real-time vehicle monitoring
"""

import os
import sys
import logging
from datetime import datetime, timedelta, timezone
from typing import Dict, Any, List, Optional
from flask import Flask, jsonify, request, send_from_directory
from flask_cors import CORS
from flask_socketio import SocketIO, emit
from influxdb_client import InfluxDBClient
from influxdb_client.client.query_api import QueryApi
import threading
import time
import csv
import io
from trip_tracker import TripTracker

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)
logger = logging.getLogger(__name__)

# Flask app
app = Flask(__name__, static_folder='static', static_url_path='')
app.config['SECRET_KEY'] = os.getenv('SECRET_KEY', 'leaf-telemetry-secret')
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

# InfluxDB configuration
INFLUX_URL = os.getenv("INFLUX_URL", "http://localhost:8086")
INFLUX_ORG = os.getenv("INFLUX_ORG", "leaf-telemetry")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET", "leaf-data")
INFLUX_TOKEN = os.getenv("INFLUX_WEB_TOKEN", "")

# Global InfluxDB client
influx_client: Optional[InfluxDBClient] = None
query_api: Optional[QueryApi] = None
trip_tracker: Optional[TripTracker] = None


def init_influxdb():
    """Initialize InfluxDB client (optional - dashboard works without it)"""
    global influx_client, query_api, trip_tracker

    if not INFLUX_TOKEN:
        logger.warning("INFLUX_WEB_TOKEN not set - dashboard will run without InfluxDB (no data logging)")
        return False

    try:
        logger.info(f"Connecting to InfluxDB: {INFLUX_URL}")
        influx_client = InfluxDBClient(
            url=INFLUX_URL,
            token=INFLUX_TOKEN,
            org=INFLUX_ORG
        )
        query_api = influx_client.query_api()

        # Initialize trip tracker
        trip_tracker = TripTracker(influx_client, INFLUX_BUCKET, INFLUX_ORG)
        logger.info("Trip tracker initialized")

        # Test connection
        health = influx_client.health()
        if health.status == "pass":
            logger.info("InfluxDB connection successful")
            return True
        else:
            logger.error(f"InfluxDB health check failed: {health.message}")
            return False
    except Exception as e:
        logger.error(f"Failed to connect to InfluxDB: {e}")
        logger.warning("Dashboard will continue without InfluxDB")
        return False


def query_latest_value(measurement: str, field: str, tag_filters: str = "") -> Optional[Dict[str, Any]]:
    """Query the latest value for a measurement/field"""
    if not query_api:
        return None

    try:
        flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -1h)
  |> filter(fn: (r) => r["_measurement"] == "{measurement}")
  |> filter(fn: (r) => r["_field"] == "{field}")
  {tag_filters}
  |> last()
        '''

        tables = query_api.query(flux_query)

        for table in tables:
            for record in table.records:
                return {
                    "value": record.get_value(),
                    "time": record.get_time().isoformat(),
                    "measurement": measurement,
                    "field": field
                }

        return None
    except Exception as e:
        logger.error(f"Query failed for {measurement}.{field}: {e}")
        return None


def query_all_fields(measurement: str, tag_filters: str = "") -> Dict[str, Any]:
    """Query all fields for a measurement - merges multiple pivot rows"""
    if not query_api:
        return {}

    try:
        flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -1h)
  |> filter(fn: (r) => r["_measurement"] == "{measurement}")
  {tag_filters}
  |> last()
  |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
        '''

        tables = query_api.query(flux_query)

        # Merge all rows into a single result (handles multiple update frequencies)
        result = {}
        latest_time = None

        for table in tables:
            for record in table.records:
                # Track the latest timestamp
                record_time = record.get_time()
                if latest_time is None or record_time > latest_time:
                    latest_time = record_time

                # Merge all fields
                for key, value in record.values.items():
                    if not key.startswith("_") and key not in ["result", "table", "time"]:
                        # Only update if value is not None
                        if value is not None:
                            result[key] = value

        # Add the latest timestamp
        if latest_time:
            result["time"] = latest_time.isoformat()

        return result
    except Exception as e:
        logger.error(f"Query failed for {measurement}: {e}")
        return {}


def query_historical_data(measurement: str, field: str, duration: str = "24h", window: str = "5m") -> List[Dict[str, Any]]:
    """Query historical data with aggregation"""
    if not query_api:
        return []

    try:
        flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -{duration})
  |> filter(fn: (r) => r["_measurement"] == "{measurement}")
  |> filter(fn: (r) => r["_field"] == "{field}")
  |> aggregateWindow(every: {window}, fn: mean, createEmpty: false)
  |> yield(name: "mean")
        '''

        tables = query_api.query(flux_query)

        data_points = []
        for table in tables:
            for record in table.records:
                data_points.append({
                    "time": record.get_time().isoformat(),
                    "value": record.get_value()
                })

        return data_points
    except Exception as e:
        logger.error(f"Historical query failed for {measurement}.{field}: {e}")
        return []


def get_component_status() -> Dict[str, Any]:
    """Check online/offline status of components based on last message time"""
    if not query_api:
        return {}

    components = {
        "leaf_inverter": {"measurement": "inverter", "name": "Leaf Inverter"},
        "leaf_battery": {"measurement": "battery_soc", "name": "Leaf Battery ECU"},
        "victron_bms": {"measurement": "victron_pack", "name": "Victron BMS"},
        "usb_gps": {"measurement": "usb_gps_position", "name": "USB GPS"},
        "can_gps": {"measurement": "gps_position", "name": "CAN GPS"},
        "charger": {"measurement": "charger", "name": "Charger"}
    }

    status = {}
    timeout_seconds = 5  # Consider offline if no message for 5 seconds

    for comp_id, comp_info in components.items():
        try:
            # Query last message time
            flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -1m)
  |> filter(fn: (r) => r["_measurement"] == "{comp_info['measurement']}")
  |> last()
  |> limit(n: 1)
            '''

            tables = query_api.query(flux_query)

            online = False
            last_seen = None

            for table in tables:
                for record in table.records:
                    last_time = record.get_time()
                    if last_time:
                        # Check if message is recent
                        age_seconds = (datetime.now(timezone.utc) - last_time).total_seconds()
                        online = age_seconds < timeout_seconds
                        last_seen = last_time.isoformat()
                    break
                break

            status[comp_id] = {
                "name": comp_info['name'],
                "online": online,
                "last_seen": last_seen
            }

        except Exception as e:
            logger.error(f"Failed to check status for {comp_id}: {e}")
            status[comp_id] = {
                "name": comp_info['name'],
                "online": False,
                "last_seen": None
            }

    return status


# ============================================================================
# REST API ENDPOINTS
# ============================================================================

@app.route('/')
def index():
    """Serve main dashboard page"""
    return send_from_directory('static', 'index.html')


@app.route('/api/status')
def api_status():
    """Get current vehicle status (all metrics)"""
    try:
        # Query latest values for all measurements
        components = get_component_status()

        # Prefer USB GPS over CAN GPS if available
        usb_gps_position = query_all_fields("usb_gps_position")
        usb_gps_velocity = query_all_fields("usb_gps_velocity")
        can_gps_position = query_all_fields("gps_position")
        can_gps_velocity = query_all_fields("gps_velocity")

        # Use USB GPS if online, otherwise fall back to CAN GPS
        gps_position = usb_gps_position if usb_gps_position else can_gps_position
        gps_velocity = usb_gps_velocity if usb_gps_velocity else can_gps_velocity

        status = {
            "timestamp": datetime.utcnow().isoformat(),
            "components": components,
            "gps_source": "usb" if usb_gps_position else ("can" if can_gps_position else "none"),
            "inverter": query_all_fields("inverter"),
            "battery_soc": query_all_fields("battery_soc"),
            "battery_temp": query_all_fields("battery_temp"),
            "vehicle_speed": query_all_fields("vehicle_speed"),
            "motor_rpm": query_all_fields("motor_rpm"),
            "charger": query_all_fields("charger"),
            "gps_position": gps_position,
            "gps_velocity": gps_velocity,
            "body_temp": query_all_fields("body_temp"),
            "body_voltage": query_all_fields("body_voltage"),
            # Victron BMS data
            "victron_pack": query_all_fields("victron_pack"),
            "victron_soc": query_all_fields("victron_soc"),
            "victron_limits": query_all_fields("victron_limits"),
            "victron_characteristics": query_all_fields("victron_characteristics"),
            "victron_alarms": query_all_fields("victron_alarms"),
            "victron_cells": query_all_fields("victron_cells")
        }

        return jsonify(status)
    except Exception as e:
        logger.error(f"Error in /api/status: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/historical/<measurement>/<field>')
def api_historical(measurement: str, field: str):
    """Get historical data for a specific measurement/field"""
    try:
        duration = request.args.get('duration', '24h')
        window = request.args.get('window', '5m')

        data = query_historical_data(measurement, field, duration, window)

        return jsonify({
            "measurement": measurement,
            "field": field,
            "duration": duration,
            "window": window,
            "data": data
        })
    except Exception as e:
        logger.error(f"Error in /api/historical: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/summary')
def api_summary():
    """Get quick summary of key metrics"""
    try:
        summary = {}

        # Battery SOC
        battery_soc = query_all_fields("battery_soc")
        if battery_soc:
            summary["battery"] = {
                "soc_percent": battery_soc.get("soc_percent"),
                "voltage": battery_soc.get("pack_voltage"),
                "current": battery_soc.get("pack_current")
            }

        # Speed
        speed = query_all_fields("vehicle_speed")
        if speed:
            summary["speed_kmh"] = speed.get("speed_kmh")

        # Motor
        motor = query_all_fields("motor_rpm")
        if motor:
            summary["motor_rpm"] = motor.get("rpm")

        # Charging
        charger = query_all_fields("charger")
        if charger:
            summary["charging"] = charger.get("charging_flag") == 1

        # Temperatures
        battery_temp = query_all_fields("battery_temp")
        if battery_temp:
            summary["battery_temp_avg"] = battery_temp.get("temp_avg")

        inverter = query_all_fields("inverter")
        if inverter:
            summary["inverter_temp"] = inverter.get("temp_inverter")
            summary["motor_temp"] = inverter.get("temp_motor")

        return jsonify(summary)
    except Exception as e:
        logger.error(f"Error in /api/summary: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/victron/status')
def api_victron_status():
    """Get current Victron BMS status (all metrics)"""
    try:
        status = {
            "timestamp": datetime.utcnow().isoformat(),
            "pack": query_all_fields("victron_pack"),
            "soc": query_all_fields("victron_soc"),
            "limits": query_all_fields("victron_limits"),
            "characteristics": query_all_fields("victron_characteristics"),
            "alarms": query_all_fields("victron_alarms")
        }
        return jsonify(status)
    except Exception as e:
        logger.error(f"Error in /api/victron/status: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/victron/cells')
def api_victron_cells():
    """Get cell extrema for all modules"""
    try:
        # Query cell data for all modules (0-3)
        cells = {}
        for module_id in range(4):
            flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -1h)
  |> filter(fn: (r) => r["_measurement"] == "victron_cells")
  |> filter(fn: (r) => r["module_id"] == "{module_id}")
  |> last()
  |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
            '''

            tables = query_api.query(flux_query)
            for table in tables:
                for record in table.records:
                    cells[f"module_{module_id}"] = {
                        "max_temp_c": record.values.get("max_temp_c"),
                        "min_temp_c": record.values.get("min_temp_c"),
                        "max_voltage_mv": record.values.get("max_voltage_mv"),
                        "min_voltage_mv": record.values.get("min_voltage_mv"),
                        "temp_delta_c": record.values.get("temp_delta_c"),
                        "voltage_delta_mv": record.values.get("voltage_delta_mv"),
                        "time": record.get_time().isoformat()
                    }

        return jsonify({
            "timestamp": datetime.utcnow().isoformat(),
            "cells": cells
        })
    except Exception as e:
        logger.error(f"Error in /api/victron/cells: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/trips/current')
def api_trips_current():
    """Get current ongoing trip status"""
    try:
        if not trip_tracker:
            return jsonify({"error": "Trip tracker not initialized"}), 500

        current_trip = trip_tracker.get_current_trip_status()
        return jsonify(current_trip if current_trip else {"status": "no_active_trip"})
    except Exception as e:
        logger.error(f"Error in /api/trips/current: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/trips/recent')
def api_trips_recent():
    """Get recent completed trips"""
    try:
        if not trip_tracker:
            return jsonify({"error": "Trip tracker not initialized"}), 500

        limit = int(request.args.get('limit', 10))
        trips = trip_tracker.get_recent_trips(limit)
        return jsonify({"trips": trips})
    except Exception as e:
        logger.error(f"Error in /api/trips/recent: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/gps/track')
def api_gps_track():
    """Get GPS track points for time range"""
    try:
        start = request.args.get('start', '-1h')
        end = request.args.get('end', 'now()')

        flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: {start}, stop: {end})
  |> filter(fn: (r) => r["_measurement"] == "gps_position")
  |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> sort(columns: ["_time"])
        '''

        tables = query_api.query(flux_query)
        track_points = []

        for table in tables:
            for record in table.records:
                track_points.append({
                    "time": record.get_time().isoformat(),
                    "latitude": record.values.get("latitude"),
                    "longitude": record.values.get("longitude")
                })

        return jsonify({"track_points": track_points})
    except Exception as e:
        logger.error(f"Error in /api/gps/track: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/export/csv')
def api_export_csv():
    """Export data to CSV"""
    try:
        start = request.args.get('start', '-24h')
        end = request.args.get('end', 'now()')
        measurement = request.args.get('measurement', 'victron_pack')

        flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: {start}, stop: {end})
  |> filter(fn: (r) => r["_measurement"] == "{measurement}")
  |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> sort(columns: ["_time"])
        '''

        tables = query_api.query(flux_query)

        # Create CSV in memory
        output = io.StringIO()
        writer = None

        for table in tables:
            for record in table.records:
                if writer is None:
                    # Create header from first record
                    fieldnames = ['timestamp'] + [k for k in record.values.keys()
                                                   if not k.startswith('_') and k not in ['result', 'table']]
                    writer = csv.DictWriter(output, fieldnames=fieldnames)
                    writer.writeheader()

                # Write data row
                row = {'timestamp': record.get_time().isoformat()}
                row.update({k: v for k, v in record.values.items()
                            if not k.startswith('_') and k not in ['result', 'table']})
                writer.writerow(row)

        # Return CSV file
        output.seek(0)
        return output.getvalue(), 200, {
            'Content-Type': 'text/csv',
            'Content-Disposition': f'attachment; filename={measurement}_export.csv'
        }
    except Exception as e:
        logger.error(f"Error in /api/export/csv: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/export/json')
def api_export_json():
    """Export data to JSON"""
    try:
        start = request.args.get('start', '-24h')
        end = request.args.get('end', 'now()')
        measurement = request.args.get('measurement', 'victron_pack')

        flux_query = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: {start}, stop: {end})
  |> filter(fn: (r) => r["_measurement"] == "{measurement}")
  |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> sort(columns: ["_time"])
        '''

        tables = query_api.query(flux_query)
        data_points = []

        for table in tables:
            for record in table.records:
                point = {'timestamp': record.get_time().isoformat()}
                point.update({k: v for k, v in record.values.items()
                              if not k.startswith('_') and k not in ['result', 'table']})
                data_points.append(point)

        return jsonify({
            "measurement": measurement,
            "start": start,
            "end": end,
            "count": len(data_points),
            "data": data_points
        })
    except Exception as e:
        logger.error(f"Error in /api/export/json: {e}")
        return jsonify({"error": str(e)}), 500


# ============================================================================
# WEBSOCKET EVENTS
# ============================================================================

@socketio.on('connect')
def handle_connect():
    """Handle client connection"""
    logger.info(f"Client connected: {request.sid}")
    emit('connection_response', {'status': 'connected'})


@socketio.on('disconnect')
def handle_disconnect():
    """Handle client disconnection"""
    logger.info(f"Client disconnected: {request.sid}")


@socketio.on('subscribe_realtime')
def handle_subscribe():
    """Client subscribes to real-time updates"""
    logger.info(f"Client subscribed to real-time: {request.sid}")


def broadcast_realtime_data():
    """Broadcast real-time data to all connected clients"""
    while True:
        try:
            # Query current status
            status = {
                "timestamp": datetime.utcnow().isoformat(),
                "battery_soc": query_all_fields("battery_soc"),
                "vehicle_speed": query_all_fields("vehicle_speed"),
                "motor_rpm": query_all_fields("motor_rpm"),
                "inverter": query_all_fields("inverter"),
                "charger": query_all_fields("charger"),
                "battery_temp": query_all_fields("battery_temp"),
                # Victron BMS real-time data
                "victron_pack": query_all_fields("victron_pack"),
                "victron_soc": query_all_fields("victron_soc"),
                "victron_limits": query_all_fields("victron_limits"),
                "victron_characteristics": query_all_fields("victron_characteristics")
            }

            # Update trip tracker with latest data
            if trip_tracker and status.get("vehicle_speed") and status.get("victron_soc") and status.get("victron_pack"):
                speed = status["vehicle_speed"].get("speed_kmh", 0.0)
                soc = status["victron_soc"].get("soc_percent", 0)
                power = status["victron_pack"].get("power_kw", 0.0)
                trip_tracker.update(speed, soc, power)

            # Broadcast to all connected clients
            socketio.emit('realtime_update', status)

        except Exception as e:
            logger.error(f"Error broadcasting realtime data: {e}")

        # Update every 2 seconds
        time.sleep(2)


# ============================================================================
# MAIN
# ============================================================================

def main():
    """Main entry point"""
    logger.info("=== Nissan Leaf Web Dashboard ===")

    # Initialize InfluxDB
    if not init_influxdb():
        logger.error("Failed to initialize InfluxDB")
        return 1

    # Start real-time broadcast thread
    broadcast_thread = threading.Thread(target=broadcast_realtime_data, daemon=True)
    broadcast_thread.start()

    # Get port from environment
    port = int(os.getenv("WEB_PORT", "8080"))

    logger.info(f"Starting web server on port {port}")
    logger.info(f"Dashboard available at: http://localhost:{port}")

    # Run Flask app with SocketIO
    socketio.run(
        app,
        host='0.0.0.0',
        port=port,
        debug=False,
        allow_unsafe_werkzeug=True
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
