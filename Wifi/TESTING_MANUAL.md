# 🧪 Smart Irrigation System - Manual Test Plan (Phase 1)

## 📋 Test Overview
This document provides a comprehensive manual testing checklist for all Phase 1 features. Each test includes:
- **Prerequisites** - What needs to be ready
- **Steps** - Exact procedure to follow
- **Expected Result** - What should happen
- **Pass/Fail Criteria** - How to verify success

---

## 🔧 Pre-Test Setup

### Hardware Checklist
- [ ] ESP8266 NodeMCU connected via USB
- [ ] Button connected to D2 (GPIO4) with pull-up resistor
- [ ] LED connected to D3 (GPIO0) with current-limiting resistor
- [ ] Pump connected to D1 (GPIO5) via ULN2003 driver
- [ ] Moisture sensor connected to A0
- [ ] Power supply adequate for all components

### Software Checklist
- [ ] Code uploaded successfully (no compilation errors)
- [ ] Serial monitor open at 115200 baud
- [ ] Firebase console open: https://console.firebase.google.com/project/bloom-watch-d6878/firestore
- [ ] Device ID noted from serial output (ESP8266_XXXXXXXXXXXX)

---

## ✅ TEST SUITE 1: INITIALIZATION & BOOT

### Test 1.1: First Boot (No Config)
**Prerequisites:** Flash empty filesystem (erase flash)
```bash
pio run --target erase
pio run --target upload
```

**Steps:**
1. Reset ESP8266
2. Observe serial output
3. Check LED pattern

**Expected Results:**
```
✓ LittleFS mounted successfully
Device ID: ESP8266_C82B9622AF07
ℹ No config file found - will create on first WiFi connection
ℹ No WiFi config - starting portal
*wm:StartAP with SSID: Irrigation-Setup
*wm:AP IP address: 192.168.4.1
```
- LED shows **fast double-blink** (100ms on, 100ms off, 100ms on, 700ms off)
- WiFi AP "Irrigation-Setup" visible in WiFi list

**Pass Criteria:**
- [ ] Portal AP broadcasts
- [ ] LED shows correct pattern
- [ ] Serial shows no errors

---

### Test 1.2: Boot with Saved Config
**Prerequisites:** WiFi already configured

**Steps:**
1. Reset ESP8266
2. Observe connection sequence

**Expected Results:**
```
✓ Configuration loaded
✓ Attempting WiFi connection...
✓ Connected! IP: 192.168.x.x
✓ Web server started on port 80
State: ONLINE
```
- LED shows **slow heartbeat** (100ms pulse every 3 seconds)
- Device connects to WiFi within 30 seconds

**Pass Criteria:**
- [ ] Connects to saved WiFi
- [ ] Gets IP address
- [ ] Shows ONLINE state
- [ ] LED heartbeat visible

---

## ✅ TEST SUITE 2: BUTTON CONTROLS

### Test 2.1: Short Press (Manual Water)
**Prerequisites:** Device ONLINE, no fault, >60 seconds since last pump

**Steps:**
1. Press and release button quickly (< 1 second)
2. Observe LED feedback
3. Watch for pump activation
4. Check serial output

**Expected Results:**
```
[BUTTON] Short press detected
[PUMP] Manual activation requested
[PUMP] Starting pump (Manual)
[STATUS] Moisture: XXX | Pump: PUMP_RUNNING | Device: ONLINE
```
- LED flashes 3 times quickly (button feedback)
- LED goes solid for 2 seconds (pumping)
- Pump relay activates for 2 seconds
- Returns to MONITORING state

**Pass Criteria:**
- [ ] Button feedback visible
- [ ] Pump runs for exactly 2 seconds
- [ ] Moisture reading taken before/after
- [ ] Logged to Firestore

---

### Test 2.2: Long Press (Clear Fault)
**Prerequisites:** Device in LOCKED_FAULT state

**Steps:**
1. Create fault condition (trigger no-effect detection twice)
2. Press and hold button for 5+ seconds
3. Release button
4. Observe state change

**Expected Results:**
```
[BUTTON] Long press detected
[FAULT] Clearing fault state
✓ Fault cleared via button
State: ONLINE
```
- LED changes from slow error blink to heartbeat
- Fault counter resets to 0
- Device returns to ONLINE state
- Pump state saved to flash

**Pass Criteria:**
- [ ] Fault clears after 5 seconds hold
- [ ] State changes to ONLINE
- [ ] LED pattern updates
- [ ] pump_state.json updated

---

### Test 2.3: Triple Press (WiFi Reset)
**Prerequisites:** Device running (any state)

