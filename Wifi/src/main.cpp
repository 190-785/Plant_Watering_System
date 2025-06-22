#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>
#include "Config.h"
#include "ProvisionServer.h"
#include "PortalLogin.h"

// WiFi components
ESP8266WebServer server(80);
ProvisionServer provision(server);
WiFiClient client;
bool wifiConnected = false;

// Irrigation System Components (unchanged)
constexpr uint8_t PUMP_CTRL_PIN = D1;   // ULN2003 IN1/O1
constexpr uint8_t SENSOR_PIN = A0;      // Moisture sensor output
constexpr uint16_t DRY_THRESHOLD = 520; // Start pumping when moisture reaches 520 (higher = drier)
constexpr uint16_t WET_THRESHOLD = 420; // Stop pumping when moisture reaches 420 or below

// Timing variables
unsigned long lastDisplayTime = 0;           // For 3-second display updates
unsigned long lastPumpActionTime = 0;        // For pump cycle timing
unsigned long lastDataLogTime = 0;           // For data logging
const unsigned long DISPLAY_INTERVAL = 3000; // 3 seconds in milliseconds
const unsigned long PUMP_RUN_TIME = 1000;    // 1 second pump run time
const unsigned long PUMP_WAIT_TIME = 60000;  // 1 minute wait time
const unsigned long LOG_INTERVAL = 300000;   // 5 minutes data logging interval

// Pump state variables
enum PumpState {
  MONITORING,   // Waiting for moisture to reach 520
  PUMP_RUNNING, // Pump is currently running
  PUMP_WAITING  // Waiting before next check
};

PumpState currentState = MONITORING;
unsigned long pumpStartTime = 0;

// Data logging variables
struct SensorData {
  unsigned long timestamp;
  uint16_t moistureLevel;
  String pumpState;
  bool pumpActive;
};

// DUMMY DATA TESTING - Remove this section when not needed
unsigned long lastDummyLogTime = 0;
const unsigned long DUMMY_LOG_INTERVAL = 3000; // 30 seconds for testing

void sendDummyDataToExcel() {
  // Check WiFi connection more reliably
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📡 WiFi not connected - skipping dummy data log");
    Serial.printf("WiFi Status: %d\n", WiFi.status());
    wifiConnected = false;
    return;
  }

  wifiConnected = true;
  Serial.println("📡 WiFi connected - sending dummy data");

  // Create dummy data
  SensorData dummyData;
  dummyData.timestamp = millis();
  dummyData.moistureLevel = random(300, 600); // Random moisture level
  dummyData.pumpState = "TESTING";
  dummyData.pumpActive = random(0, 2); // Random true/false
  
  HTTPClient http;
  WiFiClientSecure secureClient;
  secureClient.setInsecure(); // Skip certificate verification
  
  http.setTimeout(15000); // 15 second timeout
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(secureClient, "https://script.google.com/macros/s/AKfycbzAqPUst8A5IpKjFPErmVxWjg3wzoKT9Mw6VlfYBAuaLVl-HmsXay4Hfy9Ejipfr9Dp/exec");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP8266");

  // Create JSON payload
  String payload = "{";
  payload += "\"timestamp\":\"" + String(dummyData.timestamp) + "\",";
  payload += "\"moistureLevel\":" + String(dummyData.moistureLevel) + ",";
  payload += "\"pumpState\":\"" + dummyData.pumpState + "\",";
  payload += "\"pumpActive\":" + String(dummyData.pumpActive ? "true" : "false");
  payload += "}";

  Serial.println("🧪 DUMMY DATA - Logging to Excel: " + payload);
    int httpResponseCode = http.POST(payload);
  Serial.printf("HTTP Response Code: %d\n", httpResponseCode);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("✓ DUMMY DATA logged successfully");
    Serial.println("Response: " + response);
  } else if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("✗ DUMMY DATA logging failed: %d\n", httpResponseCode);
    Serial.println("Error response: " + response);
  } else {
    // Handle specific ESP8266 HTTP error codes
    switch (httpResponseCode) {
      case -1: Serial.println("✗ HTTP Error: Connection refused"); break;
      case -2: Serial.println("✗ HTTP Error: Send header failed"); break;
      case -3: Serial.println("✗ HTTP Error: Send payload failed"); break;
      case -4: Serial.println("✗ HTTP Error: Not connected"); break;
      case -5: Serial.println("✗ HTTP Error: Connection lost"); break;
      case -6: Serial.println("✗ HTTP Error: No stream"); break;
      case -7: Serial.println("✗ HTTP Error: No HTTP server"); break;
      case -8: Serial.println("✗ HTTP Error: Too less RAM"); break;
      case -9: Serial.println("✗ HTTP Error: Encoding"); break;
      case -10: Serial.println("✗ HTTP Error: Stream write"); break;
      case -11: Serial.println("✗ HTTP Error: Read timeout"); break;
      default: Serial.printf("✗ HTTP Request failed: %d\n", httpResponseCode); break;
    }
  }
  
  http.end();
}
// END DUMMY DATA TESTING SECTION

void initializeWiFi() {
  Config cfg;
  if (!loadConfig(cfg)) {
    Serial.println("No WiFi config found. Starting provisioning AP...");
    Serial.println("Connect to 'Config-AP' and go to http://192.168.4.1 to configure WiFi");
    provision.begin();
    // Non-blocking provisioning check
    for (int i = 0; i < 100; i++) { // 10 second timeout
      provision.handle();
      delay(100);
    }
    Serial.println("Continuing without WiFi configuration...");
    return;
  }
  Serial.printf("Attempting WiFi connection to %s...\n", cfg.ssid.c_str());
  WiFi.mode(WIFI_STA);
  
  // Add WiFi stability settings
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  if (cfg.wifipass.length())
    WiFi.begin(cfg.ssid.c_str(), cfg.wifipass.c_str());
  else
    WiFi.begin(cfg.ssid.c_str());

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("\n✓ WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Handle captive portal if needed
    if (cfg.doubleAuth && needsPortalLogin(client)) {
      Serial.println("⚠️ Captive portal detected – logging in...");
      const char* portalUrl = cfg.portalUrl.length() > 0 ? cfg.portalUrl.c_str() : "http://172.16.1.1/";
      auto p = fetchPortalForm(client, portalUrl);
      if (loginPortal(client, p, cfg.portalUser.c_str(), cfg.portalPass.c_str())) {
        Serial.println("✓ Portal login successful!");
      } else {
        Serial.println("✗ Portal login failed!");
        wifiConnected = false;
      }
    }
  } else {
    Serial.println("\n✗ WiFi connection failed - continuing without WiFi");
    wifiConnected = false;
  }
}

void logDataToExcel(const SensorData& data) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("📡 WiFi not connected - skipping data log");
    wifiConnected = false;
    return;
  }
  
  wifiConnected = true;
  HTTPClient http;
  WiFiClientSecure secureClient;
  secureClient.setInsecure(); // Skip certificate verification
  
  http.setTimeout(15000); // 15 second timeout
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(secureClient, "https://script.google.com/macros/s/AKfycbzAqPUst8A5IpKjFPErmVxWjg3wzoKT9Mw6VlfYBAuaLVl-HmsXay4Hfy9Ejipfr9Dp/exec");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP8266");

  // Create JSON payload
  String payload = "{";
  payload += "\"timestamp\":\"" + String(data.timestamp) + "\",";
  payload += "\"moistureLevel\":" + String(data.moistureLevel) + ",";
  payload += "\"pumpState\":\"" + data.pumpState + "\",";
  payload += "\"pumpActive\":" + String(data.pumpActive ? "true" : "false");
  payload += "}";

  Serial.println("📊 Logging data to Excel: " + payload);
  
  int httpResponseCode = http.POST(payload);
  Serial.printf("HTTP Response Code: %d\n", httpResponseCode);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("✓ Data logged successfully");
    Serial.println("Response: " + response);
  } else if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("✗ Data logging failed: %d\n", httpResponseCode);
    Serial.println("Error response: " + response);
  } else {
    Serial.printf("✗ HTTP Request failed: %d\n", httpResponseCode);
  }
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  Serial.println("🌱 Smart Irrigation System with WiFi Data Logging");
  
  // Initialize irrigation system
  pinMode(PUMP_CTRL_PIN, OUTPUT);
  digitalWrite(PUMP_CTRL_PIN, LOW); // pump OFF
  
  Serial.println("Irrigation System: Dry threshold: 520, Wet threshold: 420");
  Serial.println("Display updates every 3 seconds, Data logging every 5 minutes");
  
  // Initialize WiFi (non-blocking)
  initializeWiFi();
  
  Serial.println("🚀 System ready - Irrigation system will work regardless of WiFi status");
}

void loop() {
  unsigned long currentTime = millis();

  // 1. Display moisture reading every 3 seconds (unchanged)
  if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
    uint16_t displayMoisture = analogRead(SENSOR_PIN);
    Serial.print("Moisture Level: ");
    Serial.print(displayMoisture);
    Serial.print(" | State: ");

    switch (currentState) {
    case MONITORING:
      Serial.print("MONITORING");
      break;
    case PUMP_RUNNING:
      Serial.print("PUMP_RUNNING");
      break;
    case PUMP_WAITING:
      Serial.print("PUMP_WAITING");
      break;
    }
    
    // Show WiFi status
    Serial.print(" | WiFi: ");
    Serial.println(wifiConnected && WiFi.status() == WL_CONNECTED ? "Connected" : "Offline");

    lastDisplayTime = currentTime;
  }

  // 2. State machine for pump control (unchanged)
  switch (currentState) {
  case MONITORING: {
    uint16_t moisture = analogRead(SENSOR_PIN);
    if (moisture >= DRY_THRESHOLD) {
      // Soil is dry (520 or higher) → start pumping
      digitalWrite(PUMP_CTRL_PIN, HIGH);
      currentState = PUMP_RUNNING;
      pumpStartTime = currentTime;
      Serial.println("PUMP: ON (moisture >= 520)");
    }
  }
  break;

  case PUMP_RUNNING:
    if (currentTime - pumpStartTime >= PUMP_RUN_TIME) {
      // Turn pump OFF after 1 second
      digitalWrite(PUMP_CTRL_PIN, LOW);
      currentState = PUMP_WAITING;
      lastPumpActionTime = currentTime;
      Serial.println("PUMP: OFF (1 second completed, waiting 1 minute)");
    }
    break;

  case PUMP_WAITING:
    if (currentTime - lastPumpActionTime >= PUMP_WAIT_TIME) {
      // 1 minute has passed, check moisture level
      uint16_t moisture = analogRead(SENSOR_PIN);
      Serial.print("1-minute check - Moisture: ");
      Serial.println(moisture);

      if (moisture <= WET_THRESHOLD) {
        // Soil is wet enough (420 or below) → stop pumping cycle
        currentState = MONITORING;
        Serial.println("TARGET REACHED: Moisture <= 420, returning to monitoring");
      } else {
        // Still too dry → pump again for 1 second
        digitalWrite(PUMP_CTRL_PIN, HIGH);
        currentState = PUMP_RUNNING;
        pumpStartTime = currentTime;
        Serial.println("PUMP: ON again (moisture still > 420)");
      }
    }
    break;
  }

  // 3. Data logging every 5 minutes (new feature)
  if (currentTime - lastDataLogTime >= LOG_INTERVAL) {
    SensorData data;
    data.timestamp = currentTime;
    data.moistureLevel = analogRead(SENSOR_PIN);
    data.pumpActive = digitalRead(PUMP_CTRL_PIN);
    
    switch (currentState) {
    case MONITORING:
      data.pumpState = "MONITORING";
      break;
    case PUMP_RUNNING:
      data.pumpState = "PUMP_RUNNING";
      break;
    case PUMP_WAITING:
      data.pumpState = "PUMP_WAITING";
      break;
    }
    
    logDataToExcel(data);
    lastDataLogTime = currentTime;
  }

  // 4. DUMMY DATA TESTING - Remove this block when not needed
  if (currentTime - lastDummyLogTime >= DUMMY_LOG_INTERVAL) {
    sendDummyDataToExcel();
    lastDummyLogTime = currentTime;
  }
  // 5. Handle provisioning if WiFi fails (non-blocking check)
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("⚠️ WiFi connection lost - attempting reconnection...");
    
    // Try to reconnect
    WiFi.reconnect();
    delay(5000); // Wait 5 seconds for reconnection
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.printf("✓ WiFi reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("✗ WiFi reconnection failed - irrigation continues normally");
    }
  }
}
