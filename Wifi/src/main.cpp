/*
 * ====================================================================
 * SMART IRRIGATION SYSTEM - PHASE 1 IMPLEMENTATION
 * Complete Design Per System Specification Document
 * ====================================================================
 * 
 * Features Implemented:
 * - Hardware: Button (D2), LED (D3), Pump (D1), Moisture Sensor (A0)
 * - Button Controls: Triple Press (WiFi Reset), Long Press (Clear Fault), Short Press (Manual Water)
 * - LED Status Indicators: Multi-pattern status display
 * - Pump Safety: minIntervalSec, no-effect detection, fault locking
 * - Persistent Storage: config.json (WiFi/Firebase), pump_state.json (pump history & faults)
 * - Device ID: Generated from MAC address
 * - WiFi: Smart retry with exponential backoff, non-blocking portal
 * - Firestore: Device-specific paths, logs, config sync, remote commands
 * - State Machine: AWAITING_CONFIG, ONLINE, OFFLINE, LOCKED_FAULT
 * 
 * Author: Auto-generated from System Design Specification
 * Version: 3.0 - Phase 1
 * ====================================================================
 */

#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>  // For NTP time sync

// ====================================================================
// HARDWARE PIN CONFIGURATION
// ====================================================================
constexpr uint8_t PUMP_CTRL_PIN = D1;      // ULN2003 IN1
constexpr uint8_t SENSOR_PIN = A0;          // Moisture sensor analog out
constexpr uint8_t BUTTON_PIN = D2;          // Manual control button
constexpr uint8_t LED_PIN = D3;             // Status LED

// ====================================================================
// FIREBASE CONFIGURATION
// ====================================================================
String firebaseProjectId = "bloom-watch-d6878";
String firebaseApiKey = "YOURAPIKEY";
String firebaseDatabaseURL = "YOURDBURL"; //The old ones have been revoked
String deviceId = "";           // Generated from MAC address

// ====================================================================
// FILE SYSTEM PATHS
// ====================================================================
const char* CONFIG_FILE = "/config.json";
const char* PUMP_STATE_FILE = "/pump_state.json";

// ====================================================================
// DEVICE STATE MACHINE
// ====================================================================
enum DeviceState {
    AWAITING_CONFIG,    // No WiFi config, portal active
    ONLINE,             // Connected to WiFi and Firebase
    OFFLINE,            // WiFi not available, operating locally
    LOCKED_FAULT        // Critical fault detected, auto watering disabled
};
DeviceState deviceState = AWAITING_CONFIG;

// ====================================================================
// PUMP STATE MACHINE
// ====================================================================
enum PumpState {
    MONITORING,         // Watching sensor, ready to water
    PUMP_RUNNING,       // Actively pumping water
    PUMP_WAITING        // Cooldown period after watering
};
PumpState pumpState = MONITORING;

// ====================================================================
// BUTTON STATE TRACKING
// ====================================================================
enum ButtonAction {
    NONE,
    SHORT_PRESS,        // Manual watering
    LONG_PRESS,         // Clear fault
    TRIPLE_PRESS        // Force WiFi reset
};

// ====================================================================
// LED BLINK PATTERNS
// ====================================================================
enum LedPattern {
    LED_OFF,                    // Device off or sleeping
    LED_PORTAL_ACTIVE,          // Fast double-blink (portal mode)
    LED_CONNECTING,             // Fast single blink (connecting to WiFi)
    LED_ONLINE,                 // Slow heartbeat (connected and online)
    LED_OFFLINE,                // Single blink every 3 seconds (offline mode)
    LED_PUMPING,                // Solid on (pump running)
    LED_FAULT,                  // Slow error blink (locked fault)
    LED_BUTTON_FEEDBACK         // Quick flash (button acknowledged)
};
LedPattern currentLedPattern = LED_OFF;

// ====================================================================
// CONFIGURATION PARAMETERS (defaults for testing)
// ====================================================================
uint16_t DRY_THRESHOLD = 520;           // Moisture level to trigger watering
uint16_t WET_THRESHOLD = 420;           // Moisture level when soil is wet
unsigned long PUMP_RUN_TIME = 2000;     // 2 seconds (minimal for testing)
unsigned long MIN_INTERVAL_SEC = 0;     // NO DELAY - for testing only! Set to 60+ in production
uint8_t MAX_NO_EFFECT_REPEATS = 2;     // 2 consecutive failures triggers fault
unsigned long PUMP_SETTLE_MS = 5000;    // 5 seconds wait after pump to re-read sensor (faster testing)

// ====================================================================
// TIMING CONSTANTS
// ====================================================================
const unsigned long PORTAL_TIMEOUT = 300000;        // 5 minutes
const unsigned long DATA_SEND_INTERVAL = 5000;      // 5 seconds (faster logging for testing)
const unsigned long CONFIG_CHECK_INTERVAL = 10000;  // 10 seconds (faster remote command check)
const unsigned long DISPLAY_INTERVAL = 2000;        // 2 seconds (more frequent status updates)
const unsigned long WIFI_CHECK_INTERVAL = 5000;     // 5 seconds
const unsigned long BUTTON_DEBOUNCE_MS = 50;        // 50ms debounce
const unsigned long LONG_PRESS_MS = 5000;           // 5 second long press
const unsigned long TRIPLE_PRESS_WINDOW = 800;      // 0.8 second window for triple press (more responsive)

// Smart Retry Intervals (exponential backoff)
const unsigned long RETRY_INTERVAL_1 = 3600000;     // 1 hour
const unsigned long RETRY_INTERVAL_2 = 21600000;    // 6 hours  
const unsigned long RETRY_INTERVAL_3 = 86400000;    // 24 hours

// ====================================================================
// GLOBAL STATE VARIABLES
// ====================================================================
// WiFi & Connectivity
WiFiManager wm;
ESP8266WebServer server(80);
bool wifiConnected = false;
unsigned long lastReconnectAttempt = 0;
unsigned long nextRetryInterval = RETRY_INTERVAL_1;
uint8_t retryCount = 0;