**Steps:**
1. Press button 3 times within 2 seconds
2. Wait for portal to start
3. Check WiFi AP list

**Expected Results:**
```
[BUTTON] Triple press detected
[WiFi] Force configuration portal
*wm:StartAP with SSID: Irrigation-Setup
State: AWAITING_CONFIG
```
- LED switches to fast double-blink
- "Irrigation-Setup" AP appears
- Web portal accessible at 192.168.4.1
- Can reconfigure WiFi

**Pass Criteria:**
- [ ] Portal starts immediately
- [ ] AP broadcasts within 5 seconds
- [ ] Portal accessible
- [ ] Can save new config

---

## ✅ TEST SUITE 3: LED PATTERNS

### Test 3.1: All LED Patterns Visual Verification

| State | Pattern | Timing | How to Trigger |
|-------|---------|--------|----------------|
| **Portal Active** | Fast double-blink | 100ms on, 100ms off, 100ms on, 700ms off | Triple press button |
| **Connecting** | Fast single blink | 200ms on, 800ms off | Reboot with WiFi config, disconnect router |
| **Online** | Slow heartbeat | 100ms on every 3000ms | Normal operation when connected |
| **Offline** | Single blink | 500ms on every 3000ms | Disconnect WiFi router |
| **Pumping** | Solid on | Continuous | Trigger manual water or auto water |
| **Fault** | Slow error blink | 500ms on, 1500ms off | Trigger fault (2x no-effect) |
| **Button Feedback** | Three quick flashes | 50ms on/off x3 | Press any button |
| **Off** | No light | - | Not used in Phase 1 |

**Steps:**
1. For each state, trigger the condition
2. Use stopwatch/timer to verify exact timing
3. Record visual observation

**Pass Criteria:**
- [ ] All 8 patterns visually distinct
- [ ] Timing matches specification ±10ms
- [ ] Patterns persist correctly
- [ ] Transitions smooth

---

## ✅ TEST SUITE 4: PUMP SAFETY LOGIC

### Test 4.1: Minimum Interval Enforcement (AUTO)
**Prerequisites:** Device ONLINE, dry soil (moisture < 520)

**Steps:**
1. Allow auto-watering to trigger
2. Immediately trigger auto again (dry sensor reading)
3. Check serial output

**Expected Results:**
```
[PUMP] Auto activation requested
[SAFETY] minInterval not met: 30/60 seconds elapsed
```
- Pump does NOT activate second time
- Must wait full 60 seconds
- LED does not show pumping pattern

**Pass Criteria:**
- [ ] Second pump attempt blocked
- [ ] Serial shows safety message
- [ ] Wait 60s → then pump works

---

### Test 4.2: Minimum Interval Enforcement (MANUAL)
**Prerequisites:** Device ONLINE

**Steps:**
1. Short press button (manual water)
2. Wait for pump to finish
3. Immediately short press again
4. Check response

**Expected Results:**
```
[PUMP] Manual activation requested
[SAFETY] minInterval not met: 5/60 seconds elapsed
```
- Second manual request blocked
- Same 60-second rule applies
- Button feedback still shows

**Pass Criteria:**
- [ ] Manual also respects minInterval
- [ ] Safety message appears
- [ ] No pump activation

---

### Test 4.3: No-Effect Detection (First Occurrence)
**Prerequisites:** Device ONLINE, moisture sensor NOT in water

**Steps:**
1. Place sensor in dry air (not in soil)
2. Trigger manual water (short press)
3. Wait for pump to run and settle (12 seconds total)
4. Check serial output

**Expected Results:**
```
[PUMP] Starting pump (Manual)
[PUMP] Before: 100, After: 105, Delta: 5
⚠ Pump ineffective! No significant moisture change (5 < 30)
⚠ No-effect count: 1/2
```
- No-effect counter increments to 1
- Device stays in ONLINE state
- Pump state saved
- Still allows next watering attempt

**Pass Criteria:**
- [ ] Delta calculated correctly
- [ ] Counter = 1 (not locked yet)
- [ ] State remains ONLINE
- [ ] pump_state.json shows noEffectCounter: 1

---

### Test 4.4: Fault Locking (Second No-Effect)
**Prerequisites:** No-effect counter = 1

**Steps:**
1. Wait 60+ seconds (minInterval)
2. Sensor still dry (no water)
3. Trigger manual water again
4. Observe state change

**Expected Results:**
```
[PUMP] Before: 100, After: 103, Delta: 3
⚠ Pump ineffective! No significant moisture change (3 < 30)
⚠ No-effect count: 2/2
🚨 FAULT LOCKED - Too many ineffective pump cycles!
State: LOCKED_FAULT
```
- LED switches to slow error blink (500ms/1500ms)
- Device enters LOCKED_FAULT state
- Auto watering disabled
- Manual watering disabled
- Fault persists across reboots

