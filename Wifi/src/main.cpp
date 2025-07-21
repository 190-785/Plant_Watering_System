#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>

// ===== File System Configuration ===== //
const char* CONFIG_FILE = "/config.json";

// ===== Google Sheets Configuration ===== //
String googleScriptURL = "https://script.google.com/macros/s/AKfycbxLn8vDw9lEkvFkB8ydj70U-rb3WeLXx3-dZnO5796gHlErpX6tEUbVKlyt5VhsjpcTCw/exec"; // Hardcoded - replace with your actual URL
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 6000; // Send data every 1 minute

// ===== Web Server ===== //
ESP8266WebServer server(80);

// WiFiManager instance
WiFiManager wm;

// Custom parameters removed - simplified WiFi authentication

// WiFi state tracking
bool wifiConnected = false;
const int MAX_CONNECTION_ATTEMPTS = 3; // Reduced from 5
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 10000; // Increased to 10 seconds
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000; // Check WiFi status every 5 seconds
unsigned long lastInternetCheck = 0;
const unsigned long INTERNET_CHECK_INTERVAL = 300000; // Check internet every 5 minutes (reduced frequency)
int consecutiveFailures = 0;
const int MAX_CONSECUTIVE_FAILURES = 3;

// Function declarations
void startConfigurationPortal();
void connectWiFi();
void checkWiFi();
bool checkInternet();
void sendDataToGoogleSheets(uint16_t moisture, const String& pumpStatus);
void setupWebServer();
void handleRoot();
void handleGetStatus();
void handleSetThreshold();
void handleSetPumpTime();
void handleTestGoogleSheets();

// ===== Irrigation System Constants ===== //
constexpr uint8_t PUMP_CTRL_PIN = D1;
constexpr uint8_t SENSOR_PIN = A0;
uint16_t DRY_THRESHOLD = 520;    // Changed to variable for app control
uint16_t WET_THRESHOLD = 420;    // Changed to variable for app control

// Timing variables
unsigned long lastDisplayTime = 0;
unsigned long lastPumpActionTime = 0;
const unsigned long DISPLAY_INTERVAL = 3000;
unsigned long PUMP_RUN_TIME = 2000;      // Changed to variable for app control
const unsigned long PUMP_WAIT_TIME = 60000;

// Pump state variables
enum PumpState { MONITORING, PUMP_RUNNING, PUMP_WAITING };
PumpState currentState = MONITORING;
unsigned long pumpStartTime = 0;

// ===== File System Functions ===== //
bool saveConfig(const String& ssid, const String& pass, const String& gurl = "", 
                uint16_t dryThresh = 520, uint16_t wetThresh = 420, unsigned long pumpTime = 2000) {
  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  JsonDocument doc;
  doc["ssid"] = ssid;
  doc["pass"] = pass;
  doc["gurl"] = gurl;
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

bool loadConfig(String& ssid, String& pass, String& gurl, 
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

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
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
  gurl = doc["gurl"].as<String>();
  dryThresh = doc["dryThreshold"] | 520;  // Default to 520 if not found
  wetThresh = doc["wetThreshold"] | 420;  // Default to 420 if not found
  pumpTime = doc["pumpRunTime"] | 2000;   // Default to 2000ms if not found

  // Debug output
  Serial.println("=== Config file loaded ===");
  Serial.println("SSID: " + ssid);
  Serial.println("Google URL from file: '" + gurl + "' (length: " + String(gurl.length()) + ")");
  Serial.println("Dry Threshold: " + String(dryThresh));
  Serial.println("Wet Threshold: " + String(wetThresh));
  Serial.println("Pump Time: " + String(pumpTime));
  Serial.println("==========================");

  configFile.close();
  return true;
}

// ===== WiFi Functions ===== //

void setupWiFi() {
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
    }
    
    // Enhanced WiFi settings for stability
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoReconnect(true); // Enable auto-reconnect
    WiFi.persistent(true); // Enable persistent storage
    WiFi.setAutoConnect(true); // Enable auto-connect
    
    // Set power management
    WiFi.setOutputPower(20.5); // Maximum power for better signal
    
    wm.setConnectTimeout(30); // Increased timeout
    wm.setConfigPortalTimeout(0);
}