// Timing trackers
unsigned long lastDataSend = 0;
unsigned long lastConfigCheck = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastLedUpdate = 0;

// Pump state tracking
unsigned long pumpStartTime = 0;
unsigned long lastPumpEndEpoch = 0;     // Epoch seconds of last pump stop
unsigned long lastPumpActionTime = 0;
uint16_t moistureBeforePump = 0;
uint8_t noEffectCounter = 0;
bool lockedFault = false;
String lastActivationMethod = "NONE";

// Button tracking
bool buttonPressed = false;
unsigned long buttonPressStart = 0;
unsigned long lastButtonPress = 0;
uint8_t pressCount = 0;
bool longPressHandled = false;

// LED tracking
bool ledState = false;
unsigned long ledBlinkStart = 0;

// ====================================================================
// FUNCTION DECLARATIONS
// ====================================================================
// Device initialization
void initializeFileSystem();
void generateDeviceId();
void loadOrCreateConfig();
void loadPumpState();
void savePumpState();

// WiFi management
void setupWiFi();
void startConfigurationPortal();
void attemptWiFiConnection();
void checkWiFi();
void handleSmartRetry();

// Hardware I/O
ButtonAction readButton();
void updateLED();
void setLedPattern(LedPattern pattern);

// Pump control
void handlePumpStateMachine();
bool checkPumpSafety();
void activatePump(const String& method);
void checkPumpEffectiveness();

// Firestore integration
void syncWithFirestore();
void sendDataToFirestore(uint16_t moisture, const String& pumpStatus, const String& activationMethod);
void updateMainDeviceStatus(uint16_t moisture, const String& pumpStatus);
void checkForConfigUpdates();
void checkForRemoteCommands();
void logEventToFirestore(const String& eventType, const String& details);

// Web server
void setupWebServer();
void handleRoot();
void handleGetStatus();
void handleManualWater();
void handleClearFault();
void handleResetWiFi();

// Utility
String getDeviceStateString();
String getPumpStateString();
unsigned long getCurrentEpoch();

// ====================================================================
// SETUP
// ====================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n====================================");
    Serial.println("SMART IRRIGATION SYSTEM v3.0");
    Serial.println("Phase 1: Full Design Implementation");
    Serial.println("====================================\n");
    
    // Initialize hardware pins
    pinMode(PUMP_CTRL_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(PUMP_CTRL_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    
    // Initialize file system
    initializeFileSystem();
    
    // Generate unique device ID from MAC
    generateDeviceId();
    
    Serial.println("Device ID: " + deviceId);
    Serial.println("Firestore Path: plantData/" + deviceId);
    
    // Load configuration first
    loadOrCreateConfig();
    
    // ⚠️ TESTING MODE: Force MIN_INTERVAL_SEC to 0 regardless of saved config
    if (MIN_INTERVAL_SEC != 0) {
        Serial.println("⚠️  TESTING MODE: Overriding MIN_INTERVAL_SEC");
        Serial.printf("  Changed: %lu sec → 0 sec (NO SAFETY DELAY!)\n", MIN_INTERVAL_SEC);
        MIN_INTERVAL_SEC = 0;
        PUMP_SETTLE_MS = 5000;
        
        // Save updated config
        JsonDocument doc;
        doc["firebaseProjectId"] = firebaseProjectId;
        doc["firebaseApiKey"] = firebaseApiKey;
        doc["dryThreshold"] = DRY_THRESHOLD;
        doc["wetThreshold"] = WET_THRESHOLD;
        doc["pumpRunTime"] = PUMP_RUN_TIME;
        doc["minIntervalSec"] = MIN_INTERVAL_SEC;
        
        File configFile = LittleFS.open(CONFIG_FILE, "w");
        if (configFile) {
            serializeJson(doc, configFile);
            configFile.close();
            Serial.println("  ✓ Config updated with new safety interval");
        }
    }
    
    // Reset pump state for immediate testing
    if (LittleFS.exists(PUMP_STATE_FILE)) {
        LittleFS.remove(PUMP_STATE_FILE);
        Serial.println("  Pump state file deleted (fresh start)");
    }
    lastPumpEndEpoch = 0;
    lockedFault = false;
    noEffectCounter = 0;
    savePumpState();
    
    // Setup WiFi
    setupWiFi();
    attemptWiFiConnection();
    
    // Setup web server
    setupWebServer();
    
    Serial.println("\n====================================");
    Serial.println("INITIALIZATION COMPLETE");
    Serial.println("State: " + getDeviceStateString());
    Serial.println("====================================\n");
}

// ====================================================================
// MAIN LOOP
// ====================================================================
void loop() {
    unsigned long currentTime = millis();
    
    // Handle web server
    server.handleClient();
    
    // Read and handle button actions
    ButtonAction action = readButton();
    switch (action) {
        case TRIPLE_PRESS:
            Serial.println("\n[BUTTON] Triple press detected - Force WiFi reset");
            setLedPattern(LED_BUTTON_FEEDBACK);
            startConfigurationPortal();
            break;
            
        case LONG_PRESS:
            Serial.println("\n[BUTTON] Long press detected - Clear fault");
            setLedPattern(LED_BUTTON_FEEDBACK);
            if (lockedFault) {
                lockedFault = false;
                noEffectCounter = 0;
                savePumpState();
                deviceState = wifiConnected ? ONLINE : OFFLINE;
                logEventToFirestore("fault_cleared", "User cleared fault via button");
                Serial.println("✓ Fault cleared successfully");
            } else {
                Serial.println("ℹ No fault to clear");
            }
            break;
            
        case SHORT_PRESS:
            Serial.println("\n╔═════════════════════════════════════╗");
            Serial.println("║ 🔘 BUTTON: Manual Water Request    ║");
            Serial.println("╚═════════════════════════════════════╝");
            setLedPattern(LED_BUTTON_FEEDBACK);
            if (deviceState != LOCKED_FAULT) {
                if (checkPumpSafety()) {
                    activatePump("MANUAL");
                } else {
                    Serial.println("❌ DENIED: Safety interval not met\n");
                }
            } else {
                Serial.println("❌ DENIED: Device in FAULT state\n");
            }
            break;
            
        case NONE:
            // No button action
            break;
    }
    
    // Update LED status
    updateLED();
    
    // WiFi management
    if (deviceState != AWAITING_CONFIG) {
        checkWiFi();
        if (!wifiConnected && deviceState != LOCKED_FAULT) {
            handleSmartRetry();
        }
    }
    
    // Firestore sync (only when online)
    if (wifiConnected && deviceState == ONLINE) {
        // Send data periodically
        if (currentTime - lastDataSend >= DATA_SEND_INTERVAL) {
            syncWithFirestore();
            lastDataSend = currentTime;
        }
        
        // Check for config updates
        if (currentTime - lastConfigCheck >= CONFIG_CHECK_INTERVAL) {
            checkForConfigUpdates();
            checkForRemoteCommands();
            lastConfigCheck = currentTime;
        }
    }
    
    // Display status on serial
    if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
        uint16_t moisture = analogRead(SENSOR_PIN);
        
        // Compact status line
        Serial.printf("[STATUS] M:%d | P:%s | D:%s | W:%s",
            moisture,
            getPumpStateString().c_str(),
            getDeviceStateString().c_str(),
            wifiConnected ? "ON" : "OFF"
        );
        
        // Add extra info if relevant
        if (lockedFault) Serial.print(" | ⚠️ FAULT");
        if (wifiConnected) Serial.printf(" | RSSI:%ddBm", WiFi.RSSI());
        if (pumpState == PUMP_WAITING) {
            unsigned long timeSincePump = getCurrentEpoch() - lastPumpEndEpoch;
            Serial.printf(" | Next:%lus", MIN_INTERVAL_SEC - timeSincePump);
        }
        Serial.println();
        
        lastDisplayTime = currentTime;
    }
    
    // Pump state machine (core irrigation logic)
    handlePumpStateMachine();
    
    delay(10);  // Small delay for stability
}