**Pass Criteria:**
- [ ] Counter reaches 2
- [ ] State = LOCKED_FAULT
- [ ] LED error pattern
- [ ] Persists after reboot
- [ ] Pump blocked for all triggers

---

### Test 4.5: Fault Recovery
**Prerequisites:** Device in LOCKED_FAULT

**Steps:**
1. Long press button (5 seconds)
2. Observe state change
3. Reset device
4. Check state persists

**Expected Results:**
```
[BUTTON] Long press detected
[FAULT] Clearing fault state
✓ Fault cleared via button
State: ONLINE
```
- Counter resets to 0
- Device returns to ONLINE
- LED shows heartbeat
- pump_state.json updated

**Pass Criteria:**
- [ ] Fault clears successfully
- [ ] Counter = 0
- [ ] State = ONLINE after reboot
- [ ] Can pump again (respecting minInterval)

---

## ✅ TEST SUITE 5: WIFI & CONNECTIVITY

### Test 5.1: WiFi Portal Configuration
**Prerequisites:** Fresh device or triple-press to reset

**Steps:**
1. Connect phone/PC to "Irrigation-Setup" AP
2. Password: plant123456
3. Browser should auto-open to 192.168.4.1
4. Configure WiFi network
5. Click "Save"

**Expected Results:**
- Portal loads showing available networks
- Can select network and enter password
- After save, device reboots and connects
- config.json created in filesystem

**Pass Criteria:**
- [ ] Portal accessible
- [ ] Networks listed
- [ ] Can save configuration
- [ ] Device connects after save

---

### Test 5.2: Smart Retry - First Failure (1 hour)
**Prerequisites:** Device connected to WiFi

**Steps:**
1. Disconnect/turn off router
2. Observe retry behavior
3. Check serial output timing

**Expected Results:**
```
✗ WiFi connection lost
State: OFFLINE
Retry attempt: 1, Next retry in: 60 minutes
```
- Device switches to OFFLINE
- LED shows single blink pattern
- Retries connection after 1 hour
- Core functions continue locally

**Pass Criteria:**
- [ ] Detects disconnect within 30s
- [ ] State = OFFLINE
- [ ] Retry scheduled for 1 hour
- [ ] Sensor readings continue

---

### Test 5.3: Smart Retry - Second Failure (6 hours)
**Prerequisites:** First retry failed

**Steps:**
1. Keep router off
2. Wait for first retry (1 hour)
3. Let it fail again
4. Check next retry interval

**Expected Results:**
```
Retry attempt: 2, Next retry in: 360 minutes
```
- Retry interval increases to 6 hours
- Device stays in OFFLINE
- Local operation continues

**Pass Criteria:**
- [ ] Second retry at 1 hour mark
- [ ] Next retry = 6 hours
- [ ] No continuous retry spam

---

### Test 5.4: Smart Retry - Success Recovery
**Prerequisites:** Device in retry loop

**Steps:**
1. Turn router back on
2. Wait for next scheduled retry
3. Observe reconnection

**Expected Results:**
```
✓ WiFi reconnected!
State: ONLINE
Retry count reset to 0
```
- Reconnects successfully
- Retry counter resets
- Firebase sync resumes
- LED returns to heartbeat

**Pass Criteria:**
- [ ] Connects on retry
- [ ] State = ONLINE
- [ ] Retry count = 0
- [ ] Firestore logs resume

---

## ✅ TEST SUITE 6: FIRESTORE INTEGRATION

### Test 6.1: Log Entry Creation
**Prerequisites:** Device ONLINE, Firebase console open

**Steps:**
1. Navigate to: Firestore > plantData > ESP8266_{YOUR_MAC} > logs
2. Trigger manual water (short press)
3. Wait 2-3 seconds
4. Refresh Firestore console

**Expected Results:**
- New document appears in logs subcollection
- Document ID is timestamp
- Fields present:
  - `moisture` (integer)
  - `pumpStatus` (string: "PUMP_RUNNING")
  - `activationMethod` (string: "Manual")
  - `deviceState` (string: "ONLINE")

**Pass Criteria:**
- [ ] Document created within 3 seconds
- [ ] All fields present and correct types
- [ ] Moisture value matches serial output
- [ ] No HTTP errors in serial

---

### Test 6.2: Device Status Updates (Heartbeat)
**Prerequisites:** Device ONLINE

**Steps:**
1. Navigate to: Firestore > plantData > ESP8266_{YOUR_MAC}
2. Watch the main document (not logs subcollection)
3. Observe field updates every 10 seconds