void startConfigurationPortal() {
    Serial.println("Starting configuration portal...");
    
    // Completely disconnect and clear any existing WiFi settings
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(2000); // Increased delay
    
    // Reset WiFiManager settings
    wm.resetSettings();
    
    // Configure WiFiManager for stable AP mode
    wm.setConfigPortalTimeout(0);
    wm.setConnectTimeout(30);
    wm.setDebugOutput(false); // Reduced debug output
    wm.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    
    Serial.println("Starting access point: IrrigationAP");
    Serial.println("Password: plant123456");
    Serial.println("IP: 192.168.4.1");
    Serial.println("Connect to this network and go to http://192.168.4.1");
    
    if (!wm.startConfigPortal("IrrigationAP", "plant123456")) {
        Serial.println("Failed to start config portal");
        return;
    }
    
    String ssid = WiFi.SSID();
    String pass = WiFi.psk();
    
    if (saveConfig(ssid, pass, googleScriptURL, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME)) {
        Serial.println("Credentials saved to LittleFS");
        wifiConnected = true;
        consecutiveFailures = 0; // Reset failure counter
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
    
    String ssid, pass, gurl;
    uint16_t dryThresh, wetThresh;
    unsigned long pumpTime;
    
    if (!loadConfig(ssid, pass, gurl, dryThresh, wetThresh, pumpTime)) {
        Serial.println("No saved credentials, starting portal");
        startConfigurationPortal();
        return;
    }
    
    // Load saved parameters with debugging
    Serial.println("Loaded configuration:");
    Serial.println("  SSID: " + ssid);
    Serial.println("  Google URL: " + gurl);
    Serial.println("  Dry Threshold: " + String(dryThresh));
    Serial.println("  Wet Threshold: " + String(wetThresh));
    Serial.println("  Pump Time: " + String(pumpTime));
    
    if (gurl.length() > 0) {
        googleScriptURL = gurl;
        Serial.println("Google Script URL updated from config: " + googleScriptURL);
    } else {
        Serial.println("No Google Script URL in config, using default: " + googleScriptURL);
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
            Serial.print("SSID: ");
            Serial.println(WiFi.SSID());
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            Serial.print("Signal Strength (RSSI): ");
            Serial.println(WiFi.RSSI());

            consecutiveFailures = 0; // Reset failure counter
            
            if (!checkInternet()) {
                Serial.println("WiFi connected, but no internet access.");
                consecutiveFailures++;
            } else {
                Serial.println("Internet access confirmed.");
            }
            return;
        }
        
        Serial.printf("\nConnection failed (Attempt %d/%d)\n", attempt, MAX_CONNECTION_ATTEMPTS);
        delay(2000); // Increased delay between attempts
    }
    
    Serial.println("All connection attempts failed");
    consecutiveFailures++;
    
    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
        startConfigurationPortal();
    }
}

void checkWiFi() {
    unsigned long currentTime = millis();
    
    // Only check WiFi status at intervals to reduce overhead
    if (currentTime - lastWiFiCheck < WIFI_CHECK_INTERVAL) {
        return;
    }
    lastWiFiCheck = currentTime;
    
    wl_status_t status = WiFi.status();
    
    if (status != WL_CONNECTED) {
        if (wifiConnected) {
            Serial.printf("WiFi connection lost - Status: %d\n", status);
            Serial.print("Signal Strength was: ");
            Serial.println(WiFi.RSSI());
            wifiConnected = false;
        }
        
        if (currentTime - lastReconnectAttempt > RECONNECT_INTERVAL) {
            Serial.println("Attempting reconnection...");
            lastReconnectAttempt = currentTime;
            
            // Try a simple reconnect first
            WiFi.reconnect();
            delay(5000);
            
            if (WiFi.status() != WL_CONNECTED) {
                connectWiFi();
            }
        }
    } else {
        if (!wifiConnected) {
            Serial.println("WiFi reconnected successfully");
            wifiConnected = true;
        }
        
        // Periodic internet check (every 5 minutes - reduced frequency)
        if (currentTime - lastInternetCheck > INTERNET_CHECK_INTERVAL) {
            lastInternetCheck = currentTime;
            Serial.println("Checking internet connectivity...");
            if (!checkInternet()) {
                Serial.println("Internet connection lost. Starting configuration portal.");
                startConfigurationPortal(); // Force re-configuration
            }
        }
    }
}

bool checkInternet() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    WiFiClient client;
    client.setTimeout(5000); // 5 second timeout
    
    // Try multiple DNS servers for better reliability
    IPAddress dnsServers[] = {
        IPAddress(8, 8, 8, 8),    // Google
        IPAddress(1, 1, 1, 1),    // Cloudflare
        IPAddress(208, 67, 222, 222) // OpenDNS
    };
    
    for (int i = 0; i < 3; i++) {
        if (client.connect(dnsServers[i], 53)) {
            client.stop();
            return true;
        }
        delay(1000);
    }
    
    return false;
}

// ===== Google Sheets Integration ===== //
void sendDataToGoogleSheets(uint16_t moisture, const String& pumpStatus) {
    Serial.println("=== Sending data to Google Sheets ===");
    Serial.println("URL: " + googleScriptURL);
    Serial.println("URL Length: " + String(googleScriptURL.length()));
    
    if (!wifiConnected) {
        Serial.println("Cannot send data: WiFi not connected");
        return;
    }
    
    if (googleScriptURL.length() == 0) {
        Serial.println("Cannot send data: No Google Script URL configured");
        return;
    }
    
    // Check if it's the default hardcoded URL (means not configured by user)
    if (googleScriptURL == "https://script.google.com/macros/s/YOUR_NEW_UNIQUE_URL_HERE") {
        Serial.println("Cannot send data: Using default URL - please configure your own Google Script URL");
        return;
    }
    Serial.println("Moisture: " + String(moisture));
    Serial.println("Pump Status: " + pumpStatus);
    
    WiFiClientSecure client;
    
    // Set SSL/TLS configuration for better compatibility
    client.setInsecure(); // Skip certificate verification for Google Apps Script
    client.setTimeout(15000); // 15 second timeout
    
    // Try to set a specific cipher suite for better compatibility
    #ifdef DEBUG_ESP_SSL
    Serial.println("SSL Debug enabled");
    #endif
    
    HTTPClient http;
    
    // Begin connection with detailed error checking
    Serial.println("Initializing HTTP connection...");
    if (!http.begin(client, googleScriptURL)) {
        Serial.println("HTTP begin failed!");
        return;
    }
    
    // Set headers
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("User-Agent", "ESP8266-PlantWatering/1.0");
    http.addHeader("Accept", "application/json,text/plain");
    
    // Set timeout
    http.setTimeout(15000);
    
    // Prepare data to send
    String postData = "moisture=" + String(moisture) + "&pumpStatus=" + pumpStatus;
    Serial.println("POST Data: " + postData);
    
    // Send POST request
    Serial.println("Sending POST request...");
    int httpResponseCode = http.POST(postData);
    
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Response Length: " + String(response.length()));
        Serial.println("Response: " + response);
        
        if (httpResponseCode == 200 || httpResponseCode == 302) {
            Serial.println("✓ Data sent successfully!");
        } else {
            Serial.println("⚠ Unexpected response code: " + String(httpResponseCode));
        }
    } else {
        // Detailed error reporting
        Serial.println("✗ HTTP Error occurred:");
        switch (httpResponseCode) {
            case -1:
                Serial.println("  Connection failed - Check network or URL");
                break;
            case -2:
                Serial.println("  Send header failed");
                break;
            case -3:
                Serial.println("  Send payload failed");
                break;
            case -4:
                Serial.println("  Not connected");
                break;
            case -5:
                Serial.println("  Connection lost");
                break;
            case -6:
                Serial.println("  No stream");
                break;
            case -7:
                Serial.println("  No HTTP server");
                break;
            case -8:
                Serial.println("  Too less RAM");
                break;
            case -9:
                Serial.println("  Encoding error");
                break;
            case -10:
                Serial.println("  Stream write error");
                break;
            case -11:
                Serial.println("  Read timeout");
                break;
            default:
                Serial.println("  Unknown error: " + String(httpResponseCode));
        }
        
        // Check WiFi status
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("  WiFi disconnected during request");
        } else {
            Serial.println("  WiFi still connected (RSSI: " + String(WiFi.RSSI()) + ")");
        }
    }
    
    http.end();
    Serial.println("=== End Google Sheets request ===");
}

// ===== Web Server Functions ===== //
void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/status", HTTP_GET, handleGetStatus);
    server.on("/setThreshold", HTTP_POST, handleSetThreshold);
    server.on("/setPumpTime", HTTP_POST, handleSetPumpTime);
    server.on("/testSheets", HTTP_GET, handleTestGoogleSheets);
    
    // Enable CORS for all routes
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
    html += "<p>Dry Threshold: " + String(DRY_THRESHOLD) + "</p>";
    html += "<p>Wet Threshold: " + String(WET_THRESHOLD) + "</p>";
    html += "<p>Pump Run Time: " + String(PUMP_RUN_TIME) + "ms</p>";
    html += "<p>Data Logging: " + String((googleScriptURL != "https://script.google.com/macros/s/YOUR_NEW_UNIQUE_URL_HERE") ? "Enabled" : "Disabled") + "</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleGetStatus() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Content-Type", "application/json");
    
    JsonDocument doc;
    doc["moisture"] = analogRead(SENSOR_PIN);
    doc["dryThreshold"] = DRY_THRESHOLD;
    doc["wetThreshold"] = WET_THRESHOLD;
    doc["pumpRunTime"] = PUMP_RUN_TIME;
    doc["pumpState"] = (currentState == MONITORING) ? "MONITORING" : 
                      (currentState == PUMP_RUNNING) ? "PUMP_RUNNING" : "PUMP_WAITING";
    doc["wifiConnected"] = wifiConnected;
    doc["rssi"] = WiFi.RSSI();
    doc["dataLoggingEnabled"] = (googleScriptURL != "https://script.google.com/macros/s/YOUR_NEW_UNIQUE_URL_HERE");
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSetThreshold() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Content-Type", "application/json");
    
    if (server.hasArg("dryThreshold")) {
        int newDryThreshold = server.arg("dryThreshold").toInt();
        if (newDryThreshold > 0 && newDryThreshold < 1024) {
            DRY_THRESHOLD = newDryThreshold;
            Serial.println("Dry threshold updated to: " + String(DRY_THRESHOLD));
        }
    }
    
    if (server.hasArg("wetThreshold")) {
        int newWetThreshold = server.arg("wetThreshold").toInt();
        if (newWetThreshold > 0 && newWetThreshold < 1024) {
            WET_THRESHOLD = newWetThreshold;
            Serial.println("Wet threshold updated to: " + String(WET_THRESHOLD));
        }
    }
    
    // Save to config file
    String ssid, pass, gurl;
    uint16_t dryThresh, wetThresh;
    unsigned long pumpTime;
    if (loadConfig(ssid, pass, gurl, dryThresh, wetThresh, pumpTime)) {
        saveConfig(ssid, pass, gurl, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME);
    }
    
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Thresholds updated\"}");
}

void handleSetPumpTime() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Content-Type", "application/json");
    
    if (server.hasArg("pumpRunTime")) {
        int newPumpTime = server.arg("pumpRunTime").toInt();
        if (newPumpTime > 0 && newPumpTime <= 60000) { // Max 60 seconds
            PUMP_RUN_TIME = newPumpTime;
            Serial.println("Pump run time updated to: " + String(PUMP_RUN_TIME) + "ms");
            
            // Save to config file
            String ssid, pass, gurl;
            uint16_t dryThresh, wetThresh;
            unsigned long pumpTime;
            if (loadConfig(ssid, pass, gurl, dryThresh, wetThresh, pumpTime)) {
                saveConfig(ssid, pass, gurl, DRY_THRESHOLD, WET_THRESHOLD, PUMP_RUN_TIME);
            }
            
            server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Pump time updated\"}");
        } else {
            server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid pump time (1-60000ms)\"}");
        }
    } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing pumpRunTime parameter\"}");
    }
}

void handleTestGoogleSheets() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Content-Type", "application/json");
    
    Serial.println("Manual test of Google Sheets connection requested via web");
    
    // Send test data
    sendDataToGoogleSheets(999, "TEST");
    
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Test request sent - check serial monitor for details\"}");
}