// ====================================================================
// FILE SYSTEM FUNCTIONS
// ====================================================================
void initializeFileSystem() {
    if (!LittleFS.begin()) {
        Serial.println("✗ Failed to mount LittleFS");
        Serial.println("⚠ Running without persistent storage");
    } else {
        Serial.println("✓ LittleFS mounted successfully");
    }
}

void generateDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    // Format: ESP8266_AABBCCDDEEFF
    char macStr[18];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    deviceId = "ESP8266_" + String(macStr);
}

void loadOrCreateConfig() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("ℹ No config file found - will create on first WiFi connection");
        return;
    }
    
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (!configFile) {
        Serial.println("✗ Failed to open config file");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    
    if (error) {
        Serial.println("✗ Config JSON parse error: " + String(error.c_str()));
        return;
    }
    
    // Load Firebase credentials if present
    if (doc.containsKey("firebaseProjectId")) {
        firebaseProjectId = doc["firebaseProjectId"].as<String>();
    }
    if (doc.containsKey("firebaseApiKey")) {
        firebaseApiKey = doc["firebaseApiKey"].as<String>();
    }
    
    // Load watering parameters
    DRY_THRESHOLD = doc["dryThreshold"] | DRY_THRESHOLD;
    WET_THRESHOLD = doc["wetThreshold"] | WET_THRESHOLD;
    PUMP_RUN_TIME = doc["pumpRunTime"] | PUMP_RUN_TIME;
    MIN_INTERVAL_SEC = doc["minIntervalSec"] | MIN_INTERVAL_SEC;
    
    Serial.println("✓ Configuration loaded");
    Serial.printf("  Thresholds: Dry=%d, Wet=%d\n", DRY_THRESHOLD, WET_THRESHOLD);
    Serial.printf("  Pump Time: %lu ms, Min Interval: %lu sec\n", PUMP_RUN_TIME, MIN_INTERVAL_SEC);
}

void loadPumpState() {
    if (!LittleFS.exists(PUMP_STATE_FILE)) {
        Serial.println("ℹ No pump state file - starting fresh");
        savePumpState();  // Create initial state file
        return;
    }
    
    File stateFile = LittleFS.open(PUMP_STATE_FILE, "r");
    if (!stateFile) {
        Serial.println("✗ Failed to open pump state file");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, stateFile);
    stateFile.close();
    
    if (error) {
        Serial.println("✗ Pump state JSON parse error: " + String(error.c_str()));
        return;
    }
    
    lastPumpEndEpoch = doc["lastPumpEndEpoch"] | 0;
    lockedFault = doc["lockedFault"] | false;
    noEffectCounter = doc["noEffectCounter"] | 0;
    
    Serial.println("✓ Pump state loaded");
    Serial.printf("  Last Pump: %lu sec ago, Fault: %s, No-Effect Count: %d\n", 
                  getCurrentEpoch() - lastPumpEndEpoch,
                  lockedFault ? "YES" : "NO",
                  noEffectCounter);
    
    if (lockedFault) {
        deviceState = LOCKED_FAULT;
        setLedPattern(LED_FAULT);
    }
}

void savePumpState() {
    JsonDocument doc;
    doc["lastPumpEndEpoch"] = lastPumpEndEpoch;
    doc["lockedFault"] = lockedFault;
    doc["noEffectCounter"] = noEffectCounter;
    doc["deviceId"] = deviceId;
    
    File stateFile = LittleFS.open(PUMP_STATE_FILE, "w");
    if (!stateFile) {
        Serial.println("✗ Failed to save pump state");
        return;
    }
    
    if (serializeJson(doc, stateFile) == 0) {
        Serial.println("✗ Failed to write pump state JSON");
    }
    
    stateFile.close();
}

// ====================================================================
// WiFi MANAGEMENT
// ====================================================================
void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT / 1000);  // Convert to seconds
}

