#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "LeafCANBus.h"

// GPS configuration
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GPS_BAUD 9600

// CAN configuration (using default pins from LeafCANBus.h)
// TX=5, RX=4

// GPS objects
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // Use UART2

// CAN bus object
LeafCANBus canBus;

// GPS state structures
GPSPositionState gpsPosition = {0};
GPSVelocityState gpsVelocity = {0};
GPSTimeState gpsTime = {0};

// Update interval
unsigned long lastGPSUpdate = 0;
const unsigned long GPS_UPDATE_INTERVAL = 1000;  // 1 second

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Nissan Leaf CAN Network - GPS Module ===");

    // Initialize GPS
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("[GPS] Initialized on UART2");

    // Initialize CAN bus
    if (!canBus.begin()) {
        Serial.println("[ERROR] Failed to initialize CAN bus!");
        while (1) delay(1000);
    }

    // Setup CAN publishers
    canBus.publish(CAN_ID_GPS_POSITION, 1000, pack_gps_position, &gpsPosition);
    canBus.publish(CAN_ID_GPS_VELOCITY, 1000, pack_gps_velocity, &gpsVelocity);
    canBus.publish(CAN_ID_GPS_TIME, 1000, pack_gps_time, &gpsTime);

    Serial.println("[GPS] CAN publishers configured");
    Serial.println("[GPS] Waiting for GPS fix...");
}

void updateGPSData() {
    // Read GPS data
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        gps.encode(c);
    }

    // Update position data
    if (gps.location.isValid()) {
        gpsPosition.latitude = gps.location.lat();
        gpsPosition.longitude = gps.location.lng();
        gpsPosition.satellites = gps.satellites.value();
        gpsPosition.fix_quality = gps.location.isValid() ? 1 : 0;

        if (gps.altitude.isValid()) {
            gpsPosition.altitude = gps.altitude.meters();
        }
    }

    // Update velocity data
    if (gps.speed.isValid()) {
        gpsVelocity.speed_kmh = gps.speed.kmph();
    }

    if (gps.course.isValid()) {
        gpsVelocity.heading = gps.course.deg();
    }

    if (gps.hdop.isValid()) {
        gpsVelocity.pdop = gps.hdop.hdop();
    }

    // Update time data
    if (gps.date.isValid() && gps.time.isValid()) {
        gpsTime.year = gps.date.year();
        gpsTime.month = gps.date.month();
        gpsTime.day = gps.date.day();
        gpsTime.hour = gps.time.hour();
        gpsTime.minute = gps.time.minute();
        gpsTime.second = gps.time.second();
    }
}

void printGPSStatus() {
    Serial.println("\n--- GPS Status ---");
    Serial.printf("Satellites: %d\n", gpsPosition.satellites);

    if (gps.location.isValid()) {
        Serial.printf("Position: %.6f, %.6f\n", gpsPosition.latitude, gpsPosition.longitude);
        Serial.printf("Altitude: %.1f m\n", gpsPosition.altitude);
    } else {
        Serial.println("Position: No fix");
    }

    if (gps.speed.isValid()) {
        Serial.printf("Speed: %.1f km/h\n", gpsVelocity.speed_kmh);
        Serial.printf("Heading: %.1f°\n", gpsVelocity.heading);
    }

    if (gps.date.isValid() && gps.time.isValid()) {
        Serial.printf("Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                      gpsTime.year, gpsTime.month, gpsTime.day,
                      gpsTime.hour, gpsTime.minute, gpsTime.second);
    }

    Serial.printf("CAN Stats - RX: %u, TX: %u, Errors: %u\n",
                  canBus.getRxCount(), canBus.getTxCount(), canBus.getErrorCount());
    Serial.println("------------------");
}

void loop() {
    // Update GPS data
    updateGPSData();

    // Process CAN bus (handles publishing automatically)
    canBus.process();

    // Print status every 5 seconds
    unsigned long now = millis();
    if (now - lastGPSUpdate >= 5000) {
        printGPSStatus();
        lastGPSUpdate = now;
    }

    delay(10);
}
