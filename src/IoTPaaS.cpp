/*
 * IoTPaaS - Implementation
 * Version: 1.1.0 (Week 5 - Polish & OTA)
 */

#include "IoTPaaS.h"
#include <stdarg.h>

// Static instance pointer for MQTT callback routing
static IoTPaaS* _instance = nullptr;

// =========================================================
//  Constructor / Destructor
// =========================================================

IoTPaaS::IoTPaaS() : _mqttClient(_wifiClient) {
    _instance = this;
    
    memset(&_config, 0, sizeof(_config));
    _config.mqttPort = IOTPAAS_DEFAULT_MQTT_PORT;
    _config.useTLS = false;
    _config.autoReconnect = true;
    _config.reconnectDelayMin = IOTPAAS_RECONNECT_DELAY_MIN;
    _config.reconnectDelayMax = IOTPAAS_RECONNECT_DELAY_MAX;
    _config.maxRetries = IOTPAAS_RECONNECT_MAX_RETRIES;
    _config.logLevel = LOG_INFO;
    _config.otaEnabled = true;
    
    _configured = false;
    _credentialsSet = false;
    _provisioningSet = false;
    _state = STATE_DISCONNECTED;
    _lastReconnectAttempt = 0;
    _currentReconnectDelay = IOTPAAS_RECONNECT_DELAY_MIN;
    _reconnectCount = 0;
    _totalReconnects = 0;
    _startTime = 0;
    
    _commandCallback = nullptr;
    _stateCallback = nullptr;
    _errorCallback = nullptr;
    _otaProgressCallback = nullptr;
    _subscribedCount = 0;
    
    memset(_storedUserId, 0, sizeof(_storedUserId));
    memset(_storedDeviceId, 0, sizeof(_storedDeviceId));
    memset(_storedToken, 0, sizeof(_storedToken));
}

IoTPaaS::~IoTPaaS() {
    disconnect();
    _instance = nullptr;
}

// =========================================================
//  Logging
// =========================================================

void IoTPaaS::log(IoTPaaSLogLevel level, const char* message, ...) {
    if (level > _config.logLevel) return;
    
    const char* prefix;
    switch (level) {
        case LOG_ERROR: prefix = "[IoTPaaS ERROR] "; break;
        case LOG_WARN:  prefix = "[IoTPaaS WARN]  "; break;
        case LOG_INFO:  prefix = "[IoTPaaS INFO]  "; break;
        case LOG_DEBUG: prefix = "[IoTPaaS DEBUG] "; break;
        default: return;
    }
    
    Serial.print(prefix);
    
    char buf[256];
    va_list args;
    va_start(args, message);
    vsnprintf(buf, sizeof(buf), message, args);
    va_end(args);
    
    Serial.println(buf);
}

// =========================================================
//  Configuration Methods
// =========================================================

void IoTPaaS::setWiFi(const char* ssid, const char* password) {
    _config.wifiSsid = ssid;
    _config.wifiPassword = password;
}

void IoTPaaS::setBroker(const char* host, uint16_t port, bool useTLS) {
    _config.mqttHost = host;
    _config.mqttPort = port;
    _config.useTLS = useTLS;
}

void IoTPaaS::setCredentials(const char* userId, const char* deviceId, const char* token) {
    _config.userId = userId;
    _config.deviceId = deviceId;
    _config.deviceToken = token;
    _credentialsSet = true;
}

void IoTPaaS::setProvisioning(const char* provisionUrl, const char* provisionKey) {
    _config.provisionUrl = provisionUrl;
    _config.provisionKey = provisionKey;
    _provisioningSet = true;
}

void IoTPaaS::setAutoReconnect(bool enabled, uint32_t minDelayMs,
                                uint32_t maxDelayMs, uint16_t maxRetries) {
    _config.autoReconnect = enabled;
    _config.reconnectDelayMin = minDelayMs;
    _config.reconnectDelayMax = maxDelayMs;
    _config.maxRetries = maxRetries;
}

void IoTPaaS::setLogLevel(IoTPaaSLogLevel level) {
    _config.logLevel = level;
}

