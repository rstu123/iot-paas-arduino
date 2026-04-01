/*
 * IoTPaaS - Arduino Library for IoT Platform-as-a-Service
 * Version: 1.1.0 (Week 5 - Polish & OTA)
 * 
 * Simplifies connecting ESP32 devices to the IoT PaaS platform.
 * Handles WiFi, MQTT authentication, topic management, and OTA updates.
 */

#ifndef IOT_PAAS_H
#define IOT_PAAS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>

// =========== Configuration Defaults ===========
#define IOTPAAS_DEFAULT_MQTT_PORT 1883
#define IOTPAAS_DEFAULT_MQTTS_PORT 8883
#define IOTPAAS_RECONNECT_DELAY_MIN 2000    // Initial retry delay (2s)
#define IOTPAAS_RECONNECT_DELAY_MAX 60000   // Max retry delay (60s)
#define IOTPAAS_RECONNECT_MAX_RETRIES 0     // 0 = infinite retries
#define IOTPAAS_MAX_TOPIC_LENGTH 128
#define IOTPAAS_MAX_CHANNELS 8
#define IOTPAAS_MQTT_BUFFER_SIZE 1024
#define IOTPAAS_VERSION "1.1.0"

// =========== Log Levels ===========
enum IoTPaaSLogLevel {
    LOG_NONE  = 0,  // No output
    LOG_ERROR = 1,  // Errors only
    LOG_WARN  = 2,  // Errors + warnings
    LOG_INFO  = 3,  // Normal operational messages
    LOG_DEBUG = 4   // Verbose debugging
};

// =========== Connection States ===========
enum IoTPaaSState {
    STATE_DISCONNECTED,
    STATE_WIFI_CONNECTING,
    STATE_WIFI_CONNECTED,
    STATE_MQTT_CONNECTING,
    STATE_MQTT_CONNECTED,
    STATE_PROVISIONING,
    STATE_OTA_IN_PROGRESS,
    STATE_ERROR
};

// =========== Error Codes ===========
enum IoTPaaSError {
    ERR_NONE = 0,
    ERR_WIFI_FAILED,           // Could not connect to WiFi
    ERR_WIFI_TIMEOUT,          // WiFi connection timed out
    ERR_MQTT_CONNECTION_LOST,  // MQTT broker connection dropped
    ERR_MQTT_CONNECT_FAILED,   // Could not connect to MQTT broker
    ERR_MQTT_AUTH_FAILED,      // Bad credentials (CONNACK code 4/5)
    ERR_MQTT_NOT_AUTHORIZED,   // ACL denied (publish/subscribe rejected)
    ERR_PROVISION_FAILED,      // HTTP provisioning failed
    ERR_PROVISION_REJECTED,    // Server rejected provisioning key
    ERR_NO_CREDENTIALS,        // No credentials configured or stored
    ERR_CONFIG_MISSING,        // WiFi or broker not configured
    ERR_OTA_DOWNLOAD_FAILED,   // Could not download firmware
    ERR_OTA_INVALID_FIRMWARE,  // Firmware validation failed
    ERR_OTA_FLASH_FAILED,      // Writing to flash failed
    ERR_OTA_NO_URL             // OTA triggered but no URL provided
};

// =========== Callback Types ===========
typedef void (*CommandCallback)(const char* channel, const char* payload);
typedef void (*StateCallback)(IoTPaaSState newState, IoTPaaSState oldState);
typedef void (*ErrorCallback)(IoTPaaSError error, const char* message);
typedef void (*OtaProgressCallback)(size_t progress, size_t total);

// =========== Configuration Structure ===========
struct IoTPaaSConfig {
    // WiFi
    const char* wifiSsid;
    const char* wifiPassword;
    
    // MQTT broker
    const char* mqttHost;
    uint16_t mqttPort;
    bool useTLS;
    
    // Device credentials (hardcoded mode)
    const char* userId;
    const char* deviceId;
    const char* deviceToken;
    
    // Auto-provision endpoint
    const char* provisionUrl;
    const char* provisionKey;
    
