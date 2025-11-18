#!/usr/bin/env python3
"""
InfluxDB Cloud Sync Service
Replicates data from local InfluxDB to cloud when network is available
"""

import os
import sys
import time
import signal
import logging
import subprocess
from datetime import datetime, timedelta
from typing import Optional
import requests
from influxdb_client import InfluxDBClient
from influxdb_client.client.query_api import QueryApi

# Setup logging (systemd will capture stdout to journal)
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)


class CloudSyncService:
    """Syncs local InfluxDB data to cloud"""

    def __init__(self):
        self.running = False

        # Local InfluxDB config
        self.local_url = os.getenv("INFLUX_URL", "http://localhost:8086")
        self.local_org = os.getenv("INFLUX_ORG", "leaf-telemetry")
        self.local_bucket = os.getenv("INFLUX_BUCKET", "leaf-data")
        self.local_token = os.getenv("INFLUX_SYNC_TOKEN", "")

        # Cloud InfluxDB config
        self.cloud_url = os.getenv("INFLUX_CLOUD_URL", "")
        self.cloud_org = os.getenv("INFLUX_CLOUD_ORG", "")
        self.cloud_bucket = os.getenv("INFLUX_CLOUD_BUCKET", "")
        self.cloud_token = os.getenv("INFLUX_CLOUD_TOKEN", "")

        # Sync configuration
        self.sync_interval = int(os.getenv("SYNC_INTERVAL_SECONDS", "300"))  # 5 minutes
        self.batch_duration = os.getenv("SYNC_BATCH_DURATION", "1h")  # Sync 1 hour at a time
        self.network_check_url = os.getenv("NETWORK_CHECK_URL", "https://1.1.1.1")

        # Validate cloud config
        if not all([self.cloud_url, self.cloud_org, self.cloud_bucket, self.cloud_token]):
            raise ValueError("Cloud InfluxDB configuration incomplete. Check environment variables.")

        if not self.local_token:
            raise ValueError("INFLUX_SYNC_TOKEN not set")

        # Clients
        self.local_client: Optional[InfluxDBClient] = None
        self.cloud_client: Optional[InfluxDBClient] = None

        # State tracking
        self.last_sync_time: Optional[datetime] = None
        self.sync_count = 0
        self.error_count = 0

    def init(self) -> bool:
        """Initialize InfluxDB clients"""
        try:
            # Local client
            logger.info(f"Connecting to local InfluxDB: {self.local_url}")
            self.local_client = InfluxDBClient(
                url=self.local_url,
                token=self.local_token,
                org=self.local_org
            )

            # Test local connection
            health = self.local_client.health()
            if health.status != "pass":
                logger.error(f"Local InfluxDB health check failed: {health.message}")
                return False

            logger.info("Local InfluxDB connected successfully")

            # Cloud client
            logger.info(f"Connecting to cloud InfluxDB: {self.cloud_url}")
            self.cloud_client = InfluxDBClient(
                url=self.cloud_url,
                token=self.cloud_token,
                org=self.cloud_org,
                timeout=30000  # 30 second timeout for cloud
            )

            logger.info("Cloud InfluxDB client initialized")
            return True

        except Exception as e:
            logger.error(f"Initialization failed: {e}")
            return False

    def check_network(self) -> bool:
        """Check if network/internet is available"""
        try:
            response = requests.get(
                self.network_check_url,
                timeout=5
            )
            return response.status_code == 200
        except Exception:
            return False

    def check_cloud_connection(self) -> bool:
        """Check if cloud InfluxDB is reachable"""
        try:
            health = self.cloud_client.health()
            return health.status == "pass"
        except Exception as e:
            logger.debug(f"Cloud connection check failed: {e}")
            return False

    def sync_batch(self, start_time: datetime, end_time: datetime) -> bool:
        """Sync a batch of data from local to cloud"""
        try:
            # Query local data
            query_api = self.local_client.query_api()

            # Build Flux query to get all measurements in time range
            flux_query = f'''
from(bucket: "{self.local_bucket}")
  |> range(start: {start_time.isoformat()}Z, stop: {end_time.isoformat()}Z)
            '''

            logger.debug(f"Querying local data: {start_time} to {end_time}")
            tables = query_api.query(flux_query)

            # Convert to line protocol and write to cloud
            write_api = self.cloud_client.write_api()
            points_written = 0

            for table in tables:
                for record in table.records:
                    # Write record to cloud
                    try:
                        write_api.write(
                            bucket=self.cloud_bucket,
                            record=record
                        )
                        points_written += 1
                    except Exception as e:
                        logger.error(f"Failed to write point: {e}")
                        self.error_count += 1

            write_api.close()

            if points_written > 0:
                logger.info(f"Synced {points_written} points from {start_time} to {end_time}")

            return True

        except Exception as e:
            logger.error(f"Batch sync failed: {e}")
            self.error_count += 1
            return False

    def determine_sync_range(self) -> tuple[datetime, datetime]:
        """Determine time range to sync"""
        now = datetime.utcnow()

        if self.last_sync_time is None:
            # First sync: sync last hour
            end_time = now
            start_time = now - timedelta(hours=1)
        else:
            # Sync from last sync time to now
            start_time = self.last_sync_time
            end_time = now

            # Limit to batch duration to avoid overwhelming
            max_duration = timedelta(hours=1)
            if end_time - start_time > max_duration:
                end_time = start_time + max_duration

        return start_time, end_time

    def perform_sync(self):
        """Perform one sync cycle"""
        # Check network connectivity
        if not self.check_network():
            logger.info("Network not available, skipping sync")
            return

        # Check cloud connection
        if not self.check_cloud_connection():
            logger.warning("Cloud InfluxDB not reachable, skipping sync")
            return

        logger.info("Network available, starting sync")

        # Determine sync range
        start_time, end_time = self.determine_sync_range()

        # Sync batch
        if self.sync_batch(start_time, end_time):
            self.last_sync_time = end_time
            self.sync_count += 1
            logger.info(f"Sync successful (total syncs: {self.sync_count})")
        else:
            logger.warning("Sync failed")

    def run(self):
        """Main sync loop"""
        self.running = True
        logger.info(f"Starting cloud sync service (interval: {self.sync_interval}s)")

        # Perform initial sync after short delay
        time.sleep(30)

        try:
            while self.running:
                try:
                    self.perform_sync()
                except Exception as e:
                    logger.error(f"Error in sync cycle: {e}")
                    self.error_count += 1

                # Wait for next sync interval
                logger.info(f"Next sync in {self.sync_interval} seconds")
                for _ in range(self.sync_interval):
                    if not self.running:
                        break
                    time.sleep(1)

        except KeyboardInterrupt:
            logger.info("Keyboard interrupt received")
        finally:
            self.cleanup()

    def cleanup(self):
        """Cleanup resources"""
        logger.info("Cleaning up...")
        self.running = False

        if self.local_client:
            self.local_client.close()
        if self.cloud_client:
            self.cloud_client.close()

        logger.info("Cleanup complete")

    def signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        logger.info(f"Received signal {signum}")
        self.running = False


def main():
    """Main entry point"""
    logger.info("=== InfluxDB Cloud Sync Service ===")

    try:
        sync_service = CloudSyncService()

        # Setup signal handlers
        signal.signal(signal.SIGINT, sync_service.signal_handler)
        signal.signal(signal.SIGTERM, sync_service.signal_handler)

        # Initialize
        if not sync_service.init():
            logger.error("Failed to initialize")
            return 1

        # Run main loop
        sync_service.run()

        return 0

    except Exception as e:
        logger.error(f"Fatal error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
