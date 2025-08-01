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
// MODIFIED: Direct Firestore REST API integration for ESP8266
// You can hardcode these here, but the code will prioritize loading them from LittleFS.
String firebaseProjectId = "plantirrigation-7645a"; // Your Firebase project ID
String firebaseApiKey = "AIzaSyAu1hcRbSuOvLNfAeyfIkQ2yuYbzp2OLEY"; // Your Web API key from Firebase project settings

// Firestore REST API endpoint
String firestoreEndpoint = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + "/databases/(default)/documents/";

// ===== Data Logging Configuration ===== //
unsigned long lastDataSend = 0;
unsigned long lastConfigCheck = 0;
// MODIFIED: Increased send interval to 10 seconds for Firebase (can be adjusted)
const unsigned long DATA_SEND_INTERVAL = 10000; // Send data every 10 seconds
const unsigned long CONFIG_CHECK_INTERVAL = 30000; // Check for config updates every 30 seconds

// ===== Web Server ===== //
ESP8266WebServer server(80);

// WiFiManager instance
WiFiManager wm;

// WiFi state tracking (unchanged)
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

// Function declarations
void startConfigurationPortal();
void connectWiFi();
void checkWiFi();
bool checkInternet();
// MODIFIED: Replaced Google Sheets function with Firestore function
void sendDataToFirestore(uint16_t moisture, const String& pumpStatus);
void updateMainDeviceStatus(uint16_t moisture, const String& pumpStatus, const String& endpoint);
void checkForConfigUpdates();
void updateDeviceConfig(uint16_t dryThresh, uint16_t wetThresh, unsigned long pumpTime, unsigned long dataInterval, bool deviceEnabled, bool autoMode);
void setupWebServer();
void handleRoot();
void handleGetStatus();
void handleSetThreshold();
void handleSetPumpTime();
// MODIFIED: Replaced Google Sheets test with Firestore test
void handleTestFirestore();
void handleResetConfig();

// ===== Irrigation System Constants ===== // (unchanged)
constexpr uint8_t PUMP_CTRL_PIN = D1;
constexpr uint8_t SENSOR_PIN = A0;
uint16_t DRY_THRESHOLD = 520;
uint16_t WET_THRESHOLD = 420;

// Timing variables (unchanged)
unsigned long lastDisplayTime = 0;
unsigned long lastPumpActionTime = 0;
const unsigned long DISPLAY_INTERVAL = 3000;
unsigned long PUMP_RUN_TIME = 2000;
const unsigned long PUMP_WAIT_TIME = 60000;

// Pump state variables (unchanged)
enum PumpState { MONITORING, PUMP_RUNNING, PUMP_WAITING };
PumpState currentState = MONITORING;
unsigned long pumpStartTime = 0;

// ===== File System Functions ===== //
// MODIFIED: saveConfig now saves Firestore credentials (simplified)
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
    doc["firebaseProjectId"] = fbProjectId; // MODIFIED
    doc["firebaseApiKey"] = fbApiKey; // MODIFIED
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

// MODIFIED: loadConfig now loads Firestore credentials (simplified)
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
    fbProjectId = doc["firebaseProjectId"].as<String>(); // MODIFIED
    fbApiKey = doc["firebaseApiKey"].as<String>(); // MODIFIED
    dryThresh = doc["dryThreshold"] | 520;
    wetThresh = doc["wetThreshold"] | 420;
    pumpTime = doc["pumpRunTime"] | 2000;

    // Debug output
    Serial.println("=== Config file loaded ===");
    Serial.println("SSID: " + ssid);
    Serial.println("Firebase Project ID: " + fbProjectId); // MODIFIED
    Serial.println("Firebase API Key: " + fbApiKey); // DEBUG
    Serial.println("Dry Threshold: " + String(dryThresh));
    Serial.println("==========================");

    configFile.close();
    return true;
}


// ===== WiFi Functions ===== //

void setupWiFi() {
    // This function is mostly unchanged
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(180); // Set a 3-minute timeout for the portal
}

void startConfigurationPortal() {
    Serial.println("Starting configuration portal...");
    
    WiFi.disconnect(true);
    delay(1000);
    
    // You could add custom parameters here to ask for Firestore credentials in the portal
    // For simplicity, this example relies on them being in the code or a pre-flashed config.
    
    if (!wm.startConfigPortal("IrrigationAP", "plant123456")) {
        Serial.println("Failed to connect and hit timeout");
        ESP.restart(); // Restart if portal times out
        return;
    }
    
    String ssid = WiFi.SSID();
    String pass = WiFi.psk();
    
    // Save credentials (including the hardcoded Firestore details for the first time)
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
    
    // FORCE RESET: Delete old config file and recreate with hardcoded Firebase credentials
    if (LittleFS.exists(CONFIG_FILE)) {
        Serial.println("Deleting old config file to force reset...");
        LittleFS.remove(CONFIG_FILE);
    }
    
    String ssid, pass, fbProjectId, fbApiKey; // MODIFIED - removed email/password
    uint16_t dryThresh, wetThresh;
    unsigned long pumpTime;
    
    // Check if config file exists and load it
    bool configLoaded = loadConfig(ssid, pass, fbProjectId, fbApiKey, dryThresh, wetThresh, pumpTime);
    
    // If config doesn't exist or Firebase credentials are missing, recreate it
    if (!configLoaded || fbProjectId.length() == 0 || fbApiKey.length() == 0) {
        Serial.println("Config file missing or incomplete - starting configuration portal");
        startConfigurationPortal();
        return;
    }
    
    // Load saved parameters
    if (fbProjectId.length() > 0) {
        firebaseProjectId = fbProjectId;
        Serial.println("Loaded Firebase Project ID from config: " + fbProjectId);
    } else {
        // Use hardcoded values if config file doesn't have them
        Serial.println("Using hardcoded Firebase credentials - updating config file");
        // Force save the hardcoded credentials to config file
        saveConfig(ssid, pass, firebaseProjectId, firebaseApiKey, dryThresh, wetThresh, pumpTime);
    }
    if (fbApiKey.length() > 0) {
        firebaseApiKey = fbApiKey;
        Serial.println("Loaded Firebase API Key from config: " + fbApiKey.substring(0, 10) + "...");
    }
    DRY_THRESHOLD = dryThresh;
    WET_THRESHOLD = wetThresh;
    PUMP_RUN_TIME = pumpTime;
    
    Serial.println("=== Final loaded values ===");
    Serial.println("Firebase Project ID: " + firebaseProjectId);
    Serial.println("Firebase API Key: " + firebaseApiKey.substring(0, 10) + "...");
    Serial.println("===============================");
    
    // Debug output after loading
    Serial.println("=== Final loaded values ===");
    Serial.println("Firebase Project ID: " + firebaseProjectId);
    Serial.println("Firebase API Key: " + firebaseApiKey);
    Serial.println("===============================");
    
    // WiFi connection logic (unchanged)
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
            
            // Ensure Firebase credentials are saved to config
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

// checkWiFi() and checkInternet() can remain unchanged

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
// MODIFIED: Send data to specific user's data sub-collection
void sendDataToFirestore(uint16_t moisture, const String& pumpStatus) {
    if (!wifiConnected || firebaseProjectId.length() == 0 || firebaseApiKey.length() == 0) {
        Serial.println("Cannot send data: WiFi disconnected or Firestore not configured.");
        return;
    }

    WiFiClientSecure client;
    HTTPClient https;
    
    client.setInsecure(); // For testing - in production, use proper certificate validation
    
    // Update Firestore endpoint with current project ID
    String currentFirestoreEndpoint = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + "/databases/(default)/documents/";
    
    // MODIFIED: Store individual readings in the subcollection plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/data
    String readingId = String(millis()); // Use timestamp as reading ID
    String subcollectionPath = "plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/data";
    String url = currentFirestoreEndpoint + subcollectionPath + "?documentId=" + readingId + "&key=" + firebaseApiKey;
    
    Serial.printf("Firestore: Sending reading to %s/%s... ", subcollectionPath.c_str(), readingId.c_str());
    
    if (https.begin(client, url)) {
        https.addHeader("Content-Type", "application/json");
        
        // Prepare individual reading document
        JsonDocument doc;
        doc["fields"]["moisture"]["integerValue"] = String(moisture);
        doc["fields"]["pumpStatus"]["stringValue"] = pumpStatus;
        // FIXED: Use proper timestamp format or remove to let Firestore auto-generate
        // doc["fields"]["timestamp"]["timestampValue"] = ""; // Firestore server timestamp
        doc["fields"]["deviceId"]["stringValue"] = "ESP8266_001";
        doc["fields"]["readingId"]["stringValue"] = readingId;
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        int httpResponseCode = https.POST(jsonString);
        
        if (httpResponseCode == 200 || httpResponseCode == 201) {
            Serial.println("✓ OK");
            
            // Also update the main user device status document
            updateMainDeviceStatus(moisture, pumpStatus, currentFirestoreEndpoint);
            
        } else {
            Serial.println("✗ FAILED");
            Serial.printf("HTTP Response: %d\n", httpResponseCode);
            if (httpResponseCode > 0) {
                String response = https.getString();
                Serial.println("Response: " + response);
            }
        }
        
        https.end();
    } else {
        Serial.println("✗ FAILED to connect");
    }
}

// Helper function to update main device status
void updateMainDeviceStatus(uint16_t moisture, const String& pumpStatus, const String& endpoint) {
    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    // Update the main user document (not in subcollection)
    String userDocPath = "plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2";
    String statusUrl = endpoint + userDocPath + "?key=" + firebaseApiKey;
    
    Serial.printf("Firestore: Updating main status... ");
    
    if (https.begin(client, statusUrl)) {
        https.addHeader("Content-Type", "application/json");
        
        JsonDocument statusDoc;
        statusDoc["fields"]["currentMoisture"]["integerValue"] = String(moisture);
        statusDoc["fields"]["currentPumpStatus"]["stringValue"] = pumpStatus;
        // FIXED: Use proper timestamp format or remove to let Firestore auto-generate
        // statusDoc["fields"]["lastUpdated"]["timestampValue"] = "";
        statusDoc["fields"]["deviceId"]["stringValue"] = "ESP8266_001";
        statusDoc["fields"]["deviceName"]["stringValue"] = "Plant Irrigation System";
        statusDoc["fields"]["totalReadings"]["integerValue"] = String(millis() / 10000); // Approximate reading count
        
        String statusJsonString;
        serializeJson(statusDoc, statusJsonString);
        
        int statusResponse = https.PATCH(statusJsonString);
        
        if (statusResponse == 200 || statusResponse == 201) {
            Serial.println("✓ Status OK");
        } else {
            Serial.println("✗ Status FAILED");
        }
        
        https.end();
    }
}

// Function to check for configuration updates from Firestore
void checkForConfigUpdates() {
    if (!wifiConnected || firebaseProjectId.length() == 0 || firebaseApiKey.length() == 0) {
        return;
    }

    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    // Check for config updates in the config subcollection
    String configPath = "plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/config/DeviceConfig";
    String configUrl = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + "/databases/(default)/documents/" + configPath + "?key=" + firebaseApiKey;
    
    Serial.print("Checking for config updates... ");
    
    if (https.begin(client, configUrl)) {
        int httpResponseCode = https.GET();
        
        if (httpResponseCode == 200) {
            String response = https.getString();
            
            // Parse the JSON response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error && doc.containsKey("fields")) {
                // Extract configuration values with defaults
                uint16_t newDryThresh = DRY_THRESHOLD;
                uint16_t newWetThresh = WET_THRESHOLD;
                unsigned long newPumpTime = PUMP_RUN_TIME;
                unsigned long newDataInterval = DATA_SEND_INTERVAL;
                bool deviceEnabled = true;
                bool autoMode = true;
                bool configChanged = false;
                
                // Parse dryThreshold
                if (doc["fields"]["dryThreshold"]["integerValue"]) {
                    newDryThresh = doc["fields"]["dryThreshold"]["integerValue"].as<int>();
                    if (newDryThresh != DRY_THRESHOLD) configChanged = true;
                }
                
                // Parse wetThreshold
                if (doc["fields"]["wetThreshold"]["integerValue"]) {
                    newWetThresh = doc["fields"]["wetThreshold"]["integerValue"].as<int>();
                    if (newWetThresh != WET_THRESHOLD) configChanged = true;
                }
                
                // Parse pumpRunTime
                if (doc["fields"]["pumpRunTime"]["integerValue"]) {
                    newPumpTime = doc["fields"]["pumpRunTime"]["integerValue"].as<long>();
                    if (newPumpTime != PUMP_RUN_TIME) configChanged = true;
                }
                
                // Parse dataSendInterval
                if (doc["fields"]["dataSendInterval"]["integerValue"]) {
                    newDataInterval = doc["fields"]["dataSendInterval"]["integerValue"].as<long>();
                    if (newDataInterval != DATA_SEND_INTERVAL) configChanged = true;
                }
                
                // Parse deviceEnabled
                if (doc["fields"]["deviceEnabled"]["booleanValue"].is<bool>()) {
                    deviceEnabled = doc["fields"]["deviceEnabled"]["booleanValue"].as<bool>();
                }
                
                // Parse autoMode
                if (doc["fields"]["autoMode"]["booleanValue"].is<bool>()) {
                    autoMode = doc["fields"]["autoMode"]["booleanValue"].as<bool>();
                }
                
                if (configChanged) {
                    Serial.println("✓ Config updated from app!");
                    updateDeviceConfig(newDryThresh, newWetThresh, newPumpTime, newDataInterval, deviceEnabled, autoMode);
                } else {
                    Serial.println("✓ No changes");
                }
            } else {
                Serial.println("✓ No config found");
            }
        } else if (httpResponseCode == 404) {
            Serial.println("✓ No config document");
        } else {
            Serial.printf("✗ Failed (%d)\n", httpResponseCode);
        }
        
        https.end();
    }
}

// Function to update device configuration
void updateDeviceConfig(uint16_t dryThresh, uint16_t wetThresh, unsigned long pumpTime, unsigned long dataInterval, bool deviceEnabled, bool autoMode) {
    DRY_THRESHOLD = dryThresh;
    WET_THRESHOLD = wetThresh;
    PUMP_RUN_TIME = pumpTime;
    // Note: DATA_SEND_INTERVAL is const, so we log it but don't change it
    
    Serial.printf("Config updated: Dry=%d, Wet=%d, PumpTime=%lu, DataInterval=%lu, Enabled=%s, Auto=%s\n", 
                  dryThresh, wetThresh, pumpTime, dataInterval, 
                  deviceEnabled ? "true" : "false", autoMode ? "true" : "false");
    
    if (dataInterval != DATA_SEND_INTERVAL) {
        Serial.printf("Note: DataInterval change requires firmware update (current=%lu, requested=%lu)\n", 
                      DATA_SEND_INTERVAL, dataInterval);
    }
    
    // Save to local config file
    String ssid, pass, fbProjectId, fbApiKey;
    uint16_t oldDry, oldWet;
    unsigned long oldPump;
    
    if (loadConfig(ssid, pass, fbProjectId, fbApiKey, oldDry, oldWet, oldPump)) {
        saveConfig(ssid, pass, fbProjectId, fbApiKey, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME);
        Serial.println("Config saved to local storage");
    }
}

// ===== Web Server Functions ===== //
void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/status", HTTP_GET, handleGetStatus);
    server.on("/setThreshold", HTTP_POST, handleSetThreshold);
    server.on("/setPumpTime", HTTP_POST, handleSetPumpTime);
    // MODIFIED: Test endpoint for Firestore
    server.on("/testFirestore", HTTP_GET, handleTestFirestore);
    // NEW: Reset configuration endpoint
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
    // MODIFIED: Show Firestore status with specific user path
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
    
    // MODIFIED: Replaced complex one-liner with a clear switch statement to fix compiler error
    String pumpStateStr;
    switch (currentState) {
        case MONITORING:
            pumpStateStr = "MONITORING";
            break;
        case PUMP_RUNNING:
            pumpStateStr = "PUMP_RUNNING";
            break;
        case PUMP_WAITING:
            pumpStateStr = "PUMP_WAITING";
            break;
    }
    doc["pumpState"] = pumpStateStr;

    doc["wifiConnected"] = wifiConnected;
    doc["dataLoggingEnabled"] = (firebaseProjectId.length() > 0 && firebaseProjectId != "your-project-id");
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// handleSetThreshold() and handleSetPumpTime() can remain unchanged, as they save to the same local config file.
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


// MODIFIED: Test function for Firestore
void handleTestFirestore() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    Serial.println("Manual test of Firestore connection requested via web");
    sendDataToFirestore(999, "TEST");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Test sent to Firestore\"}");
}

// NEW: Reset configuration function
void handleResetConfig() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    Serial.println("Configuration reset requested via web");
    
    // Delete config file
    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
        Serial.println("Configuration file deleted");
    }
    
    // Clear WiFi credentials
    WiFi.disconnect(true);
    
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration reset. Device will restart.\"}");
    
    delay(1000);
    ESP.restart(); // Restart to reload configuration
}

// ===== Core Irrigation Logic ===== //
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    pinMode(PUMP_CTRL_PIN, OUTPUT);
    digitalWrite(PUMP_CTRL_PIN, LOW);
    
    Serial.println("\nSmart Irrigation System v2.0 - Firestore Edition\n");
    
    setupWiFi();
    connectWiFi();
    
    // Update Firestore endpoint with actual project ID
    if (wifiConnected && firebaseProjectId.length() > 0) {
        firestoreEndpoint = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + "/databases/(default)/documents/";
        Serial.println("Firestore endpoint: " + firestoreEndpoint);
    }
    
    setupWebServer();
    
    Serial.println("Final Firebase Project ID: " + firebaseProjectId);
    Serial.println("Data path: plantData/UWQKMJoSqSSNWVjnf1smQQQJSoB2/data");
    Serial.println("Data sent to Firestore every " + String(DATA_SEND_INTERVAL / 1000) + " seconds");
}

void loop() {
    unsigned long currentTime = millis();
    
    server.handleClient();
    checkWiFi();
    
    // Block for checking configuration updates from app every 30 seconds
    if (currentTime - lastConfigCheck >= CONFIG_CHECK_INTERVAL) {
        checkForConfigUpdates();
        lastConfigCheck = currentTime;
    }
    
    // Block for sending data to Firebase every 10 seconds
    if (currentTime - lastDataSend >= DATA_SEND_INTERVAL) {
        uint16_t moisture = analogRead(SENSOR_PIN);

        // Corrected: Uses a switch statement to determine the pump status string
        String pumpStatusStr;
        switch (currentState) {
            case MONITORING:
                pumpStatusStr = "MONITORING";
                break;
            case PUMP_RUNNING:
                pumpStatusStr = "PUMP_RUNNING";
                break;
            case PUMP_WAITING:
                pumpStatusStr = "PUMP_WAITING";
                break;
        }
        
        sendDataToFirestore(moisture, pumpStatusStr); 
        lastDataSend = currentTime;
    }

    // Block for displaying status on the Serial Monitor every 3 seconds
    if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
        uint16_t moisture = analogRead(SENSOR_PIN);

        // Corrected: Uses a switch statement to determine the pump state string
        String pumpStateStr;
        switch (currentState) {
            case MONITORING:
                pumpStateStr = "MONITORING";
                break;
            case PUMP_RUNNING:
                pumpStateStr = "PUMP_RUNNING";
                break;
            case PUMP_WAITING:
                pumpStateStr = "PUMP_WAITING";
                break;
        }

        // Corrected: Uses the new string in Serial.printf
        Serial.printf("Moisture: %d | State: %s | WiFi: %s\n",
            moisture,
            pumpStateStr.c_str(),
            (wifiConnected ? "OK" : "DISCONNECTED")
        );
        lastDisplayTime = currentTime;
    }

    // State machine for pump control (unchanged)
    switch (currentState) {
        case MONITORING: {
            if (analogRead(SENSOR_PIN) >= DRY_THRESHOLD) {
                digitalWrite(PUMP_CTRL_PIN, HIGH);
                currentState = PUMP_RUNNING;
                pumpStartTime = currentTime;
                Serial.println("PUMP: ON (dry soil detected)");
            }
            break;
        }
        case PUMP_RUNNING: {
            if (currentTime - pumpStartTime >= PUMP_RUN_TIME) {
                digitalWrite(PUMP_CTRL_PIN, LOW);
                currentState = PUMP_WAITING;
                lastPumpActionTime = currentTime;
                Serial.println("PUMP: OFF (cycle completed)");
            }
            break;
        }
        case PUMP_WAITING: {
            if (currentTime - lastPumpActionTime >= PUMP_WAIT_TIME) {
                currentState = MONITORING; // Go back to monitoring after wait time
                Serial.println("STATE: Resuming monitoring.");
            }
            break;
        }
    }
    
    delay(10); // Small delay for stability
}
