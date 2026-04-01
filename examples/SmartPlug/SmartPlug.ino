#include <IoTPaaS.h>

IoTPaaS iot;

void handleCommand(const char* channel, const char* payload) {
  Serial.printf("Command on '%s': %s\n", channel, payload);
}

void handleStateChange(IoTPaaSState newState, IoTPaaSState oldState) {
  Serial.printf("State: %s → %s\n", iot.getStateString(), iot.getStateString());
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  iot.setLogLevel(LOG_DEBUG);
  iot.setWiFi("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
  iot.setBroker("103.90.225.183", 1883, false);
  
  // Paste these from your dashboard device credentials page
  iot.setCredentials(
    "YOUR_USER_ID",      // from the Publish Topic: u/{THIS_PART}/d/...
    "YOUR_DEVICE_ID",    // Device ID (MQTT Username)
    "YOUR_DEVICE_TOKEN"  // Device Token (MQTT Password)
  );

  iot.onCommand(handleCommand);
  iot.onStateChange(handleStateChange);

  iot.begin();
}

void loop() {
  iot.loop();

  // Publish a test message every 10 seconds
  static unsigned long lastPublish = 0;
  if (millis() - lastPublish > 10000 && iot.isConnected()) {
    iot.publish("heartbeat", "alive");
    Serial.println("Published heartbeat");
    lastPublish = millis();
  }
}