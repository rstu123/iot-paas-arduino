/*
 * IoTPaaS Example: Temperature & Humidity Sensor
 * 
 * Reads DHT11 temperature/humidity, PIR motion, and light level
 * and publishes to the IoT PaaS platform.
 * 
 * Matched to custom ESP32 PCB with:
 *   - DHT11 on GPIO15
 *   - PIR motion sensor on GPIO33
 *   - Light sensor (LDR) on GPIO32 (analog)
 * 
 * MQTT Channels:
 *   Telemetry (out): "temperature"   ← °C as float
 *                    "humidity"      ← % as float
 *                    "motion"        ← "detected" / "clear"
 *                    "light"         ← raw analog value (0-4095)
 *                    "light_status"  ← "dark" / "light"
 *                    "sensors"       ← JSON with all readings
 * 
 * Setup:
 *   1. Create a device on your IoT PaaS dashboard
 *   2. Copy credentials below
 *   3. Install DHT library: Sketch > Include Library > DHT sensor library
 *   4. Upload to ESP32
 */

#include <IoTPaaS.h>
#include <DHT.h>

// ========== CONFIGURATION ==========
const char* WIFI_SSID     = "Loan BQL 1";
const char* WIFI_PASSWORD = "0583870015";

const char* MQTT_HOST     = "103.90.225.183";
const uint16_t MQTT_PORT  = 1883;

const char* USER_ID       = "dd1dc7c1-b1cd-4961-a6ee-6d4e732e4083";
const char* DEVICE_ID     = "5c8de9e0-5c75-4ac6-8c5e-301492da20ca";
const char* DEVICE_TOKEN  = "esp32-secret-token-123";

// Telemetry interval (milliseconds)
const unsigned long PUBLISH_INTERVAL = 5000;  // 5 seconds
// ====================================

// ========== PIN DEFINITIONS ==========
// Matched to custom PCB schematic
#define DHTPIN       15    // DHT11 data pin
#define DHTTYPE      DHT11
#define PIR_PIN      33    // PIR motion sensor
#define LIGHT_PIN    32    // LDR analog input
#define LED1_PIN      2    // Status LED (blinks on publish)

// Light level threshold (adjust based on your LDR)
#define LIGHT_THRESHOLD 2000  // Above = dark, Below = light
// =====================================

IoTPaaS iot;
DHT dht(DHTPIN, DHTTYPE);

// Timing
unsigned long lastPublish = 0;

// Track motion changes to publish immediately
bool lastMotionState = false;
unsigned long lastMotionPublish = 0;
const unsigned long MOTION_COOLDOWN = 5000;

// ---- Blink LED briefly to show activity ----
void blinkLED() {
    digitalWrite(LED1_PIN, HIGH);
    delay(50);
    digitalWrite(LED1_PIN, LOW);
}

// ---- Read and publish all sensor data ----
void publishSensorData() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int lightLevel = analogRead(LIGHT_PIN);
    bool motionDetected = digitalRead(PIR_PIN) == HIGH;
    
    // Publish individual channels
    if (!isnan(temperature)) {
        iot.publish("temperature", temperature);
    } else {
        Serial.println("[Sensor] DHT temperature read failed");
    }
    
    if (!isnan(humidity)) {
        iot.publish("humidity", humidity);
    } else {
        Serial.println("[Sensor] DHT humidity read failed");
    }
    
    iot.publish("light", lightLevel);
    iot.publish("light_status", lightLevel > LIGHT_THRESHOLD ? "dark" : "light");
    iot.publish("motion", motionDetected ? "detected" : "clear");
    
    // Also publish combined JSON on "sensors" channel
    JsonDocument doc;
    
    if (!isnan(temperature)) doc["temperature"] = round(temperature * 10) / 10.0;
    if (!isnan(humidity)) doc["humidity"] = round(humidity * 10) / 10.0;
    doc["light_level"] = lightLevel;
    doc["light_status"] = lightLevel > LIGHT_THRESHOLD ? "dark" : "light";
    doc["motion"] = motionDetected ? "detected" : "clear";
    doc["uptime_sec"] = iot.getUptime() / 1000;
    
    iot.publishJson("sensors", doc);
    
    // Visual feedback
    blinkLED();
    
    // Serial output
    Serial.printf("[Sensor] Temp: %.1f°C | Humidity: %.1f%% | Light: %d (%s) | Motion: %s\n",
                  isnan(temperature) ? 0.0 : temperature,
                  isnan(humidity) ? 0.0 : humidity,
                  lightLevel,
                  lightLevel > LIGHT_THRESHOLD ? "dark" : "light",
                  motionDetected ? "YES" : "no");
}

// ---- Handle commands (e.g., change publish interval) ----
void handleCommand(const char* channel, const char* payload) {
    Serial.printf(">> Command [%s]: %s\n", channel, payload);
    
    // Force an immediate sensor reading
    if (strcmp(channel, "read") == 0) {
        Serial.println("[Sensor] Manual read requested");
        publishSensorData();
    }
}

// ---- State change callback ----
void handleStateChange(IoTPaaSState newState, IoTPaaSState oldState) {
    if (newState == STATE_MQTT_CONNECTED) {
        Serial.println("[Sensor] Connected — publishing initial reading");
        publishSensorData();
    }
}

// ---- Error callback ----
void handleError(IoTPaaSError error, const char* message) {
    Serial.printf("!! ERROR [%d]: %s\n", error, message);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("=====================================");
    Serial.println("  IoTPaaS - Temperature/Humidity");
    Serial.println("  + PIR Motion + Light Sensor");
    Serial.println("  Library v" IOTPAAS_VERSION);
    Serial.println("=====================================");
    
    // Initialize sensors
    dht.begin();
    delay(2000);  // DHT11 needs time to stabilize
    
    pinMode(PIR_PIN, INPUT);
    pinMode(LIGHT_PIN, INPUT);
    pinMode(LED1_PIN, OUTPUT);
    digitalWrite(LED1_PIN, LOW);
    
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
        iot.subscribe("read");  // Subscribe to manual read commands
    } else {
        Serial.println("Connection failed. Will retry automatically.");
    }
}

void loop() {
    iot.loop();
    
    if (!iot.isConnected()) return;
    
    // Periodic sensor publishing
    unsigned long now = millis();
    if (now - lastPublish >= PUBLISH_INTERVAL) {
        lastPublish = now;
        publishSensorData();
    }
    
    // Publish immediately on motion change
    bool currentMotion = digitalRead(PIR_PIN) == HIGH;
    if (currentMotion != lastMotionState && millis() - lastMotionPublish > MOTION_COOLDOWN) {
        lastMotionPublish = millis();
        iot.publish("motion", currentMotion ? "detected" : "clear");
        Serial.printf("[Sensor] Motion %s!\n", currentMotion ? "DETECTED" : "cleared");
    }
}