void IoTPaaS::enableOTA(bool enabled) {
    _config.otaEnabled = enabled;
}

// =========================================================
//  Callbacks
// =========================================================

void IoTPaaS::onCommand(CommandCallback callback) {
    _commandCallback = callback;
}

void IoTPaaS::onStateChange(StateCallback callback) {
    _stateCallback = callback;
}

void IoTPaaS::onError(ErrorCallback callback) {
    _errorCallback = callback;
}

void IoTPaaS::onOtaProgress(OtaProgressCallback callback) {
    _otaProgressCallback = callback;
}

// =========================================================
//  State Management
// =========================================================

void IoTPaaS::setState(IoTPaaSState newState) {
    if (newState == _state) return;
    
    IoTPaaSState oldState = _state;
    _state = newState;
    
    log(LOG_DEBUG, "State changed to: %s", getStateString());
    
    if (_stateCallback) {
        _stateCallback(newState, oldState);
    }
}

void IoTPaaS::reportError(IoTPaaSError error, const char* message) {
    const char* errMsg = message ? message : getErrorString(error);
    log(LOG_ERROR, "%s", errMsg);
    
    if (_errorCallback) {
        _errorCallback(error, errMsg);
    }
}

void IoTPaaS::resetReconnectDelay() {
    _currentReconnectDelay = _config.reconnectDelayMin;
    _reconnectCount = 0;
}

void IoTPaaS::increaseReconnectDelay() {
    _reconnectCount++;
    _totalReconnects++;
    // Exponential backoff: double the delay each attempt, up to max
    _currentReconnectDelay = min(_currentReconnectDelay * 2, _config.reconnectDelayMax);
    log(LOG_DEBUG, "Reconnect attempt %d, next delay: %lums", _reconnectCount, _currentReconnectDelay);
}

// =========================================================
//  Connection Methods
// =========================================================

bool IoTPaaS::begin() {
    log(LOG_INFO, "Initializing IoTPaaS v%s", IOTPAAS_VERSION);
    _startTime = millis();
    
    // Validate configuration
    if (!_config.wifiSsid || !_config.mqttHost) {
        reportError(ERR_CONFIG_MISSING);
        setState(STATE_ERROR);
        return false;
    }
    
    // Check credential source
    if (!_credentialsSet && !_provisioningSet) {
        if (!loadStoredCredentials()) {
            reportError(ERR_NO_CREDENTIALS);
            setState(STATE_ERROR);
            return false;
        }
    }
    
    // Connect to WiFi
    if (!connectWiFi()) {
        return false;
    }
    
    // If using auto-provisioning and no stored credentials, provision now
    if (_provisioningSet && !_credentialsSet && strlen(_storedDeviceId) == 0) {
        setState(STATE_PROVISIONING);
        if (!provision()) {
            reportError(ERR_PROVISION_FAILED);
            setState(STATE_ERROR);
            return false;
        }
    }
    
    // Configure MQTT client
    if (_config.useTLS) {
        _wifiClientSecure.setInsecure(); // Skip cert validation (dev mode)
        _mqttClient.setClient(_wifiClientSecure);
    } else {
        _mqttClient.setClient(_wifiClient);
    }
    
    _mqttClient.setServer(_config.mqttHost, _config.mqttPort);
    _mqttClient.setCallback(mqttCallbackStatic);
    _mqttClient.setBufferSize(IOTPAAS_MQTT_BUFFER_SIZE);
    
    // Connect to MQTT
    if (!connectMQTT()) {
        return false;
    }
    
    // Subscribe to system OTA channel if enabled
    if (_config.otaEnabled) {
        subscribe("$ota");
        log(LOG_DEBUG, "OTA updates enabled");
    }
    
    _configured = true;
    resetReconnectDelay();
    log(LOG_INFO, "Ready! Device: %s", getDeviceId());
    return true;
}

