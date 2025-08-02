#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ===== DEVICE CONFIGURATION ===== //
const String DEVICE_ID = "BW-" + String(ESP.getChipId(), HEX); // Unique device ID based on chip ID
const String DEVICE_VERSION = "1.0.0";
const String DEVICE_TYPE = "plant-watering-system";

// ===== FIREBASE CONFIGURATION ===== //
#define FIREBASE_PROJECT_ID "bloom-watch-d6878"
// You'll need to get this from Firebase Console -> Project Settings -> Web API Key
#define FIREBASE_API_KEY "AIzaSyCt74gYV9dmCm84lBK6RFBP4z7gLOrjjdo"

const String FIRESTORE_URL = "https://firestore.googleapis.com/v1/projects/" + String(FIREBASE_PROJECT_ID) + "/databases/(default)/documents";

// ===== FILE SYSTEM CONFIGURATION ===== //
const char* CONFIG_FILE = "/config.json";
const char* DEVICE_CONFIG_FILE = "/device_config.json";

// ===== WEB SERVER ===== //
ESP8266WebServer server(80);
WiFiManager wm;

// ===== HARDWARE PINS ===== //
constexpr uint8_t PUMP_CTRL_PIN = D1;   // ULN2003 IN1/O1
constexpr uint8_t SENSOR_PIN = A0;      // Moisture sensor output
constexpr uint8_t STATUS_LED_PIN = D4;  // Built-in LED for status indication

// ===== SENSOR THRESHOLDS (can be updated from app) ===== //
uint16_t DRY_THRESHOLD = 520;  // Start pumping when moisture reaches this value
uint16_t WET_THRESHOLD = 420;  // Stop pumping when moisture reaches this value

// ===== TIMING CONFIGURATION ===== //
unsigned long lastDisplayTime = 0;
unsigned long lastPumpActionTime = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastConfigSync = 0;
unsigned long lastHeartbeat = 0;

const unsigned long DISPLAY_INTERVAL = 3000;     // 3 seconds
const unsigned long FIREBASE_UPDATE_INTERVAL = 5000;  // 5 seconds
const unsigned long CONFIG_SYNC_INTERVAL = 30000;     // 30 seconds
const unsigned long HEARTBEAT_INTERVAL = 60000;       // 1 minute
unsigned long PUMP_RUN_TIME = 2000;              // 2 seconds (configurable)
const unsigned long PUMP_WAIT_TIME = 60000;      // 1 minute

// ===== PUMP STATE MACHINE ===== //
enum PumpState {
  MONITORING,   // Waiting for moisture to reach dry threshold
  PUMP_RUNNING, // Pump is currently running
  PUMP_WAITING  // Waiting before next check
};

PumpState currentState = MONITORING;
unsigned long pumpStartTime = 0;

// ===== CONNECTION STATE ===== //
bool wifiConnected = false;
bool firestoreConnected = false;
String lastError = "";
int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 3;

// ===== HTTP CLIENT ===== //
WiFiClientSecure httpsClient;
HTTPClient http;

// ===== DEVICE REGISTRATION STATE ===== //
bool deviceRegistered = false;
String ownerUserId = "";

// ===== FUNCTION DECLARATIONS ===== //
void setupWiFi();
void setupFirestore();
void setupHardware();
void setupWebServer();
bool connectToFirestore();
bool updateSensorDataToFirestore();
bool syncConfigurationFromFirestore();
bool sendHeartbeatToFirestore();
bool checkDeviceRegistrationFromFirestore();
void handlePumpControl();
void handleDeviceRegistration();
void blinkStatusLED(int times, int delayMs = 200);
bool saveDeviceConfig();
bool loadDeviceConfig();
String makeFirestoreRequest(String method, String path, String payload = "");
String getCurrentTimestamp();

// ===== WEB SERVER HANDLERS ===== //
void handleRoot();
void handleStatus();
void handleRegister();
void handleReset();

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Bloom Watch Plant Watering System ===");
  Serial.println("Device ID: " + DEVICE_ID);
  Serial.println("Version: " + DEVICE_VERSION);
  
  // Initialize file system
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
  }
  
  setupHardware();
  setupWiFi();
  setupWebServer();
  setupFirestore();
  
  // Load device configuration
  loadDeviceConfig();
  
  Serial.println("Setup complete. Starting main loop...");
  blinkStatusLED(3, 500); // Indicate successful startup
}

void loop() {
  unsigned long currentTime = millis();
  
  // Handle web server requests
  server.handleClient();
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      wifiConnected = false;
      firestoreConnected = false;
    }
    return;
  } else if (!wifiConnected) {
    Serial.println("WiFi connected!");
    wifiConnected = true;
    connectToFirestore();
  }
  
  // Display sensor readings every 3 seconds
  if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
    uint16_t moisture = analogRead(SENSOR_PIN);
    Serial.printf("Moisture: %d | State: %s | Device: %s | Registered: %s\n", 
                  moisture, 
                  currentState == MONITORING ? "MONITORING" : 
                  currentState == PUMP_RUNNING ? "PUMP_RUNNING" : "PUMP_WAITING",
                  DEVICE_ID.c_str(),
                  deviceRegistered ? "YES" : "NO");
    lastDisplayTime = currentTime;
  }
  
  // Main pump control logic
  handlePumpControl();
  
  // Firestore operations (only if connected and registered)
  if (firestoreConnected && deviceRegistered) {
    // Update sensor data to Firestore
    if (currentTime - lastFirebaseUpdate >= FIREBASE_UPDATE_INTERVAL) {
      updateSensorDataToFirestore();
      lastFirebaseUpdate = currentTime;
    }
    
    // Sync configuration from Firestore
    if (currentTime - lastConfigSync >= CONFIG_SYNC_INTERVAL) {
      syncConfigurationFromFirestore();
      lastConfigSync = currentTime;
    }
    
    // Send heartbeat
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      sendHeartbeatToFirestore();
      lastHeartbeat = currentTime;
    }
  } else if (firestoreConnected && !deviceRegistered) {
    // Check for device registration
    if (currentTime - lastConfigSync >= CONFIG_SYNC_INTERVAL) {
      checkDeviceRegistrationFromFirestore();
      lastConfigSync = currentTime;
    }
  }
  
  delay(100); // Small delay for stability
}

void setupHardware() {
  pinMode(PUMP_CTRL_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(PUMP_CTRL_PIN, LOW);  // Pump OFF
  digitalWrite(STATUS_LED_PIN, HIGH); // LED OFF (inverted logic)
  
  Serial.println("Hardware initialized");
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalTimeout(300); // 5 minutes timeout
  wm.setConnectTimeout(30);
  
  // Try to connect with saved credentials
  if (!wm.autoConnect("BloomWatch-" + String(ESP.getChipId(), HEX).c_str(), "bloom123456")) {
    Serial.println("Failed to connect to WiFi");
    ESP.restart();
  }
  
  wifiConnected = true;
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupFirestore() {
  // Configure time for SSL certificates
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Set up SSL client
  httpsClient.setInsecure(); // For simplicity, use insecure connection
  
  Serial.println("Firestore client configured");
  connectToFirestore();
}

bool connectToFirestore() {
  if (!wifiConnected) return false;
  
  Serial.println("Testing Firestore connection...");
  
  // Test connection by trying to get device document
  String response = makeFirestoreRequest("GET", "/devices/" + DEVICE_ID);
  
  if (response.length() > 0 && !response.startsWith("ERROR")) {
    firestoreConnected = true;
    Serial.println("Firestore connected successfully!");
    blinkStatusLED(2, 100);
    return true;
  } else {
    firestoreConnected = false;
    Serial.println("Firestore connection failed: " + response);
    lastError = response;
    return false;
  }
}

void handlePumpControl() {
  unsigned long currentTime = millis();
  
  switch (currentState) {
    case MONITORING: {
      uint16_t moisture = analogRead(SENSOR_PIN);
      if (moisture >= DRY_THRESHOLD) {
        // Soil is dry → start pumping
        digitalWrite(PUMP_CTRL_PIN, HIGH);
        currentState = PUMP_RUNNING;
        pumpStartTime = currentTime;
        Serial.println("PUMP: ON (soil is dry)");
        digitalWrite(STATUS_LED_PIN, LOW); // LED ON
      }
      break;
    }
    
    case PUMP_RUNNING: {
      if (currentTime - pumpStartTime >= PUMP_RUN_TIME) {
        // Turn pump OFF after specified time
        digitalWrite(PUMP_CTRL_PIN, LOW);
        currentState = PUMP_WAITING;
        lastPumpActionTime = currentTime;
        Serial.println("PUMP: OFF (timer completed)");
        digitalWrite(STATUS_LED_PIN, HIGH); // LED OFF
      }
      break;
    }
    
    case PUMP_WAITING: {
      if (currentTime - lastPumpActionTime >= PUMP_WAIT_TIME) {
        // Check moisture level after waiting
        uint16_t moisture = analogRead(SENSOR_PIN);
        Serial.printf("Post-wait check - Moisture: %d\n", moisture);
        
        if (moisture <= WET_THRESHOLD) {
          // Target reached → return to monitoring
          currentState = MONITORING;
          Serial.println("TARGET REACHED: Returning to monitoring");
        } else {
          // Still dry → pump again
          digitalWrite(PUMP_CTRL_PIN, HIGH);
          currentState = PUMP_RUNNING;
          pumpStartTime = currentTime;
          Serial.println("PUMP: ON again (still dry)");
          digitalWrite(STATUS_LED_PIN, LOW); // LED ON
        }
      }
      break;
    }
  }
}

bool updateSensorDataToFirestore() {
  if (!firestoreConnected) return false;
  
  uint16_t moisture = analogRead(SENSOR_PIN);
  bool pumpStatus = digitalRead(PUMP_CTRL_PIN);
  
  // Create sensor data JSON
  DynamicJsonDocument doc(1024);
  doc["fields"]["moisture"]["integerValue"] = String(moisture);
  doc["fields"]["pumpStatus"]["booleanValue"] = pumpStatus;
  doc["fields"]["state"]["stringValue"] = currentState == MONITORING ? "monitoring" : 
                                          currentState == PUMP_RUNNING ? "pumping" : "waiting";
  doc["fields"]["timestamp"]["timestampValue"] = getCurrentTimestamp();
  doc["fields"]["dryThreshold"]["integerValue"] = String(DRY_THRESHOLD);
  doc["fields"]["wetThreshold"]["integerValue"] = String(WET_THRESHOLD);
  doc["fields"]["isOnline"]["booleanValue"] = true;
  doc["fields"]["lastSeen"]["timestampValue"] = getCurrentTimestamp();
  
  String payload;
  serializeJson(doc, payload);
  
  String response = makeFirestoreRequest("PATCH", "/devices/" + DEVICE_ID, payload);
  
  if (response.startsWith("ERROR")) {
    Serial.println("Failed to update sensor data: " + response);
    return false;
  } else {
    Serial.println("Sensor data updated to Firestore");
    return true;
  }
}

bool syncConfigurationFromFirestore() {
  if (!firestoreConnected || !deviceRegistered) return false;
  
  String response = makeFirestoreRequest("GET", "/devices/" + DEVICE_ID);
  
  if (response.startsWith("ERROR")) {
    Serial.println("Failed to sync configuration: " + response);
    return false;
  }
  
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, response);
  
  // Check if config exists and update thresholds
  if (doc["fields"]["config"]["mapValue"]["fields"].is<JsonObject>()) {
    auto config = doc["fields"]["config"]["mapValue"]["fields"];
    
    if (config["dryThreshold"]["integerValue"].is<const char*>()) {
      uint16_t newDryThreshold = String(config["dryThreshold"]["integerValue"].as<const char*>()).toInt();
      if (newDryThreshold != DRY_THRESHOLD && newDryThreshold > 0 && newDryThreshold < 1024) {
        DRY_THRESHOLD = newDryThreshold;
        Serial.println("Updated dry threshold to: " + String(DRY_THRESHOLD));
      }
    }
    
    if (config["wetThreshold"]["integerValue"].is<const char*>()) {
      uint16_t newWetThreshold = String(config["wetThreshold"]["integerValue"].as<const char*>()).toInt();
      if (newWetThreshold != WET_THRESHOLD && newWetThreshold > 0 && newWetThreshold < 1024) {
        WET_THRESHOLD = newWetThreshold;
        Serial.println("Updated wet threshold to: " + String(WET_THRESHOLD));
      }
    }
    
    if (config["pumpRunTime"]["integerValue"].is<const char*>()) {
      unsigned long newPumpTime = String(config["pumpRunTime"]["integerValue"].as<const char*>()).toInt();
      if (newPumpTime != PUMP_RUN_TIME && newPumpTime > 0 && newPumpTime < 30000) {
        PUMP_RUN_TIME = newPumpTime;
        Serial.println("Updated pump run time to: " + String(PUMP_RUN_TIME));
      }
    }
    
    saveDeviceConfig(); // Save updated configuration
  }
  
  return true;
}

bool sendHeartbeatToFirestore() {
  if (!firestoreConnected) return false;
  
  DynamicJsonDocument doc(1024);
  doc["fields"]["heartbeat"]["mapValue"]["fields"]["timestamp"]["timestampValue"] = getCurrentTimestamp();
  doc["fields"]["heartbeat"]["mapValue"]["fields"]["uptime"]["integerValue"] = String(millis());
  doc["fields"]["heartbeat"]["mapValue"]["fields"]["freeHeap"]["integerValue"] = String(ESP.getFreeHeap());
  doc["fields"]["heartbeat"]["mapValue"]["fields"]["wifiRSSI"]["integerValue"] = String(WiFi.RSSI());
  doc["fields"]["heartbeat"]["mapValue"]["fields"]["version"]["stringValue"] = DEVICE_VERSION;
  
  String payload;
  serializeJson(doc, payload);
  
  String response = makeFirestoreRequest("PATCH", "/devices/" + DEVICE_ID, payload);
  
  return !response.startsWith("ERROR");
}

bool checkDeviceRegistrationFromFirestore() {
  if (!firestoreConnected) return false;
  
  String response = makeFirestoreRequest("GET", "/devices/" + DEVICE_ID);
  
  if (response.startsWith("ERROR")) {
    // Device doesn't exist, create it
    handleDeviceRegistration();
    return false;
  }
  
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, response);
  
  if (doc["fields"]["registered"]["booleanValue"].is<bool>() && 
      doc["fields"]["registered"]["booleanValue"].as<bool>()) {
    deviceRegistered = true;
    
    // Get owner user ID
    if (doc["fields"]["userId"]["stringValue"].is<const char*>()) {
      ownerUserId = doc["fields"]["userId"]["stringValue"].as<const char*>();
      Serial.println("Device registered to user: " + ownerUserId);
      saveDeviceConfig();
      return true;
    }
  }
  
  return false;
}

void handleDeviceRegistration() {
  DynamicJsonDocument doc(1024);
  doc["fields"]["deviceId"]["stringValue"] = DEVICE_ID;
  doc["fields"]["type"]["stringValue"] = DEVICE_TYPE;
  doc["fields"]["version"]["stringValue"] = DEVICE_VERSION;
  doc["fields"]["name"]["stringValue"] = "Plant Watering System";
  doc["fields"]["registered"]["booleanValue"] = false;
  doc["fields"]["createdAt"]["timestampValue"] = getCurrentTimestamp();
  doc["fields"]["ipAddress"]["stringValue"] = WiFi.localIP().toString();
  doc["fields"]["macAddress"]["stringValue"] = WiFi.macAddress();
  doc["fields"]["isOnline"]["booleanValue"] = true;
  doc["fields"]["lastSeen"]["timestampValue"] = getCurrentTimestamp();
  
  String payload;
  serializeJson(doc, payload);
  
  String response = makeFirestoreRequest("PATCH", "/devices/" + DEVICE_ID, payload);
  
  if (!response.startsWith("ERROR")) {
    Serial.println("Device information posted to Firestore. Waiting for registration...");
  } else {
    Serial.println("Failed to post device info: " + response);
  }
}

String makeFirestoreRequest(String method, String path, String payload) {
  if (!wifiConnected) return "ERROR: WiFi not connected";
  
  String url = FIRESTORE_URL + path;
  if (method == "GET" || method == "DELETE") {
    // Add API key for GET/DELETE requests
    url += "?key=" + String(FIREBASE_API_KEY);
  }
  
  http.begin(httpsClient, url);
  http.addHeader("Content-Type", "application/json");
  
  if (method != "GET" && method != "DELETE") {
    // Add API key for POST/PATCH requests
    http.addHeader("Authorization", "Bearer " + String(FIREBASE_API_KEY));
  }
  
  int httpResponseCode;
  
  if (method == "GET") {
    httpResponseCode = http.GET();
  } else if (method == "POST") {
    httpResponseCode = http.POST(payload);
  } else if (method == "PATCH") {
    httpResponseCode = http.PATCH(payload);
  } else if (method == "DELETE") {
    httpResponseCode = http.sendRequest("DELETE");
  } else {
    http.end();
    return "ERROR: Invalid HTTP method";
  }
  
  String response;
  if (httpResponseCode > 0) {
    response = http.getString();
    if (httpResponseCode >= 400) {
      response = "ERROR: HTTP " + String(httpResponseCode) + " - " + response;
    }
  } else {
    response = "ERROR: HTTP request failed - " + String(httpResponseCode);
  }
  
  http.end();
  return response;
}

String getCurrentTimestamp() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
  
  return String(timestamp);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/reset", handleReset);
  
  server.begin();
  Serial.println("Web server started on port 80");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Bloom Watch Device</title></head><body>";
  html += "<h1>Bloom Watch Plant Watering System</h1>";
  html += "<p><strong>Device ID:</strong> " + DEVICE_ID + "</p>";
  html += "<p><strong>Version:</strong> " + DEVICE_VERSION + "</p>";
  html += "<p><strong>WiFi Status:</strong> " + (wifiConnected ? "Connected" : "Disconnected") + "</p>";
  html += "<p><strong>Firestore Status:</strong> " + (firestoreConnected ? "Connected" : "Disconnected") + "</p>";
  html += "<p><strong>Registration Status:</strong> " + (deviceRegistered ? "Registered" : "Pending") + "</p>";
  html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<p><strong>Moisture Level:</strong> " + String(analogRead(SENSOR_PIN)) + "</p>";
  html += "<p><strong>Pump Status:</strong> " + (digitalRead(PUMP_CTRL_PIN) ? "ON" : "OFF") + "</p>";
  html += "<hr>";
  html += "<p><a href='/status'>View Status JSON</a></p>";
  html += "<p><a href='/reset'>Reset Device</a></p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  DynamicJsonDocument status(1024);
  status["deviceId"] = DEVICE_ID;
  status["version"] = DEVICE_VERSION;
  status["wifiConnected"] = wifiConnected;
  status["firestoreConnected"] = firestoreConnected;
  status["deviceRegistered"] = deviceRegistered;
  status["moisture"] = analogRead(SENSOR_PIN);
  status["pumpStatus"] = digitalRead(PUMP_CTRL_PIN);
  status["uptime"] = millis();
  status["freeHeap"] = ESP.getFreeHeap();
  status["lastError"] = lastError;
  
  String response;
  serializeJson(status, response);
  server.send(200, "application/json", response);
}

void handleRegister() {
  // This endpoint could be used for manual registration
  server.send(200, "text/plain", "Registration endpoint - use the mobile app to register this device");
}

void handleReset() {
  server.send(200, "text/plain", "Resetting device...");
  delay(1000);
  ESP.restart();
}

void blinkStatusLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, LOW);  // LED ON
    delay(delayMs);
    digitalWrite(STATUS_LED_PIN, HIGH); // LED OFF
    delay(delayMs);
  }
}

bool saveDeviceConfig() {
  File configFile = LittleFS.open(DEVICE_CONFIG_FILE, "w");
  if (!configFile) return false;
  
  DynamicJsonDocument config(512);
  config["deviceRegistered"] = deviceRegistered;
  config["ownerUserId"] = ownerUserId;
  config["dryThreshold"] = DRY_THRESHOLD;
  config["wetThreshold"] = WET_THRESHOLD;
  config["pumpRunTime"] = PUMP_RUN_TIME;
  
  serializeJson(config, configFile);
  configFile.close();
  return true;
}

bool loadDeviceConfig() {
  if (!LittleFS.exists(DEVICE_CONFIG_FILE)) return false;
  
  File configFile = LittleFS.open(DEVICE_CONFIG_FILE, "r");
  if (!configFile) return false;
  
  DynamicJsonDocument config(512);
  DeserializationError error = deserializeJson(config, configFile);
  configFile.close();
  
  if (error) return false;
  
  deviceRegistered = config["deviceRegistered"] | false;
  ownerUserId = config["ownerUserId"] | "";
  DRY_THRESHOLD = config["dryThreshold"] | 520;
  WET_THRESHOLD = config["wetThreshold"] | 420;
  PUMP_RUN_TIME = config["pumpRunTime"] | 2000;
  
  return true;
}
