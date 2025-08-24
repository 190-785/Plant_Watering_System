#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// ===== File System Configuration ===== //
const char* CONFIG_FILE = "/config.json";

// ===== Firestore Configuration ===== //
String firebaseProjectId = "plantirrigation-7645a"; // Your Firebase project ID
String firebaseApiKey = "AIzaSyAu1hcRbSuOvLNfAeyfIkQ2yuYbzp2OLEY"; // Your Web API key from Firebase project settings
String firestoreEndpoint = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + "/databases/(default)/documents/";

// ===== Data Logging Configuration ===== //
unsigned long lastDataSend = 0;
unsigned long lastConfigCheck = 0;
const unsigned long DATA_SEND_INTERVAL = 10000; // Send data every 10 seconds
const unsigned long CONFIG_CHECK_INTERVAL = 30000; // Check for config updates every 30 seconds

// ===== Web Server ===== //
ESP8266WebServer server(80);

// WiFiManager instance
WiFiManager wm;

// WiFi state tracking
bool wifiConnected = false;
const int MAX_CONNECTION_ATTEMPTS = 3;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 10000;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
unsigned long lastInternetCheck = 0;
const unsigned long INTERNET_CHECK_INTERVAL = 300000;
int consecutiveFailures = 0;
const int MAX_CONSECUTIVE_FAILURES = 3;

// ===== Irrigation System Constants ===== //
constexpr uint8_t PUMP_CTRL_PIN = D1;
constexpr uint8_t SENSOR_PIN = A0;
// NEW: Define pins for the button and LED
constexpr uint8_t BUTTON_PIN = D2;
constexpr uint8_t LED_PIN = D3;

uint16_t DRY_THRESHOLD = 520;
uint16_t WET_THRESHOLD = 420;

// Timing variables
unsigned long lastDisplayTime = 0;
unsigned long lastPumpActionTime = 0;
const unsigned long DISPLAY_INTERVAL = 3000;
unsigned long PUMP_RUN_TIME = 2000;
const unsigned long PUMP_WAIT_TIME = 60000;

// Pump state variables
enum PumpState { MONITORING, PUMP_RUNNING, PUMP_WAITING, MANUAL_PUMPING }; // NEW: Added MANUAL_PUMPING state
PumpState currentState = MONITORING;
unsigned long pumpStartTime = 0;

// NEW: Button state tracking
bool buttonPressed = false;
unsigned long lastButtonCheck = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// NEW: LED state tracking
unsigned long lastBlinkTime = 0;
bool ledState = LOW;

// Function declarations
void startConfigurationPortal();
void connectWiFi();
void checkWiFi();
bool checkInternet();
void sendDataToFirestore(uint16_t moisture, const String& pumpStatus);
void updateMainDeviceStatus(uint16_t moisture, const String& pumpStatus, const String& endpoint);
void checkForConfigUpdates();
void updateDeviceConfig(uint16_t dryThresh, uint16_t wetThresh, unsigned long pumpTime, unsigned long dataInterval, bool deviceEnabled, bool autoMode);
void setupWebServer();
void handleRoot();
void handleGetStatus();
void handleSetThreshold();
void handleSetPumpTime();
void handleTestFirestore();
void handleResetConfig();
// NEW: Function to handle LED status
void updateLedStatus();
// NEW: Offline-first helpers
void runWateringLogic();
void handleWiFi();

// ===== File System Functions ===== //
bool saveConfig(const String& ssid, const String& pass, const String& fbProjectId = "", const String& fbApiKey = "",
                uint16_t dryThresh = 520, uint16_t wetThresh = 420, unsigned long pumpTime = 2000) {
    File configFile = LittleFS.open(CONFIG_FILE, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    doc["firebaseProjectId"] = fbProjectId;
    doc["firebaseApiKey"] = fbApiKey;
    doc["dryThreshold"] = dryThresh;
    doc["wetThreshold"] = wetThresh;
    doc["pumpRunTime"] = pumpTime;

    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Failed to write to config file");
        configFile.close();
        return false;
    }

    configFile.close();
    return true;
}

bool loadConfig(String& ssid, String& pass, String& fbProjectId, String& fbApiKey,
                uint16_t& dryThresh, uint16_t& wetThresh, unsigned long& pumpTime) {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("No config file found");
        return false;
    }

    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (!configFile) {
        Serial.println("Failed to open config file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        configFile.close();
        return false;
    }

    ssid = doc["ssid"].as<String>();
    pass = doc["pass"].as<String>();
    fbProjectId = doc["firebaseProjectId"].as<String>();
    fbApiKey = doc["firebaseApiKey"].as<String>();
    dryThresh = doc["dryThreshold"] | 520;
    wetThresh = doc["wetThreshold"] | 420;
    pumpTime = doc["pumpRunTime"] | 2000;

    configFile.close();
    return true;
}


// ===== WiFi Functions ===== //
void setupWiFi() {
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(180);
}

void startConfigurationPortal() {
    Serial.println("Starting configuration portal...");
    
    WiFi.disconnect(true);
    delay(1000);
    
    if (!wm.startConfigPortal("IrrigationAP", "plant123456")) {
        Serial.println("Failed to connect and hit timeout");
        ESP.restart();
        return;
    }
    
    String ssid = WiFi.SSID();
    String pass = WiFi.psk();
    
    Serial.println("Saving new config with Firebase credentials...");
    if (saveConfig(ssid, pass, firebaseProjectId, firebaseApiKey, 
                   DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME)) {
        Serial.println("Credentials saved to LittleFS");
        wifiConnected = true;
        consecutiveFailures = 0;
    } else {
        Serial.println("Failed to save credentials");
    }
}

void connectWiFi() {
    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
        Serial.println("Too many consecutive failures, starting portal");
        startConfigurationPortal();
        return;
    }
    
    Serial.println("Attempting WiFi connection...");
    
    if (LittleFS.exists(CONFIG_FILE)) {
        Serial.println("Deleting old config file to force reset...");
        LittleFS.remove(CONFIG_FILE);
    }
    
    String ssid, pass, fbProjectId, fbApiKey;
    uint16_t dryThresh, wetThresh;
    unsigned long pumpTime;
    
    bool configLoaded = loadConfig(ssid, pass, fbProjectId, fbApiKey, dryThresh, wetThresh, pumpTime);
    
    if (!configLoaded || fbProjectId.length() == 0 || fbApiKey.length() == 0) {
        Serial.println("Config file missing or incomplete - starting configuration portal");
        startConfigurationPortal();
        return;
    }
    
    if (fbProjectId.length() > 0) {
        firebaseProjectId = fbProjectId;
    } else {
        saveConfig(ssid, pass, firebaseProjectId, firebaseApiKey, dryThresh, wetThresh, pumpTime);
    }
    if (fbApiKey.length() > 0) {
        firebaseApiKey = fbApiKey;
    }
    DRY_THRESHOLD = dryThresh;
    WET_THRESHOLD = wetThresh;
    PUMP_RUN_TIME = pumpTime;

    for (int attempt = 1; attempt <= MAX_CONNECTION_ATTEMPTS; attempt++) {
        Serial.printf("Connection attempt %d/%d\n", attempt, MAX_CONNECTION_ATTEMPTS);
        WiFi.begin(ssid.c_str(), pass.c_str());
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
            delay(500);
            Serial.print(".");
        }
        wifiConnected = (WiFi.status() == WL_CONNECTED);
        if (wifiConnected) {
            Serial.println("\nConnected successfully!");
            Serial.println("IP Address: " + WiFi.localIP().toString());
            consecutiveFailures = 0;
            
            if (fbProjectId.length() == 0 || fbApiKey.length() == 0) {
                Serial.println("Updating config file with Firebase credentials...");
                saveConfig(ssid, pass, firebaseProjectId, firebaseApiKey, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME);
            }
            return;
        }
        Serial.printf("\nConnection failed (Attempt %d/%d)\n", attempt, MAX_CONNECTION_ATTEMPTS);
        delay(2000);
    }
    
    Serial.println("All connection attempts failed");
    consecutiveFailures++;
}