bool IoTPaaS::connectWiFi() {
    setState(STATE_WIFI_CONNECTING);
    log(LOG_INFO, "Connecting to WiFi: %s", _config.wifiSsid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(_config.wifiSsid, _config.wifiPassword);
    
    unsigned long startAttempt = millis();
    const unsigned long timeout = 15000; // 15 second timeout
    
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startAttempt > timeout) {
            reportError(ERR_WIFI_TIMEOUT, "WiFi connection timed out (15s)");
            setState(STATE_ERROR);
            return false;
        }
        delay(500);
        if (_config.logLevel >= LOG_DEBUG) Serial.print(".");
    }
    
    if (_config.logLevel >= LOG_DEBUG) Serial.println();
    setState(STATE_WIFI_CONNECTED);
    log(LOG_INFO, "WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

bool IoTPaaS::connectMQTT() {
    setState(STATE_MQTT_CONNECTING);
    
    // Determine credentials to use
    const char* clientId;
    const char* username;
    const char* password;
    
    if (_credentialsSet) {
        clientId = _config.deviceId;
        username = _config.deviceId;
        password = _config.deviceToken;
    } else {
        clientId = _storedDeviceId;
        username = _storedDeviceId;
        password = _storedToken;
    }
    
    log(LOG_INFO, "Connecting to MQTT: %s:%d", _config.mqttHost, _config.mqttPort);
    log(LOG_DEBUG, "Client ID: %s", clientId);
    
    if (_mqttClient.connect(clientId, username, password)) {
        setState(STATE_MQTT_CONNECTED);
        resetReconnectDelay();
        log(LOG_INFO, "MQTT connected!");
        
        // Resubscribe to tracked channels
        for (uint8_t i = 0; i < _subscribedCount; i++) {
            char topic[IOTPAAS_MAX_TOPIC_LENGTH];
            buildSubscribeTopic(topic, _subscribedChannels[i]);
            _mqttClient.subscribe(topic);
            log(LOG_DEBUG, "Resubscribed: %s", topic);
        }
        return true;
    }
    
    // Decode MQTT connection failure reason
    int state = _mqttClient.state();
    switch (state) {
        case -4:
            reportError(ERR_MQTT_CONNECT_FAILED, "MQTT connection timeout");
            break;
        case -2:
            reportError(ERR_MQTT_CONNECT_FAILED, "MQTT connection failed (network)");
            break;
        case 4:
            reportError(ERR_MQTT_AUTH_FAILED, "MQTT bad credentials (check device token)");
            break;
        case 5:
            reportError(ERR_MQTT_NOT_AUTHORIZED, "MQTT not authorized (check ACL)");
            break;
        default:
            char msg[64];
            snprintf(msg, sizeof(msg), "MQTT connect failed (code: %d)", state);
            reportError(ERR_MQTT_CONNECT_FAILED, msg);
            break;
    }
    
    setState(STATE_ERROR);
    return false;
}

void IoTPaaS::loop() {
    // Handle WiFi reconnection
    if (WiFi.status() != WL_CONNECTED) {
        if (_state == STATE_MQTT_CONNECTED || _state == STATE_WIFI_CONNECTED) {
            setState(STATE_DISCONNECTED);
            reportError(ERR_WIFI_FAILED, "WiFi connection lost");
        }
        
        if (_config.autoReconnect) {
            // Check max retries
            if (_config.maxRetries > 0 && _reconnectCount >= _config.maxRetries) {
                if (_state != STATE_ERROR) {
                    reportError(ERR_WIFI_FAILED, "Max reconnection attempts reached");
                    setState(STATE_ERROR);
                }
                return;
            }
            
            unsigned long now = millis();
            if (now - _lastReconnectAttempt > _currentReconnectDelay) {
                _lastReconnectAttempt = now;
                log(LOG_WARN, "WiFi lost, reconnecting (attempt %d)...", _reconnectCount + 1);
                if (connectWiFi()) {
                    resetReconnectDelay();
                } else {
                    increaseReconnectDelay();
                }
            }
        }
        return;
    }
    
    // Handle MQTT reconnection
    if (!_mqttClient.connected()) {
        if (_state == STATE_MQTT_CONNECTED) {
            setState(STATE_WIFI_CONNECTED);
            reportError(ERR_MQTT_CONNECTION_LOST, "MQTT broker connection lost");
        }
        
        if (_config.autoReconnect) {
            if (_config.maxRetries > 0 && _reconnectCount >= _config.maxRetries) {
                if (_state != STATE_ERROR) {
                    reportError(ERR_MQTT_CONNECT_FAILED, "Max reconnection attempts reached");
                    setState(STATE_ERROR);
                }
                return;
            }
            
            unsigned long now = millis();
            if (now - _lastReconnectAttempt > _currentReconnectDelay) {
                _lastReconnectAttempt = now;
                log(LOG_WARN, "MQTT lost, reconnecting (attempt %d)...", _reconnectCount + 1);
                if (connectMQTT()) {
                    resetReconnectDelay();
                } else {
                    increaseReconnectDelay();
                }
            }
        }
        return;
    }
    
    _mqttClient.loop();
}

void IoTPaaS::disconnect() {
    if (_mqttClient.connected()) {
        _mqttClient.disconnect();
    }
    WiFi.disconnect();
    setState(STATE_DISCONNECTED);
    log(LOG_INFO, "Disconnected");
}

bool IoTPaaS::isConnected() {
    return _state == STATE_MQTT_CONNECTED && _mqttClient.connected();
}

IoTPaaSState IoTPaaS::getState() {
    return _state;
}

const char* IoTPaaS::getStateString() {
    switch (_state) {
        case STATE_DISCONNECTED:    return "DISCONNECTED";
        case STATE_WIFI_CONNECTING: return "WIFI_CONNECTING";
        case STATE_WIFI_CONNECTED:  return "WIFI_CONNECTED";
        case STATE_MQTT_CONNECTING: return "MQTT_CONNECTING";
        case STATE_MQTT_CONNECTED:  return "CONNECTED";
        case STATE_PROVISIONING:    return "PROVISIONING";
        case STATE_OTA_IN_PROGRESS: return "OTA_IN_PROGRESS";
        case STATE_ERROR:           return "ERROR";
        default:                    return "UNKNOWN";
    }
}

const char* IoTPaaS::getErrorString(IoTPaaSError error) {
    switch (error) {
        case ERR_NONE:                return "No error";
        case ERR_WIFI_FAILED:         return "WiFi connection failed";
        case ERR_WIFI_TIMEOUT:        return "WiFi connection timed out";
        case ERR_MQTT_CONNECTION_LOST:return "MQTT connection lost";
        case ERR_MQTT_CONNECT_FAILED: return "MQTT connection failed";
        case ERR_MQTT_AUTH_FAILED:    return "MQTT authentication failed (bad credentials)";
        case ERR_MQTT_NOT_AUTHORIZED: return "MQTT not authorized (ACL denied)";
        case ERR_PROVISION_FAILED:    return "Device provisioning failed";
        case ERR_PROVISION_REJECTED:  return "Provisioning key rejected";
        case ERR_NO_CREDENTIALS:      return "No device credentials available";
        case ERR_CONFIG_MISSING:      return "WiFi or broker not configured";
        case ERR_OTA_DOWNLOAD_FAILED: return "OTA firmware download failed";
        case ERR_OTA_INVALID_FIRMWARE:return "OTA firmware validation failed";
        case ERR_OTA_FLASH_FAILED:    return "OTA flash write failed";
        case ERR_OTA_NO_URL:          return "OTA triggered but no firmware URL";
        default:                      return "Unknown error";
    }
}

// =========================================================
//  Messaging Methods
// =========================================================

bool IoTPaaS::subscribe(const char* channel) {
    if (!isConnected()) {
        log(LOG_WARN, "Cannot subscribe: not connected");
        return false;
    }
    
    char topic[IOTPAAS_MAX_TOPIC_LENGTH];
    buildSubscribeTopic(topic, channel);
    
    bool success = _mqttClient.subscribe(topic);
    if (success) {
        log(LOG_INFO, "Subscribed: %s", channel);
        
        // Track for resubscribe on reconnect (avoid duplicates)
        bool alreadyTracked = false;
        for (uint8_t i = 0; i < _subscribedCount; i++) {
            if (strcmp(_subscribedChannels[i], channel) == 0) {
                alreadyTracked = true;
                break;
            }
        }
        if (!alreadyTracked && _subscribedCount < IOTPAAS_MAX_CHANNELS) {
            strncpy(_subscribedChannels[_subscribedCount], channel, 31);
            _subscribedChannels[_subscribedCount][31] = '\0';
            _subscribedCount++;
        }
    } else {
        log(LOG_ERROR, "Subscribe failed: %s", channel);
    }
    return success;
}

bool IoTPaaS::subscribeAll() {
    if (!isConnected()) {
        log(LOG_WARN, "Cannot subscribe: not connected");
        return false;
    }
    
    char topic[IOTPAAS_MAX_TOPIC_LENGTH];
    buildSubscribeTopic(topic, "+");
    
    bool success = _mqttClient.subscribe(topic);
    if (success) {
        log(LOG_INFO, "Subscribed to all channels (wildcard)");
    }
    return success;
}

bool IoTPaaS::publish(const char* channel, const char* payload) {
    if (!isConnected()) {
        log(LOG_WARN, "Cannot publish: not connected");
        return false;
    }
    
    char topic[IOTPAAS_MAX_TOPIC_LENGTH];
    buildPublishTopic(topic, channel);
    
    bool success = _mqttClient.publish(topic, payload);
    if (success) {
        log(LOG_DEBUG, "Published [%s]: %s", channel, payload);
    } else {
        log(LOG_ERROR, "Publish failed [%s]", channel);
        reportError(ERR_MQTT_NOT_AUTHORIZED, "Publish rejected — check ACL rules");
    }
    return success;
}

bool IoTPaaS::publish(const char* channel, float value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", value);
    return publish(channel, buf);
}

bool IoTPaaS::publish(const char* channel, int value) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", value);
    return publish(channel, buf);
}

