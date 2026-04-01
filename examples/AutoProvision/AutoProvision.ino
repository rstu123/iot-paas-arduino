/*
 * IoTPaaS Auto-Provision Example
 * 
 * This example shows how to use auto-provisioning, where the device
 * automatically registers itself with the platform on first boot.
 * 
 * How it works:
 * 1. Generate a one-time provision key in the platform dashboard
 * 2. Flash this sketch with the provision key
 * 3. On first boot, the device registers itself and stores credentials
 * 4. On subsequent boots, it uses the stored credentials
 * 
 * This is useful for:
 * - Mass deployment of devices
 * - Devices where you can't easily access the code
 * - More secure key management (provision key is single-use)
 */

#include <IoTPaaS.h>

// ========== CONFIGURATION ==========
// WiFi Settings
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";

// Platform Settings
const char* MQTT_HOST = "your-server.com";
const uint16_t MQTT_PORT = 1883;
const bool USE_TLS = false;

// Auto-Provisioning Settings
// Generate this key in: Dashboard > Devices > New Device > Get Provision Key
const char* PROVISION_URL = "https://your-server.com/api/devices/provision";
const char* PROVISION_KEY = "your-one-time-provision-key";
// ===================================

IoTPaaS iot;

// Called when a command is received
void handleCommand(const char* channel, const char* payload) {
    Serial.print("Command received - Channel: ");
    Serial.print(channel);
    Serial.print(", Payload: ");
    Serial.println(payload);
    
    // Handle different channels
    if (strcmp(channel, "led") == 0) {
        bool state = (strcmp(payload, "on") == 0 || strcmp(payload, "true") == 0);
        Serial.print("LED: ");
        Serial.println(state ? "ON" : "OFF");
        // digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("=================================");
    Serial.println("  IoTPaaS - Auto-Provision");
    Serial.println("=================================");
    
    // Configure WiFi and broker
    iot.setWiFi(WIFI_SSID, WIFI_PASSWORD);
    iot.setBroker(MQTT_HOST, MQTT_PORT, USE_TLS);
    
    // Enable auto-provisioning
    // On first boot: device will call PROVISION_URL with PROVISION_KEY
    // On subsequent boots: device will use stored credentials from flash
    iot.setProvisioning(PROVISION_URL, PROVISION_KEY);
    
    // Set command callback
    iot.onCommand(handleCommand);
    
    // Connect (will auto-provision if needed)
    if (iot.begin()) {
        Serial.println("Connected!");
        Serial.print("Device ID: ");
        Serial.println(iot.getDeviceId());
        
        // Subscribe to all command channels
        iot.subscribeAll();
        
        // Announce we're online
        iot.publish("status", "online");
    } else {
        Serial.println("Connection failed!");
        Serial.print("State: ");
        Serial.println(iot.getStateString());
    }
}

void loop() {
    iot.loop();
    
    // Publish heartbeat every 30 seconds
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000 && iot.isConnected()) {
        lastHeartbeat = millis();
        
        // Create JSON telemetry
        StaticJsonDocument<128> doc;
        doc["uptime"] = millis() / 1000;
        doc["rssi"] = WiFi.RSSI();
        doc["heap"] = ESP.getFreeHeap();
        
        iot.publishJson("heartbeat", doc);
        Serial.println("Heartbeat sent");
    }
}
