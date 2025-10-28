# Hardware Connection Guide
## Smart Irrigation System - ESP8266 NodeMCU

```
┌─────────────────────────────────────────────────────────────┐
│                     HARDWARE WIRING DIAGRAM                  │
└─────────────────────────────────────────────────────────────┘

╔═══════════════════════════════════════════════════════════╗
║                    NodeMCU ESP8266                         ║
║                                                            ║
║  [USB] ←── Power & Programming                            ║
║                                                            ║
║  3V3  ●────────────────────────┐                          ║
║                                 │                          ║
║  GND  ●──────┬──────┬──────────┼──────┬──────────┐       ║
║              │      │          │      │          │       ║
║  VIN  ●──────┼──────┼──────────┼──────┼────┐     │       ║
║              │      │          │      │    │     │       ║
║   D1  ●──────┼──────┼────┐     │      │    │     │       ║
║              │      │    │     │      │    │     │       ║
║   D2  ●──────┼──────┼────┼────┐│      │    │     │       ║
║              │      │    │     ││      │    │     │       ║
║   D3  ●──────┼──────┼────┼────┼┼────┐ │    │     │       ║
║              │      │    │     ││    │ │    │     │       ║
║   A0  ●──────┼──────┼────┼────┼┼────┼─┼────┼───┐ │       ║
║              │      │    │     ││    │ │    │   │ │       ║
╚══════════════╪══════╪════╪═════╪╪════╪═╪════╪═══╪═╪═══════╝
               │      │    │     ││    │ │    │   │ │
               │      │    │     ││    │ │    │   │ │
    ┌──────────┘      │    │     ││    │ │    │   │ │
    │                 │    │     ││    │ │    │   │ │
╔═══▼═════════════════▼════▼═════▼▼════▼═▼════▼═══▼═▼═══════╗
║              ULN2003 Darlington Driver                      ║
╠═══════════════════════════════════════════════════════════╣
║  IN1  (from D1)                                            ║
║  IN2-7  (not used)                                         ║
║  GND   (from NodeMCU GND)                                  ║
║  COM   (from NodeMCU VIN)  ←── Power for pump             ║
║  OUT1  ────────┐                                           ║
╚════════════════╪═══════════════════════════════════════════╝
                 │
                 │         ╔════════════════════╗
                 └────────→║   WATER PUMP       ║
                           ║   Pin 1: Signal    ║
                           ║   Pin 2: VIN       ║
                           ╚════════════════════╝

╔═══════════════════════════════════════════════════════════╗
║              Capacitive Moisture Sensor                    ║
╠═══════════════════════════════════════════════════════════╣
║  VCC  → NodeMCU 3V3                                        ║
║  GND  → NodeMCU GND                                        ║
║  AOUT → NodeMCU A0                                         ║
╚═══════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════╗
║                Manual Control Button                       ║
╠═══════════════════════════════════════════════════════════╣
║  Terminal 1 → NodeMCU D2                                   ║
║  Terminal 2 → NodeMCU GND                                  ║
║                                                            ║
║  Note: Internal pull-up enabled in software               ║
╚═══════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════╗
║                    Status LED                              ║
╠═══════════════════════════════════════════════════════════╣
║  Anode (+, long leg)  → NodeMCU D3                        ║
║  Cathode (-, short leg) → NodeMCU GND                     ║
║                                                            ║
║  Note: Built-in current limiting in ESP8266 GPIO          ║
║        If LED too bright, add 220Ω resistor in series     ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Pin Summary Table

| Component          | NodeMCU Pin | Wire Color (Suggested) | Notes                    |
|--------------------|-------------|------------------------|--------------------------|
| **Pump Control**   | D1          | Yellow                 | To ULN2003 IN1          |
| **Moisture Sensor**| A0          | White                  | Analog reading          |
| **Button**         | D2          | Blue                   | Pull-up enabled         |
| **Status LED**     | D3          | Green                  | Anode (long leg)        |
| **Sensor VCC**     | 3V3         | Red                    | 3.3V power              |
| **Common Ground**  | GND         | Black                  | Multiple connections    |
| **Pump Power**     | VIN         | Red (thick)            | 5V from USB             |

---

## Component Details

### 1. ULN2003 Darlington Driver Pinout
```
        ┌─────────┐
   IN1 →│1      16│→ OUT1 (to pump)
   IN2 →│2      15│
   IN3 →│3      14│
   IN4 →│4      13│
   IN5 →│5      12│
   IN6 →│6      11│
   IN7 →│7      10│
   GND ─│8       9│─ COM (VIN power)
        └─────────┘
```

### 2. Button Wiring Options
```
Option A: Simple (Used in Code)
┌──────┐
│  D2  ●───────┬─── Button Terminal 1
│      │       │
│ GND  ●───────┴─── Button Terminal 2
└──────┘
Internal pull-up: Button not pressed = HIGH, pressed = LOW

