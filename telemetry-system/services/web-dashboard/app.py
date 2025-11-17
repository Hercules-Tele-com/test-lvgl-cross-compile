#!/usr/bin/env python3
"""
Nissan Leaf Web Dashboard
Flask web server with REST API and WebSocket support for real-time vehicle monitoring
"""

import os
import sys
import logging
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional
from flask import Flask, jsonify, request, send_from_directory
from flask_cors import CORS
from flask_socketio import SocketIO, emit
from influxdb_client import InfluxDBClient
from influxdb_client.client.query_api import QueryApi
import threading
import time

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

if not INFLUX_TOKEN:
    raise ValueError("INFLUX_WEB_TOKEN environment variable not set")

# Global InfluxDB client
influx_client: Optional[InfluxDBClient] = None
query_api: Optional[QueryApi] = None


def init_influxdb():
    """Initialize InfluxDB client"""
    global influx_client, query_api
    try:
        logger.info(f"Connecting to InfluxDB: {INFLUX_URL}")
        influx_client = InfluxDBClient(
            url=INFLUX_URL,
            token=INFLUX_TOKEN,
            org=INFLUX_ORG
        )
        query_api = influx_client.query_api()

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
        return False


def query_latest_value(measurement: str, field: str, tag_filters: str = "") -> Optional[Dict[str, Any]]:
    """Query the latest value for a measurement/field"""
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
    """Query all fields for a measurement"""
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

        for table in tables:
            for record in table.records:
                result = {"time": record.get_time().isoformat()}
                # Add all fields
                for key, value in record.values.items():
                    if not key.startswith("_") and key not in ["result", "table", "time"]:
                        result[key] = value
                return result

        return {}
    except Exception as e:
        logger.error(f"Query failed for {measurement}: {e}")
        return {}


def query_historical_data(measurement: str, field: str, duration: str = "24h", window: str = "5m") -> List[Dict[str, Any]]:
    """Query historical data with aggregation"""
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
        status = {
            "timestamp": datetime.utcnow().isoformat(),
            "inverter": query_all_fields("inverter"),
            "battery_soc": query_all_fields("battery_soc"),
            "battery_temp": query_all_fields("battery_temp"),
            "vehicle_speed": query_all_fields("vehicle_speed"),
            "motor_rpm": query_all_fields("motor_rpm"),
            "charger": query_all_fields("charger"),
            "gps_position": query_all_fields("gps_position"),
            "gps_velocity": query_all_fields("gps_velocity"),
            "body_temp": query_all_fields("body_temp"),
            "body_voltage": query_all_fields("body_voltage"),
            # Victron BMS data
            "victron_pack": query_all_fields("victron_pack"),
            "victron_soc": query_all_fields("victron_soc"),
            "victron_limits": query_all_fields("victron_limits"),
            "victron_characteristics": query_all_fields("victron_characteristics"),
            "victron_alarms": query_all_fields("victron_alarms")
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
