# IoTPaaS Arduino Library

Connect your ESP32 to the IoT PaaS platform with minimal code.

## Features

- **Simple API** - Just `begin()`, `loop()`, `publish()`, `subscribe()`
- **Two credential modes** - Hardcoded (simple) or auto-provision (flexible)
- **Auto-reconnect** - Handles WiFi and MQTT disconnections automatically
- **TLS support** - Secure connections when needed
- **JSON support** - Publish structured data easily

## Installation

### Arduino IDE

1. Download this repository as a ZIP file
2. In Arduino IDE: **Sketch** → **Include Library** → **Add .ZIP Library**
3. Select the downloaded ZIP

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/your-username/iot-paas-arduino.git
    PubSubClient
    ArduinoJson
```

## Dependencies

Install these libraries via Library Manager:

- **PubSubClient** by Nick O'Leary
- **ArduinoJson** by Benoit Blanchon (v6.x)

## Quick Start

### 1. Get Your Credentials

1. Log into the IoT PaaS dashboard
2. Create a new project (or select existing)
3. Add a new device
4. Copy your **User ID**, **Device ID**, and **Device Token**

### 2. Basic Example

```cpp
#include <IoTPaaS.h>

IoTPaaS iot;

void handleCommand(const char* channel, const char* payload) {
    Serial.printf("Command on %s: %s\n", channel, payload);
    
    if (strcmp(channel, "led") == 0) {
        digitalWrite(LED_BUILTIN, strcmp(payload, "on") == 0);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    
    iot.setWiFi("your-ssid", "your-password");
    iot.setBroker("your-server.com", 1883);
    iot.setCredentials("user-id", "device-id", "device-token");
    iot.onCommand(handleCommand);
    
    if (iot.begin()) {
        iot.subscribe("led");
        iot.publish("status", "online");
    }
}

void loop() {
    iot.loop();
}
```

## API Reference

### Configuration

```cpp
void setWiFi(const char* ssid, const char* password);
void setBroker(const char* host, uint16_t port = 1883, bool useTLS = false);
void setCredentials(const char* userId, const char* deviceId, const char* token);
void setProvisioning(const char* provisionUrl, const char* provisionKey);
void setAutoReconnect(bool enabled, uint32_t delayMs = 5000);
```

### Connection

```cpp
bool begin();           // Initialize and connect (call in setup())
void loop();            // Process messages (call in loop())
void disconnect();      // Manual disconnect
bool isConnected();     // Check connection status
```

### Messaging

```cpp
bool subscribe(const char* channel);          // Subscribe to command channel
bool subscribeAll();                          // Subscribe to all channels (wildcard)
void onCommand(CommandCallback callback);     // Set command handler

bool publish(const char* channel, const char* payload);
bool publish(const char* channel, float value);
bool publish(const char* channel, int value);
bool publish(const char* channel, bool value);
bool publishJson(const char* channel, JsonDocument& doc);
```

## Topic Structure

The library automatically handles topic formatting:

| Direction | Topic Format | Use |
|-----------|-------------|-----|
| Commands (in) | `u/{userId}/d/{deviceId}/in/{channel}` | Receive commands |
| Telemetry (out) | `u/{userId}/d/{deviceId}/out/{channel}` | Send sensor data |

## Auto-Provisioning

For devices that self-register on first boot:

```cpp
iot.setProvisioning(
    "https://your-server.com/api/devices/provision",
    "one-time-provision-key"
);
```

On first boot, the device will:
1. Connect to WiFi
2. Call the provision endpoint with the key
3. Receive and store credentials in flash
4. Connect to MQTT

On subsequent boots, it uses the stored credentials.

## TLS Support

For secure connections:

```cpp
iot.setBroker("your-server.com", 8883, true);  // TLS on port 8883
```

Note: By default, certificate validation is skipped for development. For production, modify the library to include your CA certificate.

## Troubleshooting

### Connection States

```cpp
Serial.println(iot.getStateString());
```

| State | Meaning |
|-------|---------|
| DISCONNECTED | Not connected |
| WIFI_CONNECTING | Attempting WiFi connection |
| WIFI_CONNECTED | WiFi OK, MQTT not connected |
| MQTT_CONNECTING | Attempting MQTT connection |
| MQTT_CONNECTED | Fully connected, ready to use |
| PROVISIONING | Auto-provisioning in progress |
| ERROR | Connection error occurred |

### Common Issues

1. **"MQTT connection failed, state: -2"** - Check broker host/port
2. **"Missing device credentials"** - Verify credentials are set
3. **WiFi connects but MQTT fails** - Check firewall allows MQTT port
4. **Messages not received** - Ensure `iot.loop()` is called in `loop()`

## Examples

See the `examples/` folder:

- **BasicHardcoded** - Simplest setup with hardcoded credentials
- **AutoProvision** - Self-registering device

## License

MIT License
