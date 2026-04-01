# IoTPaaS Arduino Library

Arduino library for connecting ESP32 devices to the [IoT PaaS](https://iot-paas.io.vn) platform.

Handles WiFi, MQTT, reconnection, and OTA firmware updates — connect your device in a few lines of code.

## Installation

### Arduino IDE
1. Download this repository as a ZIP file (Code → Download ZIP)
2. In Arduino IDE: **Sketch** → **Include Library** → **Add .ZIP Library**
3. Select the downloaded ZIP

### Manual
Copy the `IoTPaaS` folder into your Arduino libraries directory:
- **Windows:** `C:\Users\YourName\Documents\Arduino\libraries\`
- **Mac:** `~/Documents/Arduino/libraries/`
- **Linux:** `~/Arduino/libraries/`

## Dependencies

Install these via Arduino IDE Library Manager (**Sketch → Include Library → Manage Libraries**):

- **PubSubClient** by Nick O'Leary
- **ArduinoJson** by Benoit Blanchon (v6.x)

## Quick Start

```cpp
#include <IoTPaaS.h>

IoTPaaS iot;

void setup() {
  Serial.begin(115200);

  iot.setWiFi("YourWiFi", "YourPassword");
  iot.setBroker("103.90.225.183", 1883, false);
  iot.setCredentials("USER_ID", "DEVICE_ID", "DEVICE_TOKEN");

  iot.begin();
}

void loop() {
  iot.loop();

  static unsigned long lastPublish = 0;
  if (millis() - lastPublish > 10000 && iot.isConnected()) {
    iot.publish("heartbeat", "alive");
    lastPublish = millis();
  }
}
```

## Getting Your Credentials

1. Sign up at [iot-paas.io.vn](https://iot-paas.io.vn)
2. Create a project → Add a device
3. Copy your **User ID** (Profile page), **Device ID** and **Device Token** (Device Credentials card)

## API Reference

### Configuration
| Method | Description |
|--------|-------------|
| `setWiFi(ssid, password)` | Set WiFi credentials |
| `setBroker(host, port, useSSL)` | Set MQTT broker address |
| `setCredentials(userId, deviceId, token)` | Set IoTPaaS authentication |
| `setLogLevel(level)` | Set logging: `LOG_NONE`, `LOG_ERROR`, `LOG_INFO`, `LOG_DEBUG` |

### Connection
| Method | Description |
|--------|-------------|
| `begin()` | Connect to WiFi and MQTT |
| `loop()` | Must be called in `loop()` — handles MQTT and reconnection |
| `isConnected()` | Returns `true` if MQTT is connected |
| `getStateString()` | Returns current state as a string |

### Messaging
| Method | Description |
|--------|-------------|
| `publish(channel, payload)` | Publish data to a channel |
| `subscribe(channel)` | Subscribe to a control channel |
| `onCommand(callback)` | Register callback for incoming commands: `void cb(const char* channel, const char* payload)` |
| `onStateChange(callback)` | Register callback for state changes: `void cb(IoTPaaSState newState, IoTPaaSState oldState)` |

## MQTT Topic Structure

```
u/{user_id}/d/{device_id}/out/{channel}  ← device publishes sensor data
u/{user_id}/d/{device_id}/in/{channel}   ← device receives commands
```

## Supported Boards

- ESP32 (primary, uses IoTPaaS library)
- ESP8266 (use ESP8266WiFi.h + PubSubClient directly — see [platform docs](https://iot-paas.io.vn/docs))

## Examples

- **BasicHardcoded** — Minimal connection example with hardcoded credentials

## License

MIT

## Links

- **Platform:** [iot-paas.io.vn](https://iot-paas.io.vn)
- **Documentation:** [iot-paas.io.vn/docs](https://iot-paas.io.vn/docs)
- **Dashboard Repo:** [github.com/rstu123/iot-paas-dashboard](https://github.com/rstu123/iot-paas-dashboard)
