/*
 * IoTPaaS - Basic Example with Error Handling
 * 
 * Demonstrates the complete IoTPaaS library features:
 * - WiFi + MQTT connection with hardcoded credentials
 * - State change monitoring
 * - Error handling with specific error codes
 * - Log level configuration
 * - OTA update support (automatic)
 * 
 * Setup:
 * 1. Create a device in the platform dashboard
 * 2. Copy your User ID, Device ID, and Device Token below
 * 3. Fill in your WiFi credentials
 * 4. Upload to ESP32
 */

#include <IoTPaaS.h>

// ========== CONFIGURATION ==========
const char* WIFI_SSID     = "Loan BQL 1";
const char* WIFI_PASSWORD = "0583870015";

const char* MQTT_HOST     = "103.90.225.183";
const uint16_t MQTT_PORT  = 1883;

const char* USER_ID       = "dd1dc7c1-b1cd-4961-a6ee-6d4e732e4083";
const char* DEVICE_ID     = "5c8de9e0-5c75-4ac6-8c5e-301492da20ca";
const char* DEVICE_TOKEN  = "esp32-secret-token-123";
// ====================================

IoTPaaS iot;

// Built-in LED for status indication
#define STATUS_LED 2

// ---- Callback: Commands from platform ----
void handleCommand(const char* channel, const char* payload) {
    Serial.printf(">> Command [%s]: %s\n", channel, payload);
    
    if (strcmp(channel, "relay") == 0) {
        bool on = (strcmp(payload, "on") == 0 || strcmp(payload, "1") == 0);
        Serial.printf("   Relay -> %s\n", on ? "ON" : "OFF");
        // digitalWrite(RELAY_PIN, on ? HIGH : LOW);
    }
}

// ---- Callback: Connection state changes ----
void handleStateChange(IoTPaaSState newState, IoTPaaSState oldState) {
    // Use the built-in LED to show connection status
    switch (newState) {
        case STATE_MQTT_CONNECTED:
            digitalWrite(STATUS_LED, HIGH);  // Solid = connected
            break;
        case STATE_WIFI_CONNECTING:
        case STATE_MQTT_CONNECTING:
            // Could implement blinking here with a timer
            break;
        case STATE_ERROR:
            digitalWrite(STATUS_LED, LOW);   // Off = error
            break;
        default:
            break;
    }
}

// ---- Callback: Errors ----
void handleError(IoTPaaSError error, const char* message) {
    Serial.printf("!! ERROR [%d]: %s\n", error, message);
    
    // You could take specific action based on error type:
    switch (error) {
        case ERR_MQTT_AUTH_FAILED:
            Serial.println("   -> Check your device credentials!");
            break;
        case ERR_WIFI_TIMEOUT:
            Serial.println("   -> Check WiFi SSID and password");
            break;
        case ERR_MQTT_NOT_AUTHORIZED:
            Serial.println("   -> ACL denied. Check topic permissions.");
            break;
        default:
            break;
    }
}

// ---- Callback: OTA progress ----
void handleOtaProgress(size_t progress, size_t total) {
    int percent = (progress * 100) / total;
    Serial.printf("OTA Progress: %d%%\n", percent);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    
    Serial.println();
    Serial.println("=================================");
    Serial.println("  IoTPaaS v" IOTPAAS_VERSION);
    Serial.println("  Basic Example + Error Handling");
    Serial.println("=================================");
    
    // Configure connection
    iot.setWiFi(WIFI_SSID, WIFI_PASSWORD);
    iot.setBroker(MQTT_HOST, MQTT_PORT);
    iot.setCredentials(USER_ID, DEVICE_ID, DEVICE_TOKEN);
    
    // Set log level (LOG_NONE, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG)
    iot.setLogLevel(LOG_INFO);
    
    // Configure reconnection: min 2s, max 30s backoff, unlimited retries
    iot.setAutoReconnect(true, 2000, 30000, 0);
    
    // Enable OTA updates via platform
    iot.enableOTA(true);
    
    // Register callbacks
    iot.onCommand(handleCommand);
    iot.onStateChange(handleStateChange);
    iot.onError(handleError);
    iot.onOtaProgress(handleOtaProgress);
    
    // Connect!
    if (iot.begin()) {
        Serial.println("Connected to IoT PaaS!");
        
        // Subscribe to command channels
        iot.subscribe("relay");
        iot.subscribe("brightness");
    } else {
        Serial.println("Connection failed. Will retry automatically.");
    }
}

// Telemetry timer
unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL = 10000; // 10 seconds

void loop() {
    iot.loop();
    
    // Send periodic telemetry
    if (iot.isConnected() && millis() - lastTelemetry > TELEMETRY_INTERVAL) {
        lastTelemetry = millis();
        
        // Send device stats
        float uptimeMinutes = iot.getUptime() / 60000.0;
        iot.publish("uptime", uptimeMinutes);
        iot.publish("rssi", WiFi.RSSI());
        iot.publish("reconnects", (int)iot.getReconnectCount());
        
        Serial.printf("Telemetry sent | Uptime: %.1f min | RSSI: %d dBm\n",
                       uptimeMinutes, WiFi.RSSI());
    }
}