// ===== Core Irrigation Logic ===== //
void setup() {
    Serial.begin(115200);
    delay(2000); // Increased stabilization delay
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
    } else {
        Serial.println("Forcing removal of old config file...");
        LittleFS.remove(CONFIG_FILE);
    }
    // Initialize pump control
    pinMode(PUMP_CTRL_PIN, OUTPUT);
    digitalWrite(PUMP_CTRL_PIN, LOW);
    
    Serial.println("\nSmart Irrigation System v2.0\n");
    Serial.println("Initial Google Script URL: " + googleScriptURL);
    Serial.println("URL Length: " + String(googleScriptURL.length()));
    
    // Set up WiFi with enhanced stability
    setupWiFi();
    connectWiFi();
    
    // Start web server for app control
    setupWebServer();
    
    Serial.println("Dry threshold: " + String(DRY_THRESHOLD) + ", Wet threshold: " + String(WET_THRESHOLD));
    Serial.println("Pump run time: " + String(PUMP_RUN_TIME) + "ms");
    Serial.println("Final Google Script URL: " + googleScriptURL);
    Serial.println("Final URL Length: " + String(googleScriptURL.length()));
    Serial.println("Moisture updates every 3 seconds");
    Serial.println("Data sent to Google Sheets every 1 minute");
    Serial.println("Web server available at: http://" + WiFi.localIP().toString());
    Serial.println("WiFi stability enhancements active");
}

void loop() {
    unsigned long currentTime = millis();
    
    // Handle web server requests
    server.handleClient();
    
    // Check WiFi status (with built-in rate limiting)
    checkWiFi();
    
    // Display moisture reading every 3 seconds
    if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
        uint16_t moisture = analogRead(SENSOR_PIN);
        
        // Build the output string
        String output = "Moisture: " + String(moisture);
        output += " | State: ";
        String pumpStatusStr = "";
        switch (currentState) {
            case MONITORING: 
                output += "MONITORING"; 
                pumpStatusStr = "MONITORING";
                break;
            case PUMP_RUNNING: 
                output += "PUMP_RUNNING"; 
                pumpStatusStr = "PUMP_RUNNING";
                break;
            case PUMP_WAITING: 
                output += "PUMP_WAITING"; 
                pumpStatusStr = "PUMP_WAITING";
                break;
        }
        
        output += " | WiFi: ";
        if (wifiConnected) {
            output += "OK (RSSI: " + String(WiFi.RSSI()) + ")";
        } else {
            output += "DISCONNECTED";
        }
        
        Serial.println(output);
        lastDisplayTime = currentTime;
        
        // Send data to Google Sheets every minute
        if (currentTime - lastDataSend >= DATA_SEND_INTERVAL) {
            sendDataToGoogleSheets(moisture, pumpStatusStr);
            lastDataSend = currentTime;
        }
    }

    // State machine for pump control
    switch (currentState) {
        case MONITORING: {
            uint16_t moisture = analogRead(SENSOR_PIN);
            if (moisture >= DRY_THRESHOLD) {
                digitalWrite(PUMP_CTRL_PIN, HIGH);
                currentState = PUMP_RUNNING;
                pumpStartTime = currentTime;
                Serial.println("PUMP: ON (dry soil detected)");
            }
            break;
        }
        
        case PUMP_RUNNING:
            if (currentTime - pumpStartTime >= PUMP_RUN_TIME) {
                digitalWrite(PUMP_CTRL_PIN, LOW);
                currentState = PUMP_WAITING;
                lastPumpActionTime = currentTime;
                Serial.println("PUMP: OFF (cycle completed)");
            }
            break;
            
        case PUMP_WAITING:
            if (currentTime - lastPumpActionTime >= PUMP_WAIT_TIME) {
                uint16_t moisture = analogRead(SENSOR_PIN);
                Serial.print("Post-cycle check - Moisture: ");
                Serial.println(moisture);
                
                if (moisture <= WET_THRESHOLD) {
                    currentState = MONITORING;
                    Serial.println("TARGET REACHED: Soil adequately moist");
                } else {
                    digitalWrite(PUMP_CTRL_PIN, HIGH);
                    currentState = PUMP_RUNNING;
                    pumpStartTime = currentTime;
                    Serial.println("PUMP: ON (soil still too dry)");
                }
            }
            break;
    }
    
    // Small delay to prevent overwhelming the system
    delay(100);
}