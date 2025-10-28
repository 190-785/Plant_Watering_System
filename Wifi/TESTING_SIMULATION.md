# 🎮 Wokwi Simulation Testing Guide

## 📋 Overview
This guide explains how to test the Smart Irrigation System using **Wokwi Simulator** - a virtual hardware environment that runs your ESP8266 code without physical hardware.

---

## 🚀 Quick Start

### Option 1: VS Code with Wokwi Extension (Recommended)

1. **Install Wokwi Extension:**
   ```
   - Open VS Code Extensions (Ctrl+Shift+X)
   - Search for "Wokwi Simulator"
   - Install the extension
   ```

2. **Build Firmware:**
   ```bash
   pio run
   ```

3. **Start Simulation:**
   ```
   - Press F1
   - Type "Wokwi: Start Simulator"
   - Select diagram.json
   ```

4. **Interact with Virtual Hardware:**
   - Click button to test short/long/triple press
   - Adjust potentiometer to simulate moisture changes
   - Watch LED blink patterns
   - See relay activate for pump
   - Monitor serial output

---

### Option 2: Wokwi.com Online Simulator

1. **Upload Project:**
   - Go to https://wokwi.com/
   - Create new project (ESP8266)
   - Upload your diagram.json
   - Upload compiled firmware.bin

2. **Run Simulation:**
   - Click green "Start Simulation" button
   - Interact with components

---

## 🔧 Virtual Hardware Components

### Components in Simulation:

| Component | Pin | Purpose | Interaction |
|-----------|-----|---------|-------------|
| **ESP8266 DevKit** | - | Main controller | Auto-runs code |
| **Push Button** | D2 (GPIO4) | Control button | Click to press |
| **Blue LED** | D3 (GPIO0) | Status indicator | Observe patterns |
| **Relay Module** | D1 (GPIO5) | Pump control | Watch activation |
| **Potentiometer** | A0 | Moisture sensor | Slide to change value |
| **Resistor 330Ω** | - | LED current limit | - |
| **Resistor 10kΩ** | - | Button pull-up | - |

---

## 🧪 Simulation Test Scenarios

### Test 1: Button Pattern Detection

**Short Press:**
1. Click button quickly (< 1 second hold)
2. Watch serial: `[BUTTON] Short press detected`
3. LED should flash 3 times
4. Relay should activate for 2 seconds

**Long Press:**
1. Click and hold button for 5+ seconds
2. Watch serial: `[BUTTON] Long press detected`
3. Should clear fault if present

**Triple Press:**
1. Click button 3 times rapidly (within 2 seconds)
2. Watch serial: `[BUTTON] Triple press detected`
3. Should start WiFi portal

---

### Test 2: Moisture Threshold Testing

**Dry Soil (Auto Water Trigger):**
1. Slide potentiometer to left (low value < 520)
2. Wait for moisture reading cycle
3. Watch for auto pump activation
4. Serial: `[PUMP] Auto activation requested`

**Wet Soil (No Action):**
1. Slide potentiometer to right (high value > 750)
2. Pump should NOT activate
3. Serial: `[STATUS] Moisture: XXX | Pump: MONITORING`

**Mid-Range (Hysteresis Zone):**
1. Set potentiometer between 520-750
2. Test behavior based on previous state
3. Verify hysteresis logic

---

### Test 3: LED Pattern Observation

**All Patterns Visible:**
- Portal mode → Fast double-blink
- Connecting → Fast blink
- Online → Slow heartbeat
- Pumping → Solid on
- Fault → Slow error blink

**Simulation Advantage:**
- Can slow down time to see exact timing
- Pause to measure intervals
- No physical LED burnout risk

---

### Test 4: Pump Safety (No-Effect Detection)

**Simulate Ineffective Pump:**
1. Set potentiometer to dry (< 520)
2. Click button for manual water
3. **DO NOT move potentiometer** (simulate no water added)
4. Wait 12 seconds (pump run + settle time)
5. Serial should show: `⚠ Pump ineffective! No significant moisture change`

**Trigger Fault Lock:**
1. Repeat above test immediately (after 60s minInterval)
2. Second no-effect should trigger fault
3. Serial: `🚨 FAULT LOCKED`
4. LED pattern changes to error blink

**Verify Fault Persistence:**
1. Click button → pump should NOT activate
2. Serial: `[ERROR] Device in fault state`

---

### Test 5: WiFi Simulation (Limited)

**Note:** Wokwi has **limited WiFi simulation**. WiFi Manager portal won't work fully.

**What Works:**
- Code compiles and runs
- Serial output shows WiFi attempts
- State machine transitions
- Timeout handling

**What Doesn't Work:**
- Actual WiFi connection
- Firebase HTTP requests
- Web server access
- Portal configuration

**Workaround:**
- Test WiFi logic with serial monitoring
- Use offline mode testing
- Verify state transitions
- Check retry intervals in logs

---

## 🎯 Simulation Testing Checklist

### ✅ Tests That Work Great in Simulation:

- [x] **Button debouncing** - Click patterns work perfectly
- [x] **LED patterns** - All visible and measurable
- [x] **Moisture readings** - Potentiometer simulates sensor
- [x] **Pump activation** - Relay visible on/off
- [x] **Timing logic** - minInterval enforcement
- [x] **No-effect detection** - Can manually control moisture
- [x] **Fault locking** - State machine visible
- [x] **Persistent storage** - LittleFS works in simulation
- [x] **State machine** - All transitions testable
- [x] **Serial debugging** - Full console output