bool IoTPaaS::publish(const char* channel, bool value) {
    return publish(channel, value ? "true" : "false");
}

bool IoTPaaS::publishJson(const char* channel, JsonDocument& doc) {
    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) {
        log(LOG_ERROR, "JSON serialization failed");
        return false;
    }
    return publish(channel, buf);
}

// =========================================================
//  Device Info
// =========================================================

const char* IoTPaaS::getUserId() {
    return _credentialsSet ? _config.userId : _storedUserId;
}

const char* IoTPaaS::getDeviceId() {
    return _credentialsSet ? _config.deviceId : _storedDeviceId;
}

const char* IoTPaaS::getVersion() {
    return IOTPAAS_VERSION;
}

uint16_t IoTPaaS::getReconnectCount() {
    return _totalReconnects;
}

unsigned long IoTPaaS::getUptime() {
    return millis() - _startTime;
}

// =========================================================
//  Topic Helpers
// =========================================================

void IoTPaaS::buildTopic(char* buffer, const char* direction, const char* channel) {
    const char* uid = getUserId();
    const char* did = getDeviceId();
    snprintf(buffer, IOTPAAS_MAX_TOPIC_LENGTH, "u/%s/d/%s/%s/%s", uid, did, direction, channel);
}

void IoTPaaS::buildSubscribeTopic(char* buffer, const char* channel) {
    buildTopic(buffer, "in", channel);
}

void IoTPaaS::buildPublishTopic(char* buffer, const char* channel) {
    buildTopic(buffer, "out", channel);
}

// =========================================================
//  MQTT Callback
// =========================================================

void IoTPaaS::mqttCallbackStatic(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->mqttCallback(topic, payload, length);
    }
}

void IoTPaaS::mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Null-terminate payload
    char message[512];
    size_t copyLen = min((unsigned int)(sizeof(message) - 1), length);
    memcpy(message, payload, copyLen);
    message[copyLen] = '\0';
    
    log(LOG_DEBUG, "Received [%s]: %s", topic, message);
    
    // Extract channel from topic
    char channel[32];
    extractChannelFromTopic(topic, channel);
    
    // Handle OTA system command
    if (_config.otaEnabled && strcmp(channel, "$ota") == 0) {
        handleOtaCommand(message);
        return;
    }
    
    // Forward to user callback
    if (_commandCallback) {
        _commandCallback(channel, message);
    }
}

void IoTPaaS::extractChannelFromTopic(const char* topic, char* channel) {
    // Topic format: u/{uid}/d/{did}/in/{channel}
    // Find the 5th '/' and extract everything after it
    int slashCount = 0;
    const char* p = topic;
    while (*p && slashCount < 5) {
        if (*p == '/') slashCount++;
        p++;
    }
    if (slashCount == 5) {
        strncpy(channel, p, 31);
        channel[31] = '\0';
    } else {
        channel[0] = '\0';
    }
}