void checkWiFi() {
    unsigned long currentTime = millis();
    if (currentTime - lastWiFiCheck < WIFI_CHECK_INTERVAL) return;
    lastWiFiCheck = currentTime;
    
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiConnected) {
            Serial.printf("WiFi connection lost - Status: %d\n", WiFi.status());
            wifiConnected = false;
        }
        if (currentTime - lastReconnectAttempt > RECONNECT_INTERVAL) {
            Serial.println("Attempting reconnection...");
            lastReconnectAttempt = currentTime;
            WiFi.reconnect();
        }
    } else if (!wifiConnected) {
        Serial.println("WiFi reconnected successfully");
        wifiConnected = true;
    }
}

// ===== Firestore Integration ===== //
void sendDataToFirestore(uint16_t moisture, const String& pumpStatus) {
    if (!wifiConnected || firebaseProjectId.length() == 0 || firebaseApiKey.length() == 0) {
        Serial.println("Cannot send data: WiFi disconnected or Firestore not configured.");
        return;
    }

    WiFiClientSecure client;
    HTTPClient https;
    
    client.setInsecure();
    
    String currentFirestoreEndpoint = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + "/databases/(default)/documents/";
    
    String readingId = String(millis());
    String subcollectionPath = "plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/data";
    String url = currentFirestoreEndpoint + subcollectionPath + "?documentId=" + readingId + "&key=" + firebaseApiKey;
    
    if (https.begin(client, url)) {
        https.addHeader("Content-Type", "application/json");
        
        JsonDocument doc;
        doc["fields"]["moisture"]["integerValue"] = String(moisture);
        doc["fields"]["pumpStatus"]["stringValue"] = pumpStatus;
        doc["fields"]["deviceId"]["stringValue"] = "ESP8266_001";
        doc["fields"]["readingId"]["stringValue"] = readingId;
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        int httpResponseCode = https.POST(jsonString);
        
        if (httpResponseCode == 200 || httpResponseCode == 201) {
            updateMainDeviceStatus(moisture, pumpStatus, currentFirestoreEndpoint);
        } else {
            Serial.printf("HTTP Response: %d\n", httpResponseCode);
            if (httpResponseCode > 0) {
                String response = https.getString();
                Serial.println("Response: " + response);
            }
        }
        
        https.end();
    }
}

void updateMainDeviceStatus(uint16_t moisture, const String& pumpStatus, const String& endpoint) {
    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    String userDocPath = "plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2";
    String statusUrl = endpoint + userDocPath + "?key=" + firebaseApiKey;
    
    if (https.begin(client, statusUrl)) {
        https.addHeader("Content-Type", "application/json");
        
        JsonDocument statusDoc;
        statusDoc["fields"]["currentMoisture"]["integerValue"] = String(moisture);
        statusDoc["fields"]["currentPumpStatus"]["stringValue"] = pumpStatus;
        statusDoc["fields"]["deviceId"]["stringValue"] = "ESP8266_001";
        statusDoc["fields"]["deviceName"]["stringValue"] = "Plant Irrigation System";
        statusDoc["fields"]["totalReadings"]["integerValue"] = String(millis() / 10000);
        
        String statusJsonString;
        serializeJson(statusDoc, statusJsonString);
        
        https.PATCH(statusJsonString);
        
        https.end();
    }
}