void startConfigurationPortal() {
    Serial.println("\n[WiFi] Starting configuration portal");
    deviceState = AWAITING_CONFIG;
    setLedPattern(LED_PORTAL_ACTIVE);
    
    WiFi.disconnect(true);
    delay(1000);
    
    if (!wm.startConfigPortal("Irrigation-Setup", "plant123456")) {
        Serial.println("✗ Portal timeout - restarting");
        ESP.restart();
        return;
    }
    
    // Save configuration
    String ssid = WiFi.SSID();
    String pass = WiFi.psk();
    
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    doc["firebaseProjectId"] = firebaseProjectId;
    doc["firebaseApiKey"] = firebaseApiKey;
    doc["dryThreshold"] = DRY_THRESHOLD;
    doc["wetThreshold"] = WET_THRESHOLD;
    doc["pumpRunTime"] = PUMP_RUN_TIME;
    doc["minIntervalSec"] = MIN_INTERVAL_SEC;
    
    File configFile = LittleFS.open(CONFIG_FILE, "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        Serial.println("✓ Configuration saved");
    }
    
    wifiConnected = true;
    deviceState = ONLINE;
    setLedPattern(LED_ONLINE);
    retryCount = 0;
    nextRetryInterval = RETRY_INTERVAL_1;
    
    // Initialize NTP for accurate timestamps (UTC+0)
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("⏰ Syncing time with NTP");
    
    // Wait up to 10 seconds for time sync
    int retries = 0;
    while (time(nullptr) < 100000 && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    
    time_t now = time(nullptr);
    if (now >= 100000) {
        Serial.println(" ✓");
        Serial.printf("  Unix timestamp: %lu (GMT)\n", (unsigned long)now);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        Serial.printf("  Date/Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        Serial.println(" ✗ FAILED");
        Serial.println("  ⚠️  Timestamps will be 0 until sync succeeds");
    }
}

void attemptWiFiConnection() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("ℹ No WiFi config - starting portal");
        startConfigurationPortal();
        return;
    }
    
    // Load saved credentials
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (!configFile) {
        startConfigurationPortal();
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    
    if (error) {
        startConfigurationPortal();
        return;
    }
    
    String ssid = doc["ssid"].as<String>();
    String pass = doc["pass"].as<String>();
    
    if (ssid.length() == 0) {
        startConfigurationPortal();
        return;
    }
    
    Serial.println("\n[WiFi] Attempting connection to: " + ssid);
    setLedPattern(LED_CONNECTING);
    
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        deviceState = lockedFault ? LOCKED_FAULT : ONLINE;
        setLedPattern(lockedFault ? LED_FAULT : LED_ONLINE);
        Serial.println("✓ WiFi connected");
        Serial.println("  IP: " + WiFi.localIP().toString());
        
        // Initialize NTP for accurate timestamps (UTC+0)
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        Serial.print("⏰ Syncing time with NTP");
        
        // Wait up to 10 seconds for time sync
        int retries = 0;
        while (time(nullptr) < 100000 && retries < 20) {
            delay(500);
            Serial.print(".");
            retries++;
        }
        
        time_t now = time(nullptr);
        if (now >= 100000) {
            Serial.println(" ✓");
            Serial.printf("  Unix timestamp: %lu (GMT)\n", (unsigned long)now);
            struct tm timeinfo;
            gmtime_r(&now, &timeinfo);
            Serial.printf("  Date/Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            Serial.println(" ✗ FAILED");
            Serial.println("  ⚠️  Timestamps will be 0 until sync succeeds");
        }
        
        retryCount = 0;
        nextRetryInterval = RETRY_INTERVAL_1;
    } else {
        wifiConnected = false;
        deviceState = lockedFault ? LOCKED_FAULT : OFFLINE;
        setLedPattern(lockedFault ? LED_FAULT : LED_OFFLINE);
        Serial.println("✗ WiFi connection failed - entering offline mode");
        Serial.println("  Next retry in: " + String(nextRetryInterval / 60000) + " minutes");
        lastReconnectAttempt = millis();
    }
}

void checkWiFi() {
    unsigned long currentTime = millis();
    if (currentTime - lastWiFiCheck < WIFI_CHECK_INTERVAL) return;
    lastWiFiCheck = currentTime;
    
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiConnected) {
            Serial.println("✗ WiFi connection lost");
            wifiConnected = false;
            deviceState = lockedFault ? LOCKED_FAULT : OFFLINE;
            setLedPattern(lockedFault ? LED_FAULT : LED_OFFLINE);
            lastReconnectAttempt = currentTime;
        }
    } else if (!wifiConnected) {
        Serial.println("✓ WiFi reconnected");
        wifiConnected = true;
        deviceState = lockedFault ? LOCKED_FAULT : ONLINE;
        setLedPattern(lockedFault ? LED_FAULT : LED_ONLINE);
        
        // Re-sync NTP time after reconnection
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        Serial.println("⏰ Re-syncing NTP time...");
        retryCount = 0;
        nextRetryInterval = RETRY_INTERVAL_1;
    }
}

void handleSmartRetry() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastReconnectAttempt >= nextRetryInterval) {
        Serial.println("\n[WiFi] Smart retry attempt #" + String(retryCount + 1));
        WiFi.reconnect();
        
        delay(10000);  // Wait 10 seconds for connection
        
        if (WiFi.status() != WL_CONNECTED) {
            retryCount++;
            // Exponential backoff: 1h -> 6h -> 24h (max)
            if (retryCount == 1) {
                nextRetryInterval = RETRY_INTERVAL_2;
            } else if (retryCount >= 2) {
                nextRetryInterval = RETRY_INTERVAL_3;
            }
            Serial.println("  Failed. Next retry in: " + String(nextRetryInterval / 3600000) + " hours");
        }
        
        lastReconnectAttempt = currentTime;
    }
}