### ⚠️ Tests That Are Limited:

- [ ] **WiFi connection** - Can't connect to real network
- [ ] **Firebase API** - HTTP requests won't reach cloud
- [ ] **Web interface** - Can't access in browser
- [ ] **OTA updates** - Not supported
- [ ] **Real-time performance** - Simulation is slower

### ❌ Tests That Need Real Hardware:

- [ ] **Actual water pumping** - Physical relay only clicks
- [ ] **Real moisture sensor** - Needs soil/water
- [ ] **Power consumption** - Not measured
- [ ] **WiFi signal strength** - Virtual only
- [ ] **Long-term stability** - Simulation may crash

---

## 📊 Simulation Workflow Recommendation

### Phase 1: Simulation Testing (80% Coverage)
1. Test all button patterns ✅
2. Verify LED patterns ✅
3. Test pump safety logic ✅
4. Validate state machine ✅
5. Check persistence ✅
6. Debug serial output ✅

### Phase 2: Real Hardware Testing (100% Coverage)
1. Upload to physical ESP8266
2. Test WiFi portal configuration
3. Verify Firebase integration
4. Test with actual moisture sensor
5. Run pump with real water
6. Long-term stability test

---

## 🐛 Common Simulation Issues

### Issue 1: Simulation Won't Start
**Symptoms:** Error loading firmware
**Solution:**
```bash
# Rebuild firmware
pio run

# Check file exists
ls .pio/build/nodemcuv2/firmware.elf
```

### Issue 2: WiFi Features Don't Work
**Expected Behavior:** This is normal in simulation
**Workaround:** Focus on testing offline features

### Issue 3: Serial Monitor Shows Gibberish
**Symptoms:** Random characters
**Solution:** Set baud rate to 115200 in simulator settings

### Issue 4: LittleFS Not Persistent
**Symptoms:** Config lost on restart
**Note:** Some simulators don't persist flash between runs
**Workaround:** Test persistence on real hardware

---

## 📝 Automated Simulation Test Script

Create `test/test_simulation.cpp` for automated checks:

```cpp
#include <unity.h>

// Simulation-friendly unit tests
void test_button_debounce() {
    // Test button state tracking
    TEST_ASSERT_EQUAL(NONE, currentButtonAction);
}

void test_moisture_thresholds() {
    // Test threshold logic
    TEST_ASSERT_TRUE(520 < DRY_THRESHOLD);
    TEST_ASSERT_TRUE(750 > WET_THRESHOLD);
}

void test_pump_timing() {
    // Test timing constants
    TEST_ASSERT_EQUAL(2000, PUMP_RUN_TIME);
    TEST_ASSERT_EQUAL(60, MIN_INTERVAL_SEC);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_button_debounce);
    RUN_TEST(test_moisture_thresholds);
    RUN_TEST(test_pump_timing);
    UNITY_END();
}

void loop() {}
```

Run with:
```bash
pio test -e nodemcuv2
```

---

## 🎮 Interactive Simulation Controls

### Keyboard Shortcuts (If Supported):
- `B` - Simulate button press
- `M` - Toggle moisture (dry/wet)
- `R` - Reset device
- `P` - Pause simulation
- `S` - Step through code

### Mouse Controls:
- Click button component
- Drag potentiometer slider
- Right-click for component properties

---

## 📈 Simulation Testing Benefits

### Advantages:
✅ **Fast iteration** - No upload delays
✅ **No hardware damage** - Can't burn components
✅ **Repeatable tests** - Exact same conditions
✅ **Debug-friendly** - Pause and inspect
✅ **Free** - No hardware purchase needed
✅ **Shareable** - Share diagram.json with team

### Limitations:
❌ **Not 100% accurate** - Timing may differ
❌ **WiFi limited** - Can't test cloud features
❌ **Performance** - Slower than real hardware
❌ **Environmental factors** - No temperature, humidity, etc.

---

## 🚀 Next Steps After Simulation

1. **Simulation Testing** (You are here)
   - Verify core logic
   - Test button patterns
   - Debug LED patterns
   - Validate pump safety

2. **Upload to Hardware**
   - `pio run --target upload`
   - Test WiFi portal
   - Verify Firebase sync

3. **Field Testing**
   - Real soil/water
   - Long-term monitoring
   - Performance optimization

---

## 📚 Resources

- **Wokwi Docs:** https://docs.wokwi.com/
- **ESP8266 in Wokwi:** https://docs.wokwi.com/parts/wokwi-esp8266-devkit
- **PlatformIO + Wokwi:** https://docs.wokwi.com/vscode/project-config
- **Example Projects:** https://wokwi.com/projects

---

## ✅ Simulation Test Results Template

| Test | Status | Notes |
|------|--------|-------|
| Button Short Press | ⬜ | |
| Button Long Press | ⬜ | |
| Button Triple Press | ⬜ | |
| LED Portal Pattern | ⬜ | |
| LED Online Pattern | ⬜ | |
| LED Pumping Pattern | ⬜ | |
| LED Fault Pattern | ⬜ | |
| Moisture Reading | ⬜ | |
| Auto Water Trigger | ⬜ | |
| Manual Water | ⬜ | |
| minInterval Enforcement | ⬜ | |
| No-Effect Detection | ⬜ | |
| Fault Locking | ⬜ | |
| Fault Clear | ⬜ | |
| State Persistence | ⬜ | |

**Happy Simulating! 🎮🌱**