**Expected Results:**
Main document fields:
- `currentMoisture` (integer, updates every 10s)
- `currentPumpStatus` (string: "MONITORING" / "PUMP_RUNNING" / "PUMP_WAITING")
- `lockedFault` (boolean: false / true)
- `lastSeen` (timestamp, updates every 10s)

**Pass Criteria:**
- [ ] Main document exists
- [ ] lastSeen updates every ~10s
- [ ] currentMoisture matches sensor
- [ ] lockedFault reflects device state

---

### Test 6.3: Config Sync (Remote Update)
**Prerequisites:** Device ONLINE

**Steps:**
1. In Firestore, navigate to: plantData/{deviceId}/config/settings
2. Create document if not exists
3. Set fields:
   ```
   dryThreshold: 400 (integer)
   wetThreshold: 700 (integer)
   pumpRunTime: 3000 (integer)
   minIntervalSec: 120 (integer)
   ```
4. Save document
5. Wait up to 30 seconds
6. Check serial output

**Expected Results:**
```
[CONFIG] Update detected from Firestore
  dryThreshold: 520 → 400
  wetThreshold: 750 → 700
  pumpRunTime: 2000 → 3000
  minIntervalSec: 60 → 120
✓ Configuration updated
```
- Device syncs new values within 30 seconds
- Values take effect immediately
- config.json updated in filesystem

**Pass Criteria:**
- [ ] Sync happens within 30s
- [ ] All values update correctly
- [ ] Next pump uses new pumpRunTime (3s)
- [ ] config.json shows new values

---

### Test 6.4: Remote Fault Clear
**Prerequisites:** Device in LOCKED_FAULT state

**Steps:**
1. In Firestore: plantData/{deviceId}/commands/pending
2. Create document with field:
   ```
   clearFault: true (boolean)
   ```
3. Wait up to 30 seconds
4. Check device state

**Expected Results:**
```
✓ Remote command: Clear Fault
[FAULT] Clearing fault state
State: ONLINE
```
- Fault cleared within 30s
- Command document deleted from Firestore
- Device returns to ONLINE
- LED pattern changes to heartbeat

**Pass Criteria:**
- [ ] Command processed within 30s
- [ ] Fault clears successfully
- [ ] Command doc auto-deleted
- [ ] State = ONLINE

---

## ✅ TEST SUITE 7: PERSISTENT STORAGE

### Test 7.1: Config Persistence Across Reboot
**Prerequisites:** Device configured with WiFi

**Steps:**
1. Note current config values from serial
2. Reset ESP8266 (power cycle or reset button)
3. Check if config reloaded

**Expected Results:**
```
✓ Configuration loaded
  SSID: YourNetwork
  Firebase Project: bloom-watch-d6878
  dryThreshold: 520
```
- WiFi credentials restored
- Firebase settings restored
- Thresholds restored
- Reconnects automatically

**Pass Criteria:**
- [ ] /config.json loads successfully
- [ ] WiFi reconnects without portal
- [ ] All settings match pre-reboot

---

### Test 7.2: Pump State Persistence
**Prerequisites:** Pump run at least once

**Steps:**
1. Trigger pump (manual or auto)
2. Note lastPumpEndEpoch, noEffectCounter, lockedFault
3. Reset ESP8266
4. Check serial output on boot

**Expected Results:**
```
✓ Pump state loaded
  Last Pump: XXX sec ago, Fault: NO, No-Effect Count: 0
```
- Last pump time persists
- Fault state persists
- No-effect counter persists
- Used for minInterval calculation

**Pass Criteria:**
- [ ] /pump_state.json loads
- [ ] Last pump time correct
- [ ] Fault state maintained
- [ ] Counter value correct

---

### Test 7.3: Fault State Survives Reboot
**Prerequisites:** Device in LOCKED_FAULT

**Steps:**
1. Trigger fault lock (2x no-effect)
2. Verify LED shows error pattern
3. Reset ESP8266
4. Check state after boot

**Expected Results:**
```
✓ Pump state loaded
  Last Pump: XXX sec ago, Fault: YES, No-Effect Count: 2
State: LOCKED_FAULT
```
- Device boots into LOCKED_FAULT
- LED shows error pattern immediately
- Pump remains disabled
- Must manually clear fault

**Pass Criteria:**
- [ ] Boots with fault active
- [ ] LED error pattern on boot
- [ ] Pump disabled
- [ ] Requires manual intervention

---

## ✅ TEST SUITE 8: WEB INTERFACE

### Test 8.1: Dashboard Access
**Prerequisites:** Device ONLINE, IP address known