void checkForConfigUpdates() {
    if (!wifiConnected || firebaseProjectId.length() == 0 || firebaseApiKey.length() == 0) {
        return;
    }

    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    String configPath = "plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/config/DeviceConfig";
    String configUrl = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + "/databases/(default)/documents/" + configPath + "?key=" + firebaseApiKey;
    
    if (https.begin(client, configUrl)) {
        int httpResponseCode = https.GET();
        
        if (httpResponseCode == 200) {
            String response = https.getString();
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error && doc.containsKey("fields")) {
                uint16_t newDryThresh = DRY_THRESHOLD;
                uint16_t newWetThresh = WET_THRESHOLD;
                unsigned long newPumpTime = PUMP_RUN_TIME;
                unsigned long newDataInterval = DATA_SEND_INTERVAL;
                bool deviceEnabled = true;
                bool autoMode = true;
                bool configChanged = false;
                
                if (doc["fields"]["dryThreshold"]["integerValue"]) {
                    newDryThresh = doc["fields"]["dryThreshold"]["integerValue"].as<int>();
                    if (newDryThresh != DRY_THRESHOLD) configChanged = true;
                }
                
                if (doc["fields"]["wetThreshold"]["integerValue"]) {
                    newWetThresh = doc["fields"]["wetThreshold"]["integerValue"].as<int>();
                    if (newWetThresh != WET_THRESHOLD) configChanged = true;
                }
                
                if (doc["fields"]["pumpRunTime"]["integerValue"]) {
                    newPumpTime = doc["fields"]["pumpRunTime"]["integerValue"].as<long>();
                    if (newPumpTime != PUMP_RUN_TIME) configChanged = true;
                }
                
                if (doc["fields"]["dataSendInterval"]["integerValue"]) {
                    newDataInterval = doc["fields"]["dataSendInterval"]["integerValue"].as<long>();
                    if (newDataInterval != DATA_SEND_INTERVAL) configChanged = true;
                }
                
                if (doc["fields"]["deviceEnabled"]["booleanValue"].is<bool>()) {
                    deviceEnabled = doc["fields"]["deviceEnabled"]["booleanValue"].as<bool>();
                }
                
                if (doc["fields"]["autoMode"]["booleanValue"].is<bool>()) {
                    autoMode = doc["fields"]["autoMode"]["booleanValue"].as<bool>();
                }
                
                if (configChanged) {
                    updateDeviceConfig(newDryThresh, newWetThresh, newPumpTime, newDataInterval, deviceEnabled, autoMode);
                }
            }
        }
        
        https.end();
    }
}

void updateDeviceConfig(uint16_t dryThresh, uint16_t wetThresh, unsigned long pumpTime, unsigned long dataInterval, bool deviceEnabled, bool autoMode) {
    DRY_THRESHOLD = dryThresh;
    WET_THRESHOLD = wetThresh;
    PUMP_RUN_TIME = pumpTime;

    String ssid, pass, fbProjectId, fbApiKey;
    uint16_t oldDry, oldWet;
    unsigned long oldPump;
    
    if (loadConfig(ssid, pass, fbProjectId, fbApiKey, oldDry, oldWet, oldPump)) {
        saveConfig(ssid, pass, fbProjectId, fbApiKey, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME);
    }
}

// ===== Web Server Functions ===== //
void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/status", HTTP_GET, handleGetStatus);
    server.on("/setThreshold", HTTP_POST, handleSetThreshold);
    server.on("/setPumpTime", HTTP_POST, handleSetPumpTime);
    server.on("/testFirestore", HTTP_GET, handleTestFirestore);
    server.on("/resetConfig", HTTP_GET, handleResetConfig); 
    
    server.onNotFound([]() {
        if (server.method() == HTTP_OPTIONS) {
            server.sendHeader("Access-Control-Allow-Origin", "*");
            server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
            server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
            server.send(200);
        } else {
            server.send(404, "text/plain", "Not Found");
        }
    });
    
    server.begin();
    Serial.println("Web server started on port 80");
}

void handleRoot() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String html = "<!DOCTYPE html><html><head><title>Plant Watering System</title></head><body>";
    html += "<h1>Smart Irrigation System v2.0</h1>";
    html += "<h2>Current Status</h2>";
    html += "<p>Moisture Level: " + String(analogRead(SENSOR_PIN)) + "</p>";
    html += "<p>Data Logging: ";
    html += (firebaseProjectId.length() > 0 ? "Firestore Enabled" : "Disabled");
    html += "</p>";
    html += "<p>Data Path: plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/data</p>";
    html += "<p>Config Path: plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/config/DeviceConfig</p>";
    html += "<p>Device ID: ESP8266_001</p>";
    html += "<h3>Actions</h3>";
    html += "<p><a href='/testFirestore'>Test Firestore</a></p>";
    html += "<p><a href='/resetConfig' onclick='return confirm(\"Are you sure? This will restart the device.\")'>Reset Configuration</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleGetStatus() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    JsonDocument doc;
    doc["moisture"] = analogRead(SENSOR_PIN);
    doc["dryThreshold"] = DRY_THRESHOLD;
    doc["wetThreshold"] = WET_THRESHOLD;
    doc["pumpRunTime"] = PUMP_RUN_TIME;
    
    String pumpStateStr;
    switch (currentState) {
        case MONITORING: pumpStateStr = "MONITORING"; break;
        case PUMP_RUNNING: pumpStateStr = "PUMP_RUNNING"; break;
        case PUMP_WAITING: pumpStateStr = "PUMP_WAITING"; break;
        case MANUAL_PUMPING: pumpStateStr = "MANUAL_PUMPING"; break; // NEW
    }
    doc["pumpState"] = pumpStateStr;

    doc["wifiConnected"] = wifiConnected;
    doc["dataLoggingEnabled"] = (firebaseProjectId.length() > 0 && firebaseProjectId != "your-project-id");
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSetThreshold() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (server.hasArg("dryThreshold")) DRY_THRESHOLD = server.arg("dryThreshold").toInt();
    if (server.hasArg("wetThreshold")) WET_THRESHOLD = server.arg("wetThreshold").toInt();
    
    String ssid, pass, fbProjectId, fbApiKey;
    uint16_t dry, wet; unsigned long pumpT;
    if (loadConfig(ssid, pass, fbProjectId, fbApiKey, dry, wet, pumpT)) {
        saveConfig(ssid, pass, fbProjectId, fbApiKey, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME);
    }
    server.send(200, "application/json", "{\"status\":\"success\"}");
}

