#!/usr/bin/env python3
"""
Trip Statistics Tracker
Detects trips and calculates metrics (distance, energy, efficiency)
"""

import logging
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

logger = logging.getLogger(__name__)


class TripTracker:
    """Track vehicle trips and calculate statistics"""

    def __init__(self, influx_client: InfluxDBClient, bucket: str, org: str):
        self.influx_client = influx_client
        self.bucket = bucket
        self.org = org
        self.query_api = influx_client.query_api()
        self.write_api = influx_client.write_api(write_options=SYNCHRONOUS)

        # Trip state
        self.current_trip_id: Optional[str] = None
        self.trip_start_time: Optional[datetime] = None
        self.last_speed = 0.0
        self.last_update_time = datetime.utcnow()

        # Trip detection parameters
        self.MIN_TRIP_SPEED = 1.0  # km/h - minimum speed to consider moving
        self.TRIP_TIMEOUT = 300  # seconds - stopped for this long ends trip

    def update(self, speed_kmh: float, soc_percent: int, power_kw: float):
        """Update trip state with latest vehicle data"""
        now = datetime.utcnow()
        time_since_update = (now - self.last_update_time).total_seconds()

        # Detect trip start
        if self.current_trip_id is None and speed_kmh >= self.MIN_TRIP_SPEED:
            self._start_trip(now)

        # Detect trip end (stopped for timeout period)
        elif self.current_trip_id is not None and speed_kmh < self.MIN_TRIP_SPEED:
            if time_since_update >= self.TRIP_TIMEOUT:
                self._end_trip(now)

        # Update trip metrics if in a trip
        if self.current_trip_id is not None:
            self._update_trip_metrics(now, speed_kmh, soc_percent, power_kw)

        self.last_speed = speed_kmh
        self.last_update_time = now

    def _start_trip(self, start_time: datetime):
        """Start a new trip"""
        self.current_trip_id = f"trip_{int(start_time.timestamp())}"
        self.trip_start_time = start_time

        logger.info(f"Trip started: {self.current_trip_id}")

        # Write trip start event
        point = Point("trips") \
            .tag("trip_id", self.current_trip_id) \
            .tag("event", "start") \
            .field("start_time", start_time.isoformat()) \
            .time(start_time)

        try:
            self.write_api.write(bucket=self.bucket, record=point)
        except Exception as e:
            logger.error(f"Failed to write trip start: {e}")

    def _end_trip(self, end_time: datetime):
        """End current trip and calculate final metrics"""
        if not self.current_trip_id or not self.trip_start_time:
            return

        logger.info(f"Trip ended: {self.current_trip_id}")

        # Calculate trip metrics
        metrics = self._calculate_trip_metrics(self.trip_start_time, end_time)

        # Write trip end event with metrics
        point = Point("trips") \
            .tag("trip_id", self.current_trip_id) \
            .tag("event", "end") \
            .field("end_time", end_time.isoformat()) \
            .field("duration_minutes", metrics.get("duration_minutes", 0)) \
            .field("distance_km", metrics.get("distance_km", 0)) \
            .field("energy_consumed_kwh", metrics.get("energy_consumed_kwh", 0)) \
            .field("energy_regen_kwh", metrics.get("energy_regen_kwh", 0)) \
            .field("efficiency_wh_per_km", metrics.get("efficiency_wh_per_km", 0)) \
            .field("avg_speed_kmh", metrics.get("avg_speed_kmh", 0)) \
            .time(end_time)

        try:
            self.write_api.write(bucket=self.bucket, record=point)
        except Exception as e:
            logger.error(f"Failed to write trip end: {e}")

        # Reset trip state
        self.current_trip_id = None
        self.trip_start_time = None

    def _update_trip_metrics(self, now: datetime, speed_kmh: float, soc_percent: int, power_kw: float):
        """Update ongoing trip metrics"""
        if not self.current_trip_id:
            return

        # Write trip data point
        point = Point("trip_data") \
            .tag("trip_id", self.current_trip_id) \
            .field("speed_kmh", float(speed_kmh)) \
            .field("soc_percent", int(soc_percent)) \
            .field("power_kw", float(power_kw)) \
            .time(now)

        try:
            self.write_api.write(bucket=self.bucket, record=point)
        except Exception as e:
            logger.error(f"Failed to write trip data: {e}")

    def _calculate_trip_metrics(self, start_time: datetime, end_time: datetime) -> Dict[str, float]:
        """Calculate trip metrics from InfluxDB data"""
        metrics = {
            "duration_minutes": 0.0,
            "distance_km": 0.0,
            "energy_consumed_kwh": 0.0,
            "energy_regen_kwh": 0.0,
            "efficiency_wh_per_km": 0.0,
            "avg_speed_kmh": 0.0
        }

        try:
            # Duration
            duration = (end_time - start_time).total_seconds() / 60.0
            metrics["duration_minutes"] = duration

            # Distance (integrate GPS speed over time)
            distance_query = f'''
from(bucket: "{self.bucket}")
  |> range(start: {start_time.isoformat()}Z, stop: {end_time.isoformat()}Z)
  |> filter(fn: (r) => r["_measurement"] == "trip_data")
  |> filter(fn: (r) => r["_field"] == "speed_kmh")
  |> integral(unit: 1h)
            '''

            tables = self.query_api.query(distance_query)
            for table in tables:
                for record in table.records:
                    metrics["distance_km"] = record.get_value()

            # Average speed
            avg_speed_query = f'''
from(bucket: "{self.bucket}")
  |> range(start: {start_time.isoformat()}Z, stop: {end_time.isoformat()}Z)
  |> filter(fn: (r) => r["_measurement"] == "trip_data")
  |> filter(fn: (r) => r["_field"] == "speed_kmh")
  |> mean()
            '''

            tables = self.query_api.query(avg_speed_query)
            for table in tables:
                for record in table.records:
                    metrics["avg_speed_kmh"] = record.get_value()

            # Energy consumed (integrate positive power)
            energy_consumed_query = f'''
from(bucket: "{self.bucket}")
  |> range(start: {start_time.isoformat()}Z, stop: {end_time.isoformat()}Z)
  |> filter(fn: (r) => r["_measurement"] == "trip_data")
  |> filter(fn: (r) => r["_field"] == "power_kw")
  |> filter(fn: (r) => r["_value"] > 0.0)
  |> integral(unit: 1h)
            '''

            tables = self.query_api.query(energy_consumed_query)
            for table in tables:
                for record in table.records:
                    metrics["energy_consumed_kwh"] = record.get_value()

            # Energy regenerated (integrate negative power)
            energy_regen_query = f'''
from(bucket: "{self.bucket}")
  |> range(start: {start_time.isoformat()}Z, stop: {end_time.isoformat()}Z)
  |> filter(fn: (r) => r["_measurement"] == "trip_data")
  |> filter(fn: (r) => r["_field"] == "power_kw")
  |> filter(fn: (r) => r["_value"] < 0.0)
  |> map(fn: (r) => ({ r with _value: -r._value }))
  |> integral(unit: 1h)
            '''

            tables = self.query_api.query(energy_regen_query)
            for table in tables:
                for record in table.records:
                    metrics["energy_regen_kwh"] = record.get_value()

            # Efficiency (Wh/km)
            if metrics["distance_km"] > 0:
                net_energy = metrics["energy_consumed_kwh"] - metrics["energy_regen_kwh"]
                metrics["efficiency_wh_per_km"] = (net_energy * 1000) / metrics["distance_km"]

        except Exception as e:
            logger.error(f"Failed to calculate trip metrics: {e}")

        return metrics

    def get_current_trip_status(self) -> Optional[Dict[str, Any]]:
        """Get status of current ongoing trip"""
        if not self.current_trip_id or not self.trip_start_time:
            return None

        now = datetime.utcnow()
        duration = (now - self.trip_start_time).total_seconds() / 60.0

        # Get partial metrics for current trip
        metrics = self._calculate_trip_metrics(self.trip_start_time, now)

        return {
            "trip_id": self.current_trip_id,
            "start_time": self.trip_start_time.isoformat(),
            "duration_minutes": duration,
            "metrics": metrics
        }

    def get_recent_trips(self, limit: int = 10) -> List[Dict[str, Any]]:
        """Get recent completed trips"""
        trips = []

        try:
            # Query trip end events
            query = f'''
from(bucket: "{self.bucket}")
  |> range(start: -30d)
  |> filter(fn: (r) => r["_measurement"] == "trips")
  |> filter(fn: (r) => r["event"] == "end")
  |> sort(columns: ["_time"], desc: true)
  |> limit(n: {limit})
  |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
            '''

            tables = self.query_api.query(query)
            for table in tables:
                for record in table.records:
                    trip_id = record.values.get("trip_id", "")
                    trips.append({
                        "trip_id": trip_id,
                        "end_time": record.get_time().isoformat(),
                        "duration_minutes": record.values.get("duration_minutes", 0),
                        "distance_km": record.values.get("distance_km", 0),
                        "energy_consumed_kwh": record.values.get("energy_consumed_kwh", 0),
                        "energy_regen_kwh": record.values.get("energy_regen_kwh", 0),
                        "efficiency_wh_per_km": record.values.get("efficiency_wh_per_km", 0),
                        "avg_speed_kmh": record.values.get("avg_speed_kmh", 0)
                    })

        except Exception as e:
            logger.error(f"Failed to get recent trips: {e}")

        return trips