Option B: External Pull-up (if internal not reliable)
┌──────┐
│ 3V3  ●─── 10kΩ resistor ───┬─── Button Terminal 1
│      │                      │
│  D2  ●──────────────────────┤
│      │                      │
│ GND  ●──────────────────────┴─── Button Terminal 2
└──────┘
```

### 3. LED Polarity Identification
```
        LED
    ┌────────┐
    │  ╱╲    │
    │ ╱  ╲   │ ← Flat edge on cathode side
    │╱____╲  │
    │   ||   │
    │   ||   │
    └───┬┬───┘
        ││
    Long│  Short
  (Anode)  (Cathode)
     +       -
     D3     GND
```

---

## Testing Individual Components

### Test 1: LED
```cpp
void setup() {
  pinMode(D3, OUTPUT);
}
void loop() {
  digitalWrite(D3, HIGH);
  delay(1000);
  digitalWrite(D3, LOW);
  delay(1000);
}
```
**Expected:** LED blinks on/off every second

### Test 2: Button
```cpp
void setup() {
  Serial.begin(115200);
  pinMode(D2, INPUT_PULLUP);
}
void loop() {
  Serial.println(digitalRead(D2));
  delay(100);
}
```
**Expected:** Prints "1" normally, "0" when button pressed

### Test 3: Moisture Sensor
```cpp
void setup() {
  Serial.begin(115200);
}
void loop() {
  Serial.println(analogRead(A0));
  delay(500);
}
```
**Expected:** 
- In air: ~800-1024
- In water: ~200-400
- In moist soil: ~400-600
- In dry soil: ~600-800

### Test 4: Pump (CAREFUL!)
```cpp
void setup() {
  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);
}
void loop() {
  digitalWrite(D1, HIGH);  // ON
  delay(2000);
  digitalWrite(D1, LOW);   // OFF
  delay(10000);
}
```
**Expected:** Pump runs 2 seconds, off 10 seconds, repeat
**WARNING:** Have water reservoir connected!

---

## Power Requirements

| Component              | Voltage | Current | Source          |
|------------------------|---------|---------|-----------------|
| ESP8266 NodeMCU        | 3.3V    | ~80mA   | USB 5V (onboard regulator) |
| Moisture Sensor        | 3.3V    | ~5mA    | NodeMCU 3V3     |
| LED                    | 2-3V    | ~20mA   | NodeMCU D3      |
| Button                 | 3.3V    | <1mA    | NodeMCU D2      |
| Water Pump (typical)   | 3-6V    | 100-200mA | USB 5V (VIN)  |
| ULN2003 (driver IC)    | 5V      | <1mA    | NodeMCU VIN     |

**Total Current Draw:**
- Idle: ~85mA
- Pump Running: ~285mA
- **USB Port Limit:** 500mA (safe margin)

---

## Common Issues & Solutions

### Issue: Pump doesn't run
**Solutions:**
1. Check D1 connection to ULN2003 IN1
2. Verify ULN2003 GND connected to NodeMCU GND
3. Verify ULN2003 COM connected to NodeMCU VIN
4. Check pump polarity (try reversing if DC pump)
5. Test with multimeter: Should see ~5V on OUT1 when D1 HIGH

### Issue: LED always on/off
**Solutions:**
1. Check LED polarity (swap if backwards)
2. Verify anode (long leg) to D3
3. Verify cathode (short leg) to GND
4. Test with: `digitalWrite(D3, HIGH);` in setup()

### Issue: Button always pressed/never pressed
**Solutions:**
1. Check wiring: One side D2, other side GND
2. Try different button (could be faulty)
3. Test with multimeter: Should be open circuit normally, closed when pressed

### Issue: Moisture readings don't change
**Solutions:**
1. Verify sensor VCC to 3V3 (NOT 5V!)
2. Check AOUT to A0 connection
3. Check GND connection
4. Test in glass of water - should drop significantly

### Issue: ESP8266 resets when pump runs
**Solutions:**
1. Likely power issue - pump drawing too much current
2. Add capacitor (100µF-1000µF) across VIN and GND
3. Use separate 5V power supply for pump
4. Reduce pump runtime or use smaller pump

---

## Safety Checklist

- [ ] All connections secure (no loose wires)
- [ ] No exposed metal touching other components
- [ ] Moisture sensor fully waterproof (only prongs in soil/water)
- [ ] Water reservoir below electronics (prevent spills on board)
- [ ] USB power supply rated for 1A or higher
- [ ] No water near USB port or ESP8266
- [ ] Pump has water to pump (don't run dry)
- [ ] First test without water, verify pump spins

---

## Recommended Tools

- **Multimeter** - For voltage/continuity testing
- **Breadboard** - For prototyping before soldering
- **Jumper Wires** - Male-to-male, male-to-female
- **USB Cable** - Micro USB for NodeMCU power/programming
- **Screwdriver** - For ULN2003 terminal blocks
- **Wire Strippers** - If using solid core wire

---

## Final Assembly Tips

1. **Test each component individually first** (see test code above)
2. **Use breadboard for initial testing** before permanent connections
3. **Label all wires** with masking tape (component name + pin)
4. **Take photos** of working setup before making changes
5. **Keep moisture sensor away from electronics**
6. **Mount NodeMCU above water level**
7. **Use cable ties** to organize wires
8. **Add strain relief** on USB cable

---

**All connections verified against system design specification.**

Ready to build! 🛠️