void handleSetPumpTime() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (server.hasArg("pumpRunTime")) PUMP_RUN_TIME = server.arg("pumpRunTime").toInt();

    String ssid, pass, fbProjectId, fbApiKey;
    uint16_t dry, wet; unsigned long pumpT;
    if (loadConfig(ssid, pass, fbProjectId, fbApiKey, dry, wet, pumpT)) {
        saveConfig(ssid, pass, fbProjectId, fbApiKey, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME);
    }
    server.send(200, "application/json", "{\"status\":\"success\"}");
}

void handleTestFirestore() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    Serial.println("Manual test of Firestore connection requested via web");
    sendDataToFirestore(999, "TEST");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Test sent to Firestore\"}");
}

void handleResetConfig() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    Serial.println("Configuration reset requested via web");
    
    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
    }
    
    WiFi.disconnect(true);
    
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration reset. Device will restart.\"}");
    
    delay(1000);
    ESP.restart();
}

// ===== Core Logic ===== //
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    pinMode(PUMP_CTRL_PIN, OUTPUT);
    digitalWrite(PUMP_CTRL_PIN, LOW);
    
    // NEW: Initialize button and LED pins
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    Serial.println("\nSmart Irrigation System v2.0 - Firestore Edition (Offline-First Mode)\n");

    // Mount FS (already attempted in functions, but ensure thresholds potentially loaded early)
    String ssid, pass, fbPid, fbKey; uint16_t dry, wet; unsigned long pumpTime;
    if (loadConfig(ssid, pass, fbPid, fbKey, dry, wet, pumpTime)) {
        if (fbPid.length()) firebaseProjectId = fbPid;
        if (fbKey.length()) firebaseApiKey = fbKey;
        DRY_THRESHOLD = dry; WET_THRESHOLD = wet; PUMP_RUN_TIME = pumpTime;
        if (ssid.length()) {
            Serial.println("Initiating non-blocking WiFi begin...");
            WiFi.begin(ssid.c_str(), pass.c_str());
        }
    } else {
        Serial.println("No saved config yet; device will run offline until configured.");
    }
    // Web server will be started upon successful WiFi connection in handleWiFi()
}

// NEW: Function to handle LED status indicators
void updateLedStatus() {
    unsigned long currentTime = millis();
    
    // Solid ON: System is running and monitoring
    if (currentState == MONITORING || currentState == PUMP_WAITING) {
        digitalWrite(LED_PIN, HIGH);
    } 
    // Fast Blink: Pump is running (either auto or manual)
    else if (currentState == PUMP_RUNNING || currentState == MANUAL_PUMPING) {
        if (currentTime - lastBlinkTime >= 150) { // Blink every 150ms
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
            lastBlinkTime = currentTime;
        }
    }
    // Slow Blink: WiFi is disconnected
    if (!wifiConnected) {
        if (currentTime - lastBlinkTime >= 1000) { // Blink every 1 second
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
            lastBlinkTime = currentTime;
        }
    }
}