// =========================================================
//  OTA Updates
// =========================================================

void IoTPaaS::handleOtaCommand(const char* payload) {
    log(LOG_INFO, "OTA command received");
    
    // Parse JSON: {"action":"ota", "url":"https://...", "version":"1.0.1"}
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        log(LOG_ERROR, "OTA: Invalid JSON payload");
        reportError(ERR_OTA_NO_URL, "OTA command has invalid JSON");
        return;
    }
    
    const char* url = doc["url"];
    const char* version = doc["version"] | "unknown";
    
    if (!url || strlen(url) == 0) {
        reportError(ERR_OTA_NO_URL);
        return;
    }
    
    log(LOG_INFO, "OTA: Updating to version %s", version);
    log(LOG_INFO, "OTA: Firmware URL: %s", url);
    
    // Report status back to platform
    char statusMsg[128];
    snprintf(statusMsg, sizeof(statusMsg), "{\"status\":\"downloading\",\"version\":\"%s\"}", version);
    publish("$ota_status", statusMsg);
    
    if (performOTA(url)) {
        // If we get here, OTA failed (success would reboot)
        log(LOG_ERROR, "OTA: Update did not trigger reboot");
    }
}

bool IoTPaaS::performOTA(const char* firmwareUrl) {
    setState(STATE_OTA_IN_PROGRESS);
    
    HTTPClient http;
    http.begin(firmwareUrl);
    http.setTimeout(30000); // 30 second timeout
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        char msg[64];
        snprintf(msg, sizeof(msg), "OTA download failed (HTTP %d)", httpCode);
        reportError(ERR_OTA_DOWNLOAD_FAILED, msg);
        setState(STATE_MQTT_CONNECTED);
        
        // Report failure
        publish("$ota_status", "{\"status\":\"failed\",\"reason\":\"download_error\"}");
        return false;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        reportError(ERR_OTA_INVALID_FIRMWARE, "OTA firmware size is 0");
        setState(STATE_MQTT_CONNECTED);
        publish("$ota_status", "{\"status\":\"failed\",\"reason\":\"empty_firmware\"}");
        return false;
    }
    
    log(LOG_INFO, "OTA: Firmware size: %d bytes", contentLength);
    
    if (!Update.begin(contentLength)) {
        reportError(ERR_OTA_FLASH_FAILED, "OTA: Not enough space for firmware");
        setState(STATE_MQTT_CONNECTED);
        publish("$ota_status", "{\"status\":\"failed\",\"reason\":\"no_space\"}");
        return false;
    }
    
    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buf[1024];
    
    while (http.connected() && written < (size_t)contentLength) {
        size_t available = stream->available();
        if (available) {
            size_t readBytes = stream->readBytes(buf, min(available, sizeof(buf)));
            size_t wroteBytes = Update.write(buf, readBytes);
            if (wroteBytes != readBytes) {
                reportError(ERR_OTA_FLASH_FAILED, "OTA: Flash write mismatch");
                Update.abort();
                setState(STATE_MQTT_CONNECTED);
                publish("$ota_status", "{\"status\":\"failed\",\"reason\":\"write_error\"}");
                return false;
            }
            written += wroteBytes;
            
            // Progress callback
            if (_otaProgressCallback) {
                _otaProgressCallback(written, contentLength);
            }
            
            // Log progress every 10%
            int percent = (written * 100) / contentLength;
            static int lastPercent = -1;
            if (percent / 10 != lastPercent / 10) {
                lastPercent = percent;
                log(LOG_INFO, "OTA: %d%%", percent);
            }
        }
        delay(1);
    }
    
    if (Update.end(true)) {
        log(LOG_INFO, "OTA: Update successful! Rebooting...");
        publish("$ota_status", "{\"status\":\"success\",\"rebooting\":true}");
        delay(500); // Give MQTT time to send
        ESP.restart();
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "OTA: Finalize failed (err: %d)", Update.getError());
        reportError(ERR_OTA_FLASH_FAILED, msg);
        setState(STATE_MQTT_CONNECTED);
        publish("$ota_status", "{\"status\":\"failed\",\"reason\":\"finalize_error\"}");
    }
    
    return false;
}

// =========================================================
//  Provisioning
// =========================================================

bool IoTPaaS::provision() {
    log(LOG_INFO, "Starting auto-provisioning...");
    
    HTTPClient http;
    http.begin(_config.provisionUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    
    // Build provision request
    JsonDocument doc;
    doc["provision_key"] = _config.provisionKey;
    doc["chip_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
    doc["firmware_version"] = IOTPAAS_VERSION;
    
    char body[256];
    serializeJson(doc, body, sizeof(body));
    
    log(LOG_DEBUG, "Provision request: %s", body);
    
    int httpCode = http.POST(body);
    if (httpCode != 200 && httpCode != 201) {
        if (httpCode == 401 || httpCode == 403) {
            reportError(ERR_PROVISION_REJECTED);
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "Provision HTTP error: %d", httpCode);
            reportError(ERR_PROVISION_FAILED, msg);
        }
        http.end();
        return false;
    }
    
    String response = http.getString();
    http.end();
    
    // Parse response
    JsonDocument respDoc;
    DeserializationError err = deserializeJson(respDoc, response);
    if (err) {
        reportError(ERR_PROVISION_FAILED, "Invalid provision response JSON");
        return false;
    }
    
    const char* userId = respDoc["user_id"];
    const char* deviceId = respDoc["device_id"];
    const char* token = respDoc["device_token"];
    
    if (!userId || !deviceId || !token) {
        reportError(ERR_PROVISION_FAILED, "Provision response missing fields");
        return false;
    }
    
    // Store credentials
    saveCredentials(userId, deviceId, token);
    
    log(LOG_INFO, "Provisioned! Device: %s", deviceId);
    return true;
}

bool IoTPaaS::loadStoredCredentials() {
    _preferences.begin("iotpaas", true); // read-only
    
    String uid = _preferences.getString("user_id", "");
    String did = _preferences.getString("device_id", "");
    String tok = _preferences.getString("token", "");
    
    _preferences.end();
    
    if (uid.length() == 0 || did.length() == 0 || tok.length() == 0) {
        log(LOG_DEBUG, "No stored credentials found");
        return false;
    }
    
    strncpy(_storedUserId, uid.c_str(), sizeof(_storedUserId) - 1);
    strncpy(_storedDeviceId, did.c_str(), sizeof(_storedDeviceId) - 1);
    strncpy(_storedToken, tok.c_str(), sizeof(_storedToken) - 1);
    
    log(LOG_DEBUG, "Loaded stored credentials for device: %s", _storedDeviceId);
    return true;
}

bool IoTPaaS::saveCredentials(const char* userId, const char* deviceId, const char* token) {
    strncpy(_storedUserId, userId, sizeof(_storedUserId) - 1);
    strncpy(_storedDeviceId, deviceId, sizeof(_storedDeviceId) - 1);
    strncpy(_storedToken, token, sizeof(_storedToken) - 1);
    
    _preferences.begin("iotpaas", false); // read-write
    _preferences.putString("user_id", userId);
    _preferences.putString("device_id", deviceId);
    _preferences.putString("token", token);
    _preferences.end();
    
    log(LOG_DEBUG, "Credentials saved to flash");
    return true;
}

void IoTPaaS::clearCredentials() {
    memset(_storedUserId, 0, sizeof(_storedUserId));
    memset(_storedDeviceId, 0, sizeof(_storedDeviceId));
    memset(_storedToken, 0, sizeof(_storedToken));
    
    _preferences.begin("iotpaas", false);
    _preferences.clear();
    _preferences.end();
    
    log(LOG_INFO, "Stored credentials cleared");
}