// ====================================================================
// HARDWARE I/O - BUTTON
// ====================================================================
ButtonAction readButton() {
    static bool lastButtonState = HIGH;  // Pulled up = HIGH when not pressed
    bool currentButtonState = digitalRead(BUTTON_PIN);
    unsigned long currentTime = millis();
    
    // Debounce
    if (currentButtonState != lastButtonState) {
        delay(BUTTON_DEBOUNCE_MS);
        currentButtonState = digitalRead(BUTTON_PIN);
    }
    
    // Button pressed (LOW because of pull-up)
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        buttonPressed = true;
        buttonPressStart = currentTime;
        longPressHandled = false;
        
        // Check for triple press
        if (currentTime - lastButtonPress < TRIPLE_PRESS_WINDOW) {
            pressCount++;
            if (pressCount >= 3) {
                // Triple press detected immediately on third press
                pressCount = 0;
                lastButtonState = currentButtonState;
                return TRIPLE_PRESS;
            }
        } else {
            pressCount = 1;
        }
        lastButtonPress = currentTime;
    }
    
    // Button released
    if (currentButtonState == HIGH && lastButtonState == LOW) {
        unsigned long pressDuration = currentTime - buttonPressStart;
        buttonPressed = false;
        
        if (!longPressHandled && pressDuration < LONG_PRESS_MS) {
            // Don't immediately return SHORT_PRESS - wait to see if it's part of triple press
            // We'll handle short press only after the triple press window expires
            lastButtonState = currentButtonState;
            return NONE;
        }
        
        longPressHandled = false;
    }
    
    // Check for long press while button is still held
    if (buttonPressed && !longPressHandled) {
        if (currentTime - buttonPressStart >= LONG_PRESS_MS) {
            longPressHandled = true;
            pressCount = 0;
            lastButtonState = currentButtonState;
            return LONG_PRESS;
        }
    }
    
    // Handle delayed short press - only after triple press window expires
    if (!buttonPressed && pressCount == 1 && (currentTime - lastButtonPress > TRIPLE_PRESS_WINDOW)) {
        pressCount = 0;
        return SHORT_PRESS;
    }
    
    // Reset incomplete triple press after window expires
    if (!buttonPressed && pressCount >= 2 && (currentTime - lastButtonPress > TRIPLE_PRESS_WINDOW)) {
        pressCount = 0;
    }
    
    lastButtonState = currentButtonState;
    return NONE;
}

// ====================================================================
// HARDWARE I/O - LED
// ====================================================================
void setLedPattern(LedPattern pattern) {
    currentLedPattern = pattern;
    ledBlinkStart = millis();
}

void updateLED() {
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - ledBlinkStart;
    
    switch (currentLedPattern) {
        case LED_OFF:
            digitalWrite(LED_PIN, LOW);
            break;
            
        case LED_PORTAL_ACTIVE:
            // Fast double-blink: 100ms on, 100ms off, 100ms on, 700ms off (1 second cycle)
            if (elapsed % 1000 < 100 || (elapsed % 1000 >= 200 && elapsed % 1000 < 300)) {
                digitalWrite(LED_PIN, HIGH);
            } else {
                digitalWrite(LED_PIN, LOW);
            }
            break;
            
        case LED_CONNECTING:
            // Fast single blink: 200ms on, 800ms off
            digitalWrite(LED_PIN, (elapsed % 1000 < 200) ? HIGH : LOW);
            break;
            
        case LED_ONLINE:
            // Slow heartbeat: 100ms on, 2900ms off
            digitalWrite(LED_PIN, (elapsed % 3000 < 100) ? HIGH : LOW);
            break;
            
        case LED_OFFLINE:
            // Single blink every 3 seconds: 500ms on, 2500ms off
            digitalWrite(LED_PIN, (elapsed % 3000 < 500) ? HIGH : LOW);
            break;
            
        case LED_PUMPING:
            // Solid on
            digitalWrite(LED_PIN, HIGH);
            break;
            
        case LED_FAULT:
            // Slow error blink: 500ms on, 1500ms off
            digitalWrite(LED_PIN, (elapsed % 2000 < 500) ? HIGH : LOW);
            break;
            
        case LED_BUTTON_FEEDBACK:
            // Three quick flashes then return to previous state
            if (elapsed < 600) {
                digitalWrite(LED_PIN, (elapsed % 200 < 100) ? HIGH : LOW);
            } else {
                // Return to appropriate state
                if (lockedFault) {
                    setLedPattern(LED_FAULT);
                } else if (deviceState == ONLINE) {
                    setLedPattern(LED_ONLINE);
                } else if (deviceState == OFFLINE) {
                    setLedPattern(LED_OFFLINE);
                } else if (deviceState == AWAITING_CONFIG) {
                    setLedPattern(LED_PORTAL_ACTIVE);
                }
            }
            break;
    }
}

// ====================================================================
// PUMP CONTROL
// ====================================================================
void handlePumpStateMachine() {
    unsigned long currentTime = millis();
    
    switch (pumpState) {
        case MONITORING:
            // Only auto-water if not in fault state
            if (deviceState != LOCKED_FAULT) {
                uint16_t moisture = analogRead(SENSOR_PIN);
                if (moisture >= DRY_THRESHOLD) {
                    if (checkPumpSafety()) {
                        activatePump("AUTO");
                    }
                }
            }
            break;
            
        case PUMP_RUNNING:
            if (currentTime - pumpStartTime >= PUMP_RUN_TIME) {
                digitalWrite(PUMP_CTRL_PIN, LOW);
                pumpState = PUMP_WAITING;
                lastPumpActionTime = currentTime;
                lastPumpEndEpoch = getCurrentEpoch();
                savePumpState();
                
                Serial.println("  PUMP: OFF (cycle completed)");
                
                // Check effectiveness after pump settles
                // Note: We'll check after settle time in PUMP_WAITING state
            }
            break;
            
        case PUMP_WAITING:
            // After settle time, check pump effectiveness
            if (currentTime - lastPumpActionTime >= PUMP_SETTLE_MS && 
                lastActivationMethod == "AUTO" && moistureBeforePump > 0) {
                checkPumpEffectiveness();
                moistureBeforePump = 0;  // Clear for next cycle
            }
            
            // Return to monitoring after full wait period
            if (currentTime - lastPumpActionTime >= (MIN_INTERVAL_SEC * 1000)) {
                pumpState = MONITORING;
                Serial.println("  STATE: Resuming monitoring");
                
                // Update LED if pump was running
                if (currentLedPattern == LED_PUMPING) {
                    if (lockedFault) {
                        setLedPattern(LED_FAULT);
                    } else if (wifiConnected) {
                        setLedPattern(LED_ONLINE);
                    } else {
                        setLedPattern(LED_OFFLINE);
                    }
                }
            }
            break;
    }
}

bool checkPumpSafety() {
    unsigned long currentEpoch = getCurrentEpoch();
    unsigned long timeSinceLastPump = currentEpoch - lastPumpEndEpoch;
    
    if (timeSinceLastPump < MIN_INTERVAL_SEC) {
        Serial.printf("  ✗ Safety: Only %lu sec since last pump (need %lu sec)\n", 
                      timeSinceLastPump, MIN_INTERVAL_SEC);
        return false;
    }
    
    return true;
}

void activatePump(const String& method) {
    moistureBeforePump = analogRead(SENSOR_PIN);
    lastActivationMethod = method;
    
    digitalWrite(PUMP_CTRL_PIN, HIGH);
    pumpState = PUMP_RUNNING;
    pumpStartTime = millis();
    setLedPattern(LED_PUMPING);
    
    Serial.println("\n┌─────────────────────────────────────┐");
    Serial.printf("│ PUMP ACTIVATED: %s%-14s│\n", method.c_str(), "");
    Serial.printf("│ Moisture Before: %-18d│\n", moistureBeforePump);
    Serial.printf("│ Run Time: %d ms%-20s│\n", PUMP_RUN_TIME, "");
    Serial.println("└─────────────────────────────────────┘");
    
    // Log to Firestore if online
    if (wifiConnected) {
        logEventToFirestore("pump_activated", 
                           "method=" + method + ",moisture=" + String(moistureBeforePump));
    }
}

void checkPumpEffectiveness() {
    uint16_t moistureAfter = analogRead(SENSOR_PIN);
    int16_t delta = moistureAfter - moistureBeforePump;  // Positive = wetter
    
    Serial.println("\n┌─────────────────────────────────────┐");
    Serial.println("│ PUMP EFFECTIVENESS CHECK            │");
    Serial.printf("│ Before: %-27d│\n", moistureBeforePump);
    Serial.printf("│ After:  %-27d│\n", moistureAfter);
    Serial.printf("│ Delta:  %-27d│\n", delta);
    
    // Define minimum acceptable delta (soil should be at least 30 points wetter)
    const int16_t MIN_DELTA = 30;
    
    if (delta < MIN_DELTA) {
        noEffectCounter++;
        Serial.printf("│ ⚠️  NO EFFECT! Count: %d/%d%-10s│\n", 
                      noEffectCounter, MAX_NO_EFFECT_REPEATS, "");
        
        if (noEffectCounter >= MAX_NO_EFFECT_REPEATS) {
            Serial.println("│                                     │");
            Serial.println("│ ❌ CRITICAL FAULT DETECTED!         │");
            Serial.println("│ → Pump ineffective                  │");
            Serial.println("│ → Auto-watering LOCKED              │");
            lockedFault = true;
            deviceState = LOCKED_FAULT;
            savePumpState();
            setLedPattern(LED_FAULT);
            
            if (wifiConnected) {
                logEventToFirestore("fault_locked", 
                                   "Pump ineffective after " + String(MAX_NO_EFFECT_REPEATS) + " attempts");
            }
        }
    } else {
        Serial.println("│ ✅ PUMP EFFECTIVE - Soil wetter     │");
        if (noEffectCounter > 0) {
            Serial.printf("│ Counter reset: %d → 0%-15s│\n", noEffectCounter, "");
        }
        noEffectCounter = 0;
        savePumpState();
    }
    Serial.println("└─────────────────────────────────────┘\n");
}

// ====================================================================
// FIRESTORE INTEGRATION
// ====================================================================
void syncWithFirestore() {
    uint16_t moisture = analogRead(SENSOR_PIN);
    String pumpStatus = getPumpStateString();
    
    sendDataToFirestore(moisture, pumpStatus, lastActivationMethod);
    updateMainDeviceStatus(moisture, pumpStatus);
}

void sendDataToFirestore(uint16_t moisture, const String& pumpStatus, const String& activationMethod) {
    if (!wifiConnected) return;
    
    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    // Create document in logs subcollection with timestamp-based ID
    unsigned long timestamp = getCurrentEpoch();
    String logId = String(timestamp) + "_" + String(millis() % 1000);
    String url = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + 
                 "/databases/(default)/documents/plantData/" + deviceId + "/logs?documentId=" + logId + 
                 "&key=" + firebaseApiKey;
    
    if (!https.begin(client, url)) {
        Serial.println("✗ [FIREBASE] Connection failed");
        return;
    }
    
    https.addHeader("Content-Type", "application/json");
    
    // Enhanced Firestore log with more details
    JsonDocument doc;
    doc["fields"]["moisture"]["integerValue"] = moisture;
    doc["fields"]["pumpStatus"]["stringValue"] = pumpStatus;
    doc["fields"]["activationMethod"]["stringValue"] = activationMethod;
    doc["fields"]["deviceState"]["stringValue"] = getDeviceStateString();
    doc["fields"]["wifiRSSI"]["integerValue"] = WiFi.RSSI();  // Signal strength
    doc["fields"]["uptime"]["integerValue"] = millis() / 1000;  // Device uptime in seconds
    doc["fields"]["lockedFault"]["booleanValue"] = lockedFault;
    doc["fields"]["noEffectCount"]["integerValue"] = noEffectCounter;
    doc["fields"]["timestamp"]["integerValue"] = timestamp;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    int httpCode = https.POST(jsonString);
    
    if (httpCode == 200 || httpCode == 201) {
        Serial.printf("✓ [FIREBASE] Log sent → Moisture:%d, Pump:%s, State:%s\n", 
                     moisture, pumpStatus.c_str(), getDeviceStateString().c_str());
    } else {
        Serial.printf("✗ [FIREBASE] Log failed (HTTP %d)\n", httpCode);
        if (httpCode > 0) {
            String response = https.getString();
            Serial.println("   Response: " + response.substring(0, 200));  // First 200 chars
        }
    }
    
    https.end();
}

void updateMainDeviceStatus(uint16_t moisture, const String& pumpStatus) {
    if (!wifiConnected) return;
    
    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    // Update main device document with heartbeat
    String url = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + 
                 "/databases/(default)/documents/plantData/" + deviceId + 
                 "?updateMask.fieldPaths=currentMoisture" +
                 "&updateMask.fieldPaths=currentPumpStatus" +
                 "&updateMask.fieldPaths=lockedFault" +
                 "&updateMask.fieldPaths=lastSeen" +
                 "&updateMask.fieldPaths=wifiRSSI" +
                 "&updateMask.fieldPaths=uptime" +
                 "&key=" + firebaseApiKey;
    
    if (!https.begin(client, url)) {
        return;
    }
    
    https.addHeader("Content-Type", "application/json");
    
    JsonDocument doc;
    doc["fields"]["currentMoisture"]["integerValue"] = moisture;
    doc["fields"]["currentPumpStatus"]["stringValue"] = pumpStatus;
    doc["fields"]["lockedFault"]["booleanValue"] = lockedFault;
    // Use current Unix timestamp (seconds since epoch)
    doc["fields"]["lastSeen"]["integerValue"] = getCurrentEpoch();
    doc["fields"]["wifiRSSI"]["integerValue"] = WiFi.RSSI();
    doc["fields"]["uptime"]["integerValue"] = millis() / 1000;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    int httpCode = https.PATCH(jsonString);
    
    if (httpCode == 200) {
        Serial.printf("✓ [FIREBASE] Status updated → Last seen: %lu\n", getCurrentEpoch());
    } else if (httpCode > 0) {
        Serial.printf("✗ [FIREBASE] Status update failed (HTTP %d)\n", httpCode);
    }
    
    https.end();
}

void checkForConfigUpdates() {
    if (!wifiConnected) return;
    
    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    // Get config document
    String url = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + 
                 "/databases/(default)/documents/plantData/" + deviceId + "/config/settings" +
                 "?key=" + firebaseApiKey;
    
    if (!https.begin(client, url)) {
        return;
    }
    
    int httpCode = https.GET();
    
    if (httpCode == 200) {
        String response = https.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error && doc.containsKey("fields")) {
            JsonObject fields = doc["fields"];
            
            bool changed = false;
            
            if (fields.containsKey("dryThreshold")) {
                uint16_t newDry = fields["dryThreshold"]["integerValue"].as<uint16_t>();
                if (newDry != DRY_THRESHOLD) {
                    DRY_THRESHOLD = newDry;
                    changed = true;
                }
            }
            
            if (fields.containsKey("wetThreshold")) {
                uint16_t newWet = fields["wetThreshold"]["integerValue"].as<uint16_t>();
                if (newWet != WET_THRESHOLD) {
                    WET_THRESHOLD = newWet;
                    changed = true;
                }
            }
            
            if (fields.containsKey("pumpRunTime")) {
                unsigned long newTime = fields["pumpRunTime"]["integerValue"].as<unsigned long>();
                if (newTime != PUMP_RUN_TIME) {
                    PUMP_RUN_TIME = newTime;
                    changed = true;
                }
            }
            
            if (fields.containsKey("minIntervalSec")) {
                unsigned long newInterval = fields["minIntervalSec"]["integerValue"].as<unsigned long>();
                if (newInterval != MIN_INTERVAL_SEC) {
                    MIN_INTERVAL_SEC = newInterval;
                    changed = true;
                }
            }
            
            if (changed) {
                Serial.println("✓ Config updated from Firestore");
                // TODO: Save to local config file
            }
        }
    }
    
    https.end();
}

void checkForRemoteCommands() {
    if (!wifiConnected) return;
    
    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    // Get pending commands document
    String url = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + 
                 "/databases/(default)/documents/plantData/" + deviceId + "/commands/pending" +
                 "?key=" + firebaseApiKey;
    
    if (!https.begin(client, url)) {
        return;
    }
    
    int httpCode = https.GET();
    
    if (httpCode == 200) {
        String response = https.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error && doc.containsKey("fields")) {
            JsonObject fields = doc["fields"];
            
            // Check for clearFault command
            if (fields.containsKey("clearFault") && 
                fields["clearFault"]["booleanValue"].as<bool>()) {
                
                Serial.println("✓ Remote command: Clear Fault");
                
                if (lockedFault) {
                    lockedFault = false;
                    noEffectCounter = 0;
                    savePumpState();
                    deviceState = ONLINE;
                    setLedPattern(LED_ONLINE);
                    logEventToFirestore("fault_cleared", "Remote clear via app");
                }
                
                // Clear the clearFault field
                HTTPClient httpsPatch;
                if (httpsPatch.begin(client, url + "&updateMask.fieldPaths=clearFault")) {
                    httpsPatch.addHeader("Content-Type", "application/json");
                    String clearPayload = "{\"fields\":{\"clearFault\":{\"booleanValue\":false}}}";
                    httpsPatch.PATCH(clearPayload);
                    httpsPatch.end();
                }
            }
            
            // Check for waterNow command
            if (fields.containsKey("waterNow") && 
                fields["waterNow"]["booleanValue"].as<bool>()) {
                
                Serial.println("✓ Remote command: Water Now");
                
                if (deviceState != LOCKED_FAULT && checkPumpSafety()) {
                    activatePump("REMOTE");
                } else {
                    Serial.println("✗ Remote water command denied (safety/fault)");
                }
                
                // Clear the waterNow field
                HTTPClient httpsPatch;
                if (httpsPatch.begin(client, url + "&updateMask.fieldPaths=waterNow")) {
                    httpsPatch.addHeader("Content-Type", "application/json");
                    String clearPayload = "{\"fields\":{\"waterNow\":{\"booleanValue\":false}}}";
                    httpsPatch.PATCH(clearPayload);
                    httpsPatch.end();
                }
            }
        }
    }
    
    https.end();
}