// NEW: Offline-first refactored loop
void loop() {
    unsigned long currentTime = millis();

    // Core always-on logic
    runWateringLogic();
    updateLedStatus();
    handleWiFi();

    // Network dependent tasks
    if (wifiConnected) {
        server.handleClient();

        if (currentTime - lastConfigCheck >= CONFIG_CHECK_INTERVAL) {
            checkForConfigUpdates();
            lastConfigCheck = currentTime;
        }

        if (currentTime - lastDataSend >= DATA_SEND_INTERVAL) {
            uint16_t moisture = analogRead(SENSOR_PIN);
            String pumpStatusStr;
            switch (currentState) {
                case MONITORING: pumpStatusStr = "MONITORING"; break;
                case PUMP_RUNNING: pumpStatusStr = "PUMP_RUNNING"; break;
                case PUMP_WAITING: pumpStatusStr = "PUMP_WAITING"; break;
                case MANUAL_PUMPING: pumpStatusStr = "MANUAL_PUMPING"; break;
            }
            sendDataToFirestore(moisture, pumpStatusStr);
            lastDataSend = currentTime;
        }
    }
    delay(10);
}

// NEW: Extracted watering logic function
void runWateringLogic() {
    unsigned long currentTime = millis();

    // Debounced button handling
    if (currentTime - lastButtonCheck > DEBOUNCE_DELAY) {
        bool pressed = (digitalRead(BUTTON_PIN) == LOW);
        if (pressed && !buttonPressed) {
            buttonPressed = true;
            if (currentState == MONITORING || currentState == PUMP_WAITING) {
                currentState = MANUAL_PUMPING;
                pumpStartTime = currentTime;
                digitalWrite(PUMP_CTRL_PIN, HIGH);
                Serial.println("PUMP: ON (manual override)");
            }
        } else if (!pressed) {
            buttonPressed = false;
        }
        lastButtonCheck = currentTime;
    }

    // Periodic status display
    if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
        uint16_t moisture = analogRead(SENSOR_PIN);
        const char* stateStr = "UNKNOWN";
        switch (currentState) {
            case MONITORING: stateStr = "MONITORING"; break;
            case PUMP_RUNNING: stateStr = "PUMP_RUNNING"; break;
            case PUMP_WAITING: stateStr = "PUMP_WAITING"; break;
            case MANUAL_PUMPING: stateStr = "MANUAL_PUMPING"; break;
        }
        Serial.printf("Moisture: %d | State: %s | WiFi: %s\n", moisture, stateStr, (wifiConnected ? "OK" : "DISCONNECTED"));
        lastDisplayTime = currentTime;
    }

    // Pump state machine
    switch (currentState) {
        case MONITORING:
            if (analogRead(SENSOR_PIN) >= DRY_THRESHOLD) {
                digitalWrite(PUMP_CTRL_PIN, HIGH);
                currentState = PUMP_RUNNING;
                pumpStartTime = currentTime;
                Serial.println("PUMP: ON (dry soil detected)");
            }
            break;
        case PUMP_RUNNING:
        case MANUAL_PUMPING:
            if (currentTime - pumpStartTime >= PUMP_RUN_TIME) {
                digitalWrite(PUMP_CTRL_PIN, LOW);
                currentState = PUMP_WAITING;
                lastPumpActionTime = currentTime;
                Serial.println("PUMP: OFF (cycle completed)");
            }
            break;
        case PUMP_WAITING:
            if (currentTime - lastPumpActionTime >= PUMP_WAIT_TIME) {
                currentState = MONITORING;
                Serial.println("STATE: Resuming monitoring.");
            }
            break;
    }
}

// NEW: Non-blocking WiFi management
void handleWiFi() {
    unsigned long currentTime = millis();

    // Already connected
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            Serial.println("WiFi connected");
            Serial.println("IP: " + WiFi.localIP().toString());
            // Start server when first connected
            setupWebServer();
        }
        return;
    }

    // Lost connection
    if (wifiConnected && WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        Serial.println("WiFi lost - transitioning to offline mode");
    }

    // Periodic reconnect attempts
    if (currentTime - lastReconnectAttempt >= RECONNECT_INTERVAL) {
        lastReconnectAttempt = currentTime;
        if (WiFi.SSID().length()) {
            Serial.println("Attempting WiFi reconnect...");
            WiFi.reconnect();
        } else {
            Serial.println("No stored SSID; start config portal if user action required.");
        }
    }
}