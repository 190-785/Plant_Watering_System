#include <Arduino.h>

constexpr uint8_t PUMP_CTRL_PIN = D1;   // ULN2003 IN1/O1
constexpr uint8_t SENSOR_PIN = A0;      // Moisture sensor output
constexpr uint16_t DRY_THRESHOLD = 520; // Start pumping when moisture reaches 520 (higher = drier)
constexpr uint16_t WET_THRESHOLD = 420; // Stop pumping when moisture reaches 420 or below

// Timing variables
unsigned long lastDisplayTime = 0;           // For 3-second display updates
unsigned long lastPumpActionTime = 0;        // For pump cycle timing
const unsigned long DISPLAY_INTERVAL = 3000; // 3 seconds in milliseconds
const unsigned long PUMP_RUN_TIME = 1000;    // 2 second pump run time
const unsigned long PUMP_WAIT_TIME = 60000; // 2 minutes wait time (120 seconds)

// Pump state variables
enum PumpState
{
  MONITORING,   // Waiting for moisture to reach 520
  PUMP_RUNNING, // Pump is currently running
  PUMP_WAITING  // Waiting 2 minutes before next check
};

PumpState currentState = MONITORING;
unsigned long pumpStartTime = 0;

void setup()
{
  Serial.begin(115200);
  pinMode(PUMP_CTRL_PIN, OUTPUT);
  digitalWrite(PUMP_CTRL_PIN, LOW); // pump OFF

  Serial.println("Smart Irrigation System Started");
  Serial.println("Dry threshold: 520, Wet threshold: 420");
  Serial.println("Display updates every 3 seconds");
}

void loop()
{
  unsigned long currentTime = millis();

  // 1. Display moisture reading every 3 seconds
  if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL)
  {
    uint16_t displayMoisture = analogRead(SENSOR_PIN);
    Serial.print("Moisture Level: ");
    Serial.print(displayMoisture);
    Serial.print(" | State: ");

    switch (currentState)
    {
    case MONITORING:
      Serial.println("MONITORING");
      break;
    case PUMP_RUNNING:
      Serial.println("PUMP_RUNNING");
      break;
    case PUMP_WAITING:
      Serial.println("PUMP_WAITING");
      break;
    }

    lastDisplayTime = currentTime;
  }

  // 2. State machine for pump control
  switch (currentState)
  {
  case MONITORING:
  {
    uint16_t moisture = analogRead(SENSOR_PIN);
    if (moisture >= DRY_THRESHOLD)
    {
      // Soil is dry (520 or higher) → start pumping
      digitalWrite(PUMP_CTRL_PIN, HIGH);
      currentState = PUMP_RUNNING;
      pumpStartTime = currentTime;
      Serial.println("PUMP: ON (moisture >= 520)");
    }
  }
  break;

  case PUMP_RUNNING:
    if (currentTime - pumpStartTime >= PUMP_RUN_TIME)
    {
      // Turn pump OFF after 1 second
      digitalWrite(PUMP_CTRL_PIN, LOW);
      currentState = PUMP_WAITING;
      lastPumpActionTime = currentTime;
      Serial.println("PUMP: OFF (1 second completed, waiting 2 minutes)");
    }
    break;

  case PUMP_WAITING:
    if (currentTime - lastPumpActionTime >= PUMP_WAIT_TIME)
    {
      // 2 minutes have passed, check moisture level
      uint16_t moisture = analogRead(SENSOR_PIN);
      Serial.print("2-minute check - Moisture: ");
      Serial.println(moisture);

      if (moisture <= WET_THRESHOLD)
      {
        // Soil is wet enough (420 or below) → stop pumping cycle
        currentState = MONITORING;
        Serial.println("TARGET REACHED: Moisture <= 420, returning to monitoring");
      }
      else
      {
        // Still too dry → pump again for 1 second
        digitalWrite(PUMP_CTRL_PIN, HIGH);
        currentState = PUMP_RUNNING;
        pumpStartTime = currentTime;
        Serial.println("PUMP: ON again (moisture still > 420)");
      }
    }
    break;
  }
}