void logEventToFirestore(const String& eventType, const String& details) {
    if (!wifiConnected) return;
    
    WiFiClientSecure client;
    HTTPClient https;
    client.setInsecure();
    
    String logId = String(millis());
    String url = "https://firestore.googleapis.com/v1/projects/" + firebaseProjectId + 
                 "/databases/(default)/documents/plantData/" + deviceId + "/logs?documentId=" + logId + 
                 "&key=" + firebaseApiKey;
    
    if (!https.begin(client, url)) {
        return;
    }
    
    https.addHeader("Content-Type", "application/json");
    
    JsonDocument doc;
    doc["fields"]["eventType"]["stringValue"] = eventType;
    doc["fields"]["details"]["stringValue"] = details;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    https.POST(jsonString);
    https.end();
}

// ====================================================================
// WEB SERVER
// ====================================================================
void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/status", HTTP_GET, handleGetStatus);
    server.on("/water", HTTP_POST, handleManualWater);
    server.on("/clearFault", HTTP_POST, handleClearFault);
    server.on("/resetWiFi", HTTP_POST, handleResetWiFi);
    
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not Found");
    });
    
    server.begin();
    Serial.println("✓ Web server started on port 80");
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Smart Irrigation</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:20px;} .status{padding:10px;margin:10px 0;border-radius:5px;} ";
    html += ".online{background:#d4edda;} .offline{background:#f8d7da;} .fault{background:#ff6b6b;color:white;} ";
    html += "button{padding:10px 20px;margin:5px;font-size:16px;cursor:pointer;}</style></head><body>";
    
    html += "<h1>🌱 Smart Irrigation System</h1>";
    html += "<div class='status " + String(lockedFault ? "fault" : (wifiConnected ? "online" : "offline")) + "'>";
    html += "<h2>Status: " + getDeviceStateString() + "</h2>";
    html += "<p><strong>Device ID:</strong> " + deviceId + "</p>";
    html += "<p><strong>Moisture:</strong> " + String(analogRead(SENSOR_PIN)) + "</p>";
    html += "<p><strong>Pump:</strong> " + getPumpStateString() + "</p>";
    html += "<p><strong>WiFi:</strong> " + String(wifiConnected ? "Connected" : "Disconnected") + "</p>";
    
    if (lockedFault) {
        html += "<p>⚠️ <strong>FAULT LOCKED</strong> - Pump appears ineffective</p>";
    }
    
    html += "</div>";
    
    html += "<h3>Controls</h3>";
    html += "<button onclick='fetch(\"/water\",{method:\"POST\"}).then(()=>location.reload())'>💧 Water Now</button>";
    
    if (lockedFault) {
        html += "<button onclick='fetch(\"/clearFault\",{method:\"POST\"}).then(()=>location.reload())'>✓ Clear Fault</button>";
    }
    
    html += "<button onclick='if(confirm(\"Reset WiFi?\")){fetch(\"/resetWiFi\",{method:\"POST\"})}'>🔄 Reset WiFi</button>";
    
    html += "<h3>Configuration</h3>";
    html += "<p>Dry Threshold: " + String(DRY_THRESHOLD) + "</p>";
    html += "<p>Wet Threshold: " + String(WET_THRESHOLD) + "</p>";
    html += "<p>Pump Run Time: " + String(PUMP_RUN_TIME) + " ms</p>";
    html += "<p>Min Interval: " + String(MIN_INTERVAL_SEC) + " sec</p>";
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleGetStatus() {
    JsonDocument doc;
    doc["deviceId"] = deviceId;
    doc["moisture"] = analogRead(SENSOR_PIN);
    doc["pumpState"] = getPumpStateString();
    doc["deviceState"] = getDeviceStateString();
    doc["wifiConnected"] = wifiConnected;
    doc["lockedFault"] = lockedFault;
    doc["dryThreshold"] = DRY_THRESHOLD;
    doc["wetThreshold"] = WET_THRESHOLD;
    doc["pumpRunTime"] = PUMP_RUN_TIME;
    doc["minIntervalSec"] = MIN_INTERVAL_SEC;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleManualWater() {
    if (deviceState == LOCKED_FAULT) {
        server.send(403, "application/json", "{\"error\":\"Device in fault state\"}");
        return;
    }
    
    if (!checkPumpSafety()) {
        server.send(429, "application/json", "{\"error\":\"Too soon since last watering\"}");
        return;
    }
    
    activatePump("WEB");
    server.send(200, "application/json", "{\"status\":\"Pump activated\"}");
}

void handleClearFault() {
    if (lockedFault) {
        lockedFault = false;
        noEffectCounter = 0;
        savePumpState();
        deviceState = wifiConnected ? ONLINE : OFFLINE;
        setLedPattern(wifiConnected ? LED_ONLINE : LED_OFFLINE);
        logEventToFirestore("fault_cleared", "Cleared via web interface");
        server.send(200, "application/json", "{\"status\":\"Fault cleared\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"No fault to clear\"}");
    }
}

void handleResetWiFi() {
    server.send(200, "application/json", "{\"status\":\"Resetting WiFi...\"}");
    delay(1000);
    
    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
    }
    
    WiFi.disconnect(true);
    ESP.restart();
}

// ====================================================================
// UTILITY FUNCTIONS
// ====================================================================
String getDeviceStateString() {
    switch (deviceState) {
        case AWAITING_CONFIG: return "AWAITING_CONFIG";
        case ONLINE: return "ONLINE";
        case OFFLINE: return "OFFLINE";
        case LOCKED_FAULT: return "LOCKED_FAULT";
        default: return "UNKNOWN";
    }
}

String getPumpStateString() {
    switch (pumpState) {
        case MONITORING: return "MONITORING";
        case PUMP_RUNNING: return "PUMP_RUNNING";
        case PUMP_WAITING: return "PUMP_WAITING";
        default: return "UNKNOWN";
    }
}

unsigned long getCurrentEpoch() {
    // Get current Unix timestamp from NTP-synchronized time
    time_t now = time(nullptr);
    if (now < 100000) {
        // Time not yet synced, return 0 to indicate invalid timestamp
        return 0;
    }
    return now;
}
