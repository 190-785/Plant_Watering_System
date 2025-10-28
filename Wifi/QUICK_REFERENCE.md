# Quick Reference Card - Smart Irrigation System v3.0

## 🎮 Button Controls (D2)
```
┌─────────────────────────────────────────────┐
│  Triple Press  │ 3 clicks in 2 seconds      │ → WiFi Reset
│  Long Press    │ Hold 5+ seconds            │ → Clear Fault  
│  Short Press   │ Quick click                │ → Manual Water
└─────────────────────────────────────────────┘
```

## 💡 LED Patterns (D3)
```
Portal Active    ●●──●●──  Fast double-blink
Connecting       ●───●───  Fast blink
Online           ●────────  Slow heartbeat (every 3s)
Offline          ●────────  Slow blink (every 3s)
Pumping          ████████  Solid on
Fault            ●●────●●  Error blink
Button Feedback  ●●●       Three flashes
```

## 📍 Pin Mapping
```
D1 → Pump (via ULN2003)
D2 → Button (+ pull-up)
D3 → LED Anode
A0 → Moisture Sensor
```

## 🌐 Network
```
AP Name:  Irrigation-Setup
Password: plant123456
Web UI:   http://{device-ip}/
```

## 🔗 Web Endpoints
```
GET  /         → Dashboard
GET  /status   → JSON status
POST /water    → Manual water
POST /clearFault → Clear fault
POST /resetWiFi → Reset config
```

## 📊 Firestore Paths
```
plantData/{deviceId}/
  ├── (document)           → Live status
  ├── logs/{timestamp}     → Historical data
  ├── config/settings      → Configuration
  └── commands/pending     → Remote commands
```

## ⚙️ Default Config (Testing)
```
Dry Threshold:   520
Wet Threshold:   420
Pump Run Time:   2000 ms (2 sec)
Min Interval:    60 sec (1 min)
No-Effect Max:   2 failures
Settle Time:     10000 ms (10 sec)
```

## 🔄 State Transitions
```
AWAITING_CONFIG → ONLINE → OFFLINE ⟲
                    ↓
              LOCKED_FAULT (clearable)
```

## 🚨 Common Issues
```
No WiFi      → Triple-press button
Pump stuck   → Check ULN2003 wiring
Fault locked → Long-press 5 seconds
LED wrong    → Check polarity (long leg = +)
```

## 📝 Serial Monitor Commands
```
115200 baud
Look for:
  ✓ = Success
  ✗ = Error
  ⚠ = Warning
  [STATUS] = Regular update
  [BUTTON] = Button event
  [WiFi] = Network event
```

## 🎯 Quick Test Sequence
```
1. Power on
2. Watch LED (should show portal or connecting)
3. Press button once (LED should flash 3x)
4. Check serial for "BUTTON Short press"
5. Dry sensor (should auto-water)
6. Check Firebase console for logs
```

## 📱 Firebase Update Example
```javascript
// Update config
db.collection('plantData')
  .doc(deviceId)
  .collection('config')
  .doc('settings')
  .update({
    dryThreshold: 530,
    pumpRunTime: 5000
  });

// Clear fault
db.collection('plantData')
  .doc(deviceId)
  .collection('commands')
  .doc('pending')
  .set({
    clearFault: true
  });
```

## 🔐 Default Credentials
```
Firebase Project: bloom-watch-d6878
API Key: AIzaSyCt74... (in code)
Device ID: ESP8266_{MAC} (auto-generated)
```

## ⏱️ Timing Reference
```
Data Send:        Every 10 seconds
Config Check:     Every 30 seconds
Status Display:   Every 3 seconds
WiFi Check:       Every 5 seconds
Smart Retry:      1h → 6h → 24h
Portal Timeout:   5 minutes
```

## 💾 Files
```
/config.json       → WiFi & Firebase creds
/pump_state.json   → Pump history & faults
```

## 🛠️ Build Commands
```
pio run --target upload    → Flash device
pio device monitor         → View serial
pio run --target clean     → Clean build
```

## 📞 Emergency Recovery
```
1. Triple-press button → Force portal
2. Connect to "Irrigation-Setup"
3. Reconfigure WiFi
4. Or: Flash with USB cable
```

## ✅ Deployment Checklist
```
□ Hardware connected per diagram
□ Firebase credentials verified
□ Libraries installed
□ Code compiles
□ First boot shows portal
□ WiFi connects
□ LED shows correct pattern
□ Button presses register
□ Pump activates
□ Firebase logs appear
□ Web interface accessible
```

---

**Keep this card handy during testing!** 📋

Print or save for quick reference while working with your device.

Version: 3.0 Phase 1 | ESP8266 NodeMCU | Firebase Firestore
