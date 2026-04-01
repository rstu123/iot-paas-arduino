/*
 * IoTPaaS Example: LED Control
 * 
 * Controls LEDs via the IoT PaaS platform with on/off and brightness (PWM).
 * Can be adapted for RGB LED control by wiring an RGB LED to PWM-capable pins.
 * 
 * Matched to custom ESP32 PCB with:
 *   - LED1 on GPIO2
 *   - LED2 on GPIO4
 * 
 * MQTT Channels:
 *   Commands (in):  "led1"        ← "on" / "off" / "toggle"
 *                   "led2"        ← "on" / "off" / "toggle"
 *                   "led1_brightness" ← 0-255
 *                   "led2_brightness" ← 0-255
 *                   "all"         ← "on" / "off" (controls both)
 *                   "rgb"         ← JSON: {"r":255,"g":128,"b":0} (if RGB LED wired)
 *   Telemetry (out): "led_state"  ← JSON with current LED states
 * 
 * For RGB LED (optional wiring):
 *   Connect an common-cathode RGB LED to any 3 PWM-capable GPIOs
 *   and update the RGB_*_PIN defines below.
 * 
 * Setup:
 *   1. Create a device on your IoT PaaS dashboard
 *   2. Copy credentials below
 *   3. Upload to ESP32
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

// ========== PIN DEFINITIONS ==========
// Board LEDs
#define LED1_PIN   2
#define LED2_PIN   4

// Optional RGB LED (wire your own, update pins)
// Set ENABLE_RGB to true if you have an RGB LED connected
#define ENABLE_RGB     false
#define RGB_RED_PIN    25
#define RGB_GREEN_PIN  26
#define RGB_BLUE_PIN   27

// PWM Configuration (ESP32 LEDC)
#define PWM_FREQ       5000
#define PWM_RESOLUTION 8     // 8-bit = 0-255
#define LED1_CHANNEL   0
#define LED2_CHANNEL   1
#define RGB_R_CHANNEL  2
#define RGB_G_CHANNEL  3
#define RGB_B_CHANNEL  4
// =====================================

IoTPaaS iot;

// LED states
uint8_t led1Brightness = 0;
uint8_t led2Brightness = 0;
uint8_t rgbR = 0, rgbG = 0, rgbB = 0;

// ---- Set LED brightness via PWM ----
void setLED1(uint8_t brightness) {
    led1Brightness = brightness;
    ledcWrite(LED1_PIN, brightness);
    Serial.printf("[LED] LED1 brightness: %d\n", brightness);
}

void setLED2(uint8_t brightness) {
    led2Brightness = brightness;
    ledcWrite(LED2_PIN, brightness);
    Serial.printf("[LED] LED2 brightness: %d\n", brightness);
}

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    if (!ENABLE_RGB) return;
    rgbR = r; rgbG = g; rgbB = b;
    ledcWrite(RGB_RED_PIN, r);
    ledcWrite(RGB_GREEN_PIN, g);
    ledcWrite(RGB_BLUE_PIN, b);
    Serial.printf("[LED] RGB: (%d, %d, %d)\n", r, g, b);
}

// ---- Report LED states to platform ----
void reportLEDState() {
    JsonDocument doc;
    doc["led1"] = led1Brightness > 0 ? "on" : "off";
    doc["led1_brightness"] = led1Brightness;
    doc["led2"] = led2Brightness > 0 ? "on" : "off";
    doc["led2_brightness"] = led2Brightness;
    
    if (ENABLE_RGB) {
        JsonObject rgb = doc["rgb"].to<JsonObject>();
        rgb["r"] = rgbR;
        rgb["g"] = rgbG;
        rgb["b"] = rgbB;
    }
    
    iot.publishJson("led_state", doc);
}

// ---- Handle commands from platform ----
void handleCommand(const char* channel, const char* payload) {
    Serial.printf(">> Command [%s]: %s\n", channel, payload);
    
    // LED1 on/off/toggle
    if (strcmp(channel, "led1") == 0) {
        if (strcmp(payload, "toggle") == 0) {
            setLED1(led1Brightness > 0 ? 0 : 255);
        } else if (strcmp(payload, "on") == 0 || strcmp(payload, "1") == 0) {
            setLED1(255);
        } else {
            setLED1(0);
        }
        reportLEDState();
    }
    // LED2 on/off/toggle
    else if (strcmp(channel, "led2") == 0) {
        if (strcmp(payload, "toggle") == 0) {
            setLED2(led2Brightness > 0 ? 0 : 255);
        } else if (strcmp(payload, "on") == 0 || strcmp(payload, "1") == 0) {
            setLED2(255);
        } else {
            setLED2(0);
        }
        reportLEDState();
    }
    // LED1 brightness (0-255)
    else if (strcmp(channel, "led1_brightness") == 0) {
        int val = atoi(payload);
        val = constrain(val, 0, 255);
        setLED1(val);
        reportLEDState();
    }
    // LED2 brightness (0-255)
    else if (strcmp(channel, "led2_brightness") == 0) {
        int val = atoi(payload);
        val = constrain(val, 0, 255);
        setLED2(val);
        reportLEDState();
    }
    // All LEDs on/off
    else if (strcmp(channel, "all") == 0) {
        if (strcmp(payload, "on") == 0 || strcmp(payload, "1") == 0) {
            setLED1(255);
            setLED2(255);
        } else {
            setLED1(0);
            setLED2(0);
        }
        reportLEDState();
    }
    // RGB LED control (JSON: {"r":255,"g":128,"b":0})
    else if (strcmp(channel, "rgb") == 0 && ENABLE_RGB) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            uint8_t r = doc["r"] | 0;
            uint8_t g = doc["g"] | 0;
            uint8_t b = doc["b"] | 0;
            setRGB(r, g, b);
            reportLEDState();
        } else {
            Serial.println("[LED] Invalid RGB JSON");
        }
    }
}

// ---- State change callback ----
void handleStateChange(IoTPaaSState newState, IoTPaaSState oldState) {
    if (newState == STATE_MQTT_CONNECTED) {
        reportLEDState();
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("================================");
    Serial.println("  IoTPaaS - LED Control");
    Serial.println("  Library v" IOTPAAS_VERSION);
    Serial.println("================================");
    
// Setup PWM for smooth brightness control (ESP32 Core v3.x API)
    ledcAttach(LED1_PIN, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(LED2_PIN, PWM_FREQ, PWM_RESOLUTION);
    
    // Setup RGB if enabled
    if (ENABLE_RGB) {
        ledcAttach(RGB_RED_PIN, PWM_FREQ, PWM_RESOLUTION);
        ledcAttach(RGB_GREEN_PIN, PWM_FREQ, PWM_RESOLUTION);
        ledcAttach(RGB_BLUE_PIN, PWM_FREQ, PWM_RESOLUTION);
        Serial.println("[LED] RGB LED enabled");
    }
    
    // Start with LEDs off
    setLED1(0);
    setLED2(0);
    setRGB(0, 0, 0);
    
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
    iot.onError([](IoTPaaSError error, const char* message) {
        Serial.printf("!! ERROR [%d]: %s\n", error, message);
    });
    
    // Connect
    if (iot.begin()) {
        Serial.println("Connected to IoT PaaS!");
        iot.subscribe("led1");
        iot.subscribe("led2");
        iot.subscribe("led1_brightness");
        iot.subscribe("led2_brightness");
        iot.subscribe("all");
        if (ENABLE_RGB) iot.subscribe("rgb");
    } else {
        Serial.println("Connection failed. Will retry automatically.");
    }
}

void loop() {
    iot.loop();
}
