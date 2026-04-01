/*
 * IoTPaaS Example: Generic Sensor Template
 * 
 * A minimal starting template for connecting ANY sensor to the
 * IoT PaaS platform. Copy this and customize for your project.
 * 
 * This template demonstrates:
 *   - Periodic telemetry publishing
 *   - Receiving commands from the platform
 *   - Error handling and state monitoring
 *   - OTA update support
 * 
 * MQTT Channels:
 *   Commands (in):  "config"    ← Change settings (e.g., interval)
 *                   "read"      ← Force immediate sensor reading
 *   Telemetry (out): "sensor"   ← Your sensor data
 *                    "status"   ← Device status/health info
 * 
 * How to customize:
 *   1. Add your sensor #includes and pin definitions
 *   2. Initialize your sensor in setup()
 *   3. Replace readAndPublish() with your sensor reading logic
 *   4. Add command handlers for your actuators (if any)
 */

#include <IoTPaaS.h>

// ========== CONFIGURATION ==========
const char* WIFI_SSID     = "YourWiFi";
const char* WIFI_PASSWORD = "YourPassword";

const char* MQTT_HOST     = "103.90.225.183";
const uint16_t MQTT_PORT  = 1883;

const char* USER_ID       = "your-user-id";
const char* DEVICE_ID     = "your-device-id";
const char* DEVICE_TOKEN  = "your-device-token";
// ====================================

// ========== YOUR SENSOR PINS ==========
// TODO: Define your sensor pins here
// #define SENSOR_PIN  34
// #define ACTUATOR_PIN 25
#define STATUS_LED  2  // Built-in LED for status
// =======================================

// ========== SETTINGS ==========
unsigned long publishInterval = 5000;  // Default: 5 seconds
// ==============================

IoTPaaS iot;
unsigned long lastPublish = 0;

// ================================================
//  TODO: Replace this with YOUR sensor logic
// ================================================
void readAndPublish() {
    // --- Example: Read an analog sensor ---
    // int sensorValue = analogRead(SENSOR_PIN);
    // float voltage = sensorValue * (3.3 / 4095.0);
    // iot.publish("sensor", voltage);
    
    // --- Example: Read and publish as JSON ---
    JsonDocument doc;
    doc["value"] = random(0, 100);            // Replace with real sensor reading
    doc["unit"] = "units";                     // Replace with actual unit
    doc["uptime_sec"] = iot.getUptime() / 1000;
    doc["rssi"] = WiFi.RSSI();
    
    iot.publishJson("sensor", doc);
    
    // Blink LED to show activity
    digitalWrite(STATUS_LED, HIGH);
    delay(50);
    digitalWrite(STATUS_LED, LOW);
    
    Serial.printf("[Sensor] Published reading (interval: %lums)\n", publishInterval);
}

// ================================================
//  Handle commands from platform
// ================================================
void handleCommand(const char* channel, const char* payload) {
    Serial.printf(">> Command [%s]: %s\n", channel, payload);
    
    // Force an immediate reading
    if (strcmp(channel, "read") == 0) {
        readAndPublish();
    }
    // Change publish interval
    else if (strcmp(channel, "config") == 0) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            if (doc.containsKey("interval")) {
                publishInterval = doc["interval"].as<unsigned long>();
                publishInterval = max(1000UL, publishInterval);  // Min 1 second
                Serial.printf("[Config] Publish interval: %lums\n", publishInterval);
                
                // Acknowledge config change
                JsonDocument ack;
                ack["interval"] = publishInterval;
                ack["status"] = "updated";
                iot.publishJson("status", ack);
            }
        }
    }
    // TODO: Add your own command handlers here
    // else if (strcmp(channel, "actuator") == 0) {
    //     digitalWrite(ACTUATOR_PIN, strcmp(payload, "on") == 0 ? HIGH : LOW);
    // }
}

// ================================================
//  Connection state monitoring
// ================================================
void handleStateChange(IoTPaaSState newState, IoTPaaSState oldState) {
    switch (newState) {
        case STATE_MQTT_CONNECTED:
            Serial.println("[Status] Connected to platform");
            digitalWrite(STATUS_LED, HIGH);
            
            // Publish device info on connect
            {
                JsonDocument info;
                info["firmware"] = IOTPAAS_VERSION;
                info["interval_ms"] = publishInterval;
                info["ip"] = WiFi.localIP().toString();
                info["rssi"] = WiFi.RSSI();
                info["reconnects"] = iot.getReconnectCount();
                iot.publishJson("status", info);
            }
            break;
            
        case STATE_DISCONNECTED:
        case STATE_ERROR:
            digitalWrite(STATUS_LED, LOW);
            break;
            
        default:
            break;
    }
}

void handleError(IoTPaaSError error, const char* message) {
    Serial.printf("!! ERROR [%d]: %s\n", error, message);
}

// ================================================
//  Setup
// ================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("================================");
    Serial.println("  IoTPaaS - Generic Sensor");
    Serial.println("  Library v" IOTPAAS_VERSION);
    Serial.println("================================");
    
    // Initialize pins
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    
    // TODO: Initialize your sensor here
    // pinMode(SENSOR_PIN, INPUT);
    // pinMode(ACTUATOR_PIN, OUTPUT);
    // yourSensor.begin();
    
    // Configure IoTPaaS
    iot.setWiFi(WIFI_SSID, WIFI_PASSWORD);
    iot.setBroker(MQTT_HOST, MQTT_PORT);
    iot.setCredentials(USER_ID, DEVICE_ID, DEVICE_TOKEN);
    iot.setLogLevel(LOG_INFO);
    iot.setAutoReconnect(true, 2000, 30000, 0);
    iot.enableOTA(true);
    
    // Register callbacks
    iot.onCommand(handleCommand);
    iot.onStateChange(handleStateChange);
    iot.onError(handleError);
    
    // Connect
    if (iot.begin()) {
        Serial.println("Connected to IoT PaaS!");
        
        // Subscribe to command channels
        iot.subscribe("read");
        iot.subscribe("config");
        // TODO: Add your command subscriptions
        // iot.subscribe("actuator");
    } else {
        Serial.println("Connection failed. Will retry automatically.");
    }
}

// ================================================
//  Main Loop
// ================================================
void loop() {
    iot.loop();
    
    // Periodic sensor publishing
    if (iot.isConnected() && millis() - lastPublish >= publishInterval) {
        lastPublish = millis();
        readAndPublish();
    }
    
    // TODO: Add your own loop logic here
    // e.g., check for button presses, threshold alerts, etc.
}