    // Reconnection behavior
    bool autoReconnect;
    uint32_t reconnectDelayMin;
    uint32_t reconnectDelayMax;
    uint16_t maxRetries;       // 0 = infinite
    
    // Logging
    IoTPaaSLogLevel logLevel;
    
    // OTA
    bool otaEnabled;
};

// =========== Main Class ===========
class IoTPaaS {
public:
    IoTPaaS();
    ~IoTPaaS();
    
    // =========== Configuration ===========
    void setWiFi(const char* ssid, const char* password);
    void setBroker(const char* host, uint16_t port = 1883, bool useTLS = false);
    void setCredentials(const char* userId, const char* deviceId, const char* token);
    void setProvisioning(const char* provisionUrl, const char* provisionKey);
    void setAutoReconnect(bool enabled, uint32_t minDelayMs = IOTPAAS_RECONNECT_DELAY_MIN,
                          uint32_t maxDelayMs = IOTPAAS_RECONNECT_DELAY_MAX,
                          uint16_t maxRetries = IOTPAAS_RECONNECT_MAX_RETRIES);
    void setLogLevel(IoTPaaSLogLevel level);
    void enableOTA(bool enabled = true);
    
    // =========== Callbacks ===========
    void onCommand(CommandCallback callback);
    void onStateChange(StateCallback callback);
    void onError(ErrorCallback callback);
    void onOtaProgress(OtaProgressCallback callback);
    
    // =========== Connection ===========
    bool begin();
    void loop();
    void disconnect();
    bool isConnected();
    IoTPaaSState getState();
    const char* getStateString();
    const char* getErrorString(IoTPaaSError error);
    
    // =========== Messaging ===========
    bool subscribe(const char* channel);
    bool subscribeAll();
    bool publish(const char* channel, const char* payload);
    bool publish(const char* channel, float value);
    bool publish(const char* channel, int value);
    bool publish(const char* channel, bool value);
    bool publishJson(const char* channel, JsonDocument& doc);
    
    // =========== Device Info ===========
    const char* getUserId();
    const char* getDeviceId();
    const char* getVersion();
    uint16_t getReconnectCount();
    unsigned long getUptime();
    
private:
    // Configuration
    IoTPaaSConfig _config;
    bool _configured;
    bool _credentialsSet;
    bool _provisioningSet;
    
    // Stored credentials (from provisioning)
    char _storedUserId[64];
    char _storedDeviceId[64];
    char _storedToken[128];
    
    // Connection state
    IoTPaaSState _state;
    unsigned long _lastReconnectAttempt;
    uint32_t _currentReconnectDelay;
    uint16_t _reconnectCount;
    uint16_t _totalReconnects;
    unsigned long _startTime;
    
    // MQTT clients
    WiFiClient _wifiClient;
    WiFiClientSecure _wifiClientSecure;
    PubSubClient _mqttClient;
    
    // Callbacks
    CommandCallback _commandCallback;
    StateCallback _stateCallback;
    ErrorCallback _errorCallback;
    OtaProgressCallback _otaProgressCallback;
    
    // Subscribed channels tracking
    char _subscribedChannels[IOTPAAS_MAX_CHANNELS][32];
    uint8_t _subscribedCount;
    
    // Persistent storage
    Preferences _preferences;
    
    // Internal methods
    bool connectWiFi();
    bool connectMQTT();
    bool provision();
    bool loadStoredCredentials();
    bool saveCredentials(const char* userId, const char* deviceId, const char* token);
    void clearCredentials();
    
    void setState(IoTPaaSState newState);
    void reportError(IoTPaaSError error, const char* message = nullptr);
    void resetReconnectDelay();
    void increaseReconnectDelay();
    
    void buildTopic(char* buffer, const char* direction, const char* channel);
    void buildSubscribeTopic(char* buffer, const char* channel);
    void buildPublishTopic(char* buffer, const char* channel);
    
    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void extractChannelFromTopic(const char* topic, char* channel);
    
    // OTA
    void handleOtaCommand(const char* payload);
    bool performOTA(const char* firmwareUrl);
    
    // Logging helpers
    void log(IoTPaaSLogLevel level, const char* format, ...);
};

#endif // IOT_PAAS_H