**Steps:**
1. Open browser
2. Navigate to http://{device_ip}/
3. Observe dashboard

**Expected Results:**
- HTML dashboard loads
- Shows current status:
  - Device ID
  - State (ONLINE/OFFLINE/FAULT)
  - WiFi status
  - Current moisture reading
  - Pump status
  - Fault status
  - Last pump time

**Pass Criteria:**
- [ ] Dashboard loads < 2 seconds
- [ ] All status fields accurate
- [ ] Auto-refreshes (or manual refresh works)

---

### Test 8.2: Status JSON API
**Prerequisites:** Device ONLINE

**Steps:**
1. GET http://{device_ip}/status
2. Check response format

**Expected Results:**
```json
{
  "deviceId": "ESP8266_C82B9622AF07",
  "state": "ONLINE",
  "moisture": 350,
  "pumpState": "MONITORING",
  "lockedFault": false,
  "wifiConnected": true,
  "lastPumpEpoch": 1234567890,
  "noEffectCounter": 0
}
```

**Pass Criteria:**
- [ ] Returns valid JSON
- [ ] All fields present
- [ ] Values match device state
- [ ] Content-Type: application/json

---

### Test 8.3: Manual Water Endpoint
**Prerequisites:** Device ONLINE, >60s since last pump

**Steps:**
1. POST http://{device_ip}/water
2. Observe pump activation

**Expected Results:**
```
HTTP 200 OK
"Watering started"
```
- Pump activates immediately
- LED shows pumping pattern
- Same safety rules apply
- Logged to Firestore

**Pass Criteria:**
- [ ] Returns 200 status
- [ ] Pump runs for 2 seconds
- [ ] Respects minInterval
- [ ] Blocked if in fault

---

### Test 8.4: Clear Fault Endpoint
**Prerequisites:** Device in LOCKED_FAULT

**Steps:**
1. POST http://{device_ip}/clearFault
2. Check state change

**Expected Results:**
```
HTTP 200 OK
"Fault cleared"
```
- Fault clears immediately
- State changes to ONLINE
- LED pattern updates
- pump_state.json updated

**Pass Criteria:**
- [ ] Returns 200
- [ ] Fault clears
- [ ] Same as long-press button

---

### Test 8.5: Reset WiFi Endpoint
**Prerequisites:** Device ONLINE

**Steps:**
1. POST http://{device_ip}/resetWiFi
2. Observe portal start

**Expected Results:**
```
HTTP 200 OK
"Restarting to configuration portal"
```
- Device enters portal mode
- "Irrigation-Setup" AP appears
- Can reconfigure WiFi
- Same as triple-press button

**Pass Criteria:**
- [ ] Returns 200
- [ ] Portal starts immediately
- [ ] Can reconfigure

---

## 📊 TEST RESULTS SUMMARY SHEET

| Test Suite | Total Tests | Passed | Failed | Notes |
|------------|-------------|--------|--------|-------|
| 1. Initialization | 2 | | | |
| 2. Button Controls | 3 | | | |
| 3. LED Patterns | 8 | | | |
| 4. Pump Safety | 5 | | | |
| 5. WiFi Connectivity | 4 | | | |
| 6. Firestore Integration | 4 | | | |
| 7. Persistent Storage | 3 | | | |
| 8. Web Interface | 5 | | | |
| **TOTAL** | **34** | | | |

---

## 🐛 Issue Tracking Template

### Issue Report Format:
```
Test ID: [e.g., 6.1]
Test Name: [e.g., Log Entry Creation]
Status: FAILED
Date: [YYYY-MM-DD]

Expected Behavior:
[What should happen]

Actual Behavior:
[What actually happened]

Serial Output:
```
[Paste relevant serial output]
```

Steps to Reproduce:
1. [Step 1]
2. [Step 2]
3. [Step 3]

Additional Notes:
[Any other observations]
```

---

## ✅ CRITICAL PATH TESTS (Must Pass)

Before considering Phase 1 complete, these tests MUST pass:

1. **Test 2.1** - Manual water button works
2. **Test 4.1** - minInterval enforcement (safety!)
3. **Test 4.4** - Fault locking works (safety!)
4. **Test 5.1** - WiFi portal configuration
5. **Test 6.1** - Firestore logging works
6. **Test 7.2** - Pump state persists

---

## 📝 Testing Notes

- All tests designed for minimal hardware (can test without water)
- Use serial monitor for detailed diagnostics
- Firebase console for cloud verification
- Timing critical - use stopwatch for LED patterns
- Document all failures with serial output
- Test in order - some tests depend on previous state

**Good luck with testing! 🌱**
