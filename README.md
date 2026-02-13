# Plant Watering System 🌱💧

An automated irrigation system using ESP8266 microcontrollers with soil moisture monitoring and intelligent watering control. Available in two versions: Basic IO and WiFi-enabled.

> **Part of the BloomWatch ecosystem.** This firmware repo is one half of a two-repo project. The ESP8266 WiFi module collects soil moisture data and pushes it to Firebase Firestore, where the companion **[BloomWatch Web App](https://github.com/190-785/bloomwatch-webapp)** picks it up to display real-time dashboards, historical charts, and remote watering controls.

## Overview

This project consists of two main components with increasing levels of functionality:

### 1. IO Module (Basic Version)
A standalone irrigation controller that:
- Monitors soil moisture levels using an analog sensor
- Automatically controls a water pump based on moisture thresholds
- Provides real-time serial monitoring of system status
- Uses simple threshold-based logic for pump control

### 2. WiFi Module (Advanced Version)
An enhanced version with networking and cloud capabilities that includes:
- All basic irrigation functionality from the IO module
- WiFi connectivity with web-based configuration portal
- Firebase/Firestore integration for cloud data synchronization
- Real-time data synchronization to the cloud
- Remote configuration and control via Firebase
- Historical data logging and analytics
- Device-specific data paths using MAC address
- Web dashboard integration capabilities
- Data logging and remote monitoring capabilities
- Web server for system status and configuration
- JSON-based configuration management
- Button controls and LED status indicators
- Pump safety features and fault detection

## Hardware Requirements

### Core Components
- **Microcontroller**: ESP8266 (NodeMCU v2/v3)
- **Moisture Sensor**: Analog soil moisture sensor
- **Water Pump**: Small DC water pump (5V/12V)
- **Pump Driver**: ULN2003 or similar relay/driver circuit
- **Power Supply**: Appropriate for your pump voltage

### WiFi Version Additions
- **Push Button**: Momentary push button for manual controls
- **Status LED**: Single LED for status indication
- **WiFi Network**: 2.4GHz WiFi network for connectivity

### Wiring
- **Pump Control**: GPIO D1 (Pin 5) → ULN2003 IN1
- **Moisture Sensor**: A0 (Analog pin) → Sensor output
- **Button** (WiFi/Firebase): GPIO D2 (Pin 4) → Button + pull-up resistor
- **LED** (WiFi/Firebase): GPIO D3 (Pin 0) → LED anode
- **Power**: 3.3V/5V and GND connections as required

## Project Structure

```
Plant_Watering_System/
├── IO/                     # Basic irrigation system
│   ├── platformio.ini     # PlatformIO configuration
│   ├── src/
│   │   └── main.cpp       # Main irrigation logic
│   ├── include/           # Header files
│   └── lib/               # Local libraries
│
├── Wifi/                   # WiFi-enabled version
│   ├── platformio.ini     # PlatformIO configuration with WiFi libs
│   ├── src/
│   │   ├── main.cpp       # Main application with WiFi
│   │   ├── Config.cpp/h   # Configuration management
│   │   ├── ProvisionServer.cpp/h  # WiFi provisioning
│   │   └── PortalLogin.cpp/h      # Web portal authentication
│   ├── include/           # Header files
│   ├── lib/               # Local libraries
│   ├── HARDWARE_GUIDE.md  # Detailed hardware setup
│   ├── QUICK_REFERENCE.md # Quick reference card
│   ├── TESTING_MANUAL.md  # Manual testing procedures
│   └── TESTING_SIMULATION.md # Simulation testing guide
│
├── .gitignore             # Git ignore rules
├── LICENSE                # Project license
└── README.md              # This file
```

## Getting Started

### Prerequisites
- [PlatformIO IDE](https://platformio.org/platformio-ide) or [PlatformIO Core](https://platformio.org/install/cli)
- ESP8266 development board (NodeMCU recommended)
- Basic electronics components listed above

### Installation

1. **Clone the repository**:
   ```bash
   git clone https://github.com/190-785/Plant_Watering_System.git
   cd Plant_Watering_System
   ```

2. **Choose your version**:
   - For basic functionality: Navigate to `IO/` directory
   - For WiFi features: Navigate to `Wifi/` directory

3. **Build and upload**:
   ```bash
   # For basic version
   cd IO
   pio run --target upload

   # For WiFi version
   cd Wifi
   pio run --target upload
   ```

4. **Monitor serial output**:
   ```bash
   pio device monitor
   ```

### Configuration

#### Basic Version (IO)
- Power on the device near your plant
- Insert the moisture sensor into the soil
- Position the water pump in your water reservoir
- The system will automatically maintain optimal soil moisture

#### WiFi Version
1. **Initial Setup**: On first boot, the device creates a WiFi access point
2. **Connect**: Join the AP and navigate to the configuration portal
3. **Configure WiFi**: Set WiFi credentials and system parameters
4. **Firebase Setup** (Optional): Create a Firebase project and enable Firestore
5. **Configure Firebase** (Optional): Update `firebaseProjectId`, `firebaseApiKey`, and `firebaseDatabaseURL` in the code
6. **Deploy**: The system will connect to your network and begin operation, with optional cloud sync

## Features by Version

| Feature | IO | WiFi |
|---------|----|------|
| Moisture monitoring | ✅ | ✅ |
| Automatic watering | ✅ | ✅ |
| Serial monitoring | ✅ | ✅ |
| Web interface | ❌ | ✅ |
| WiFi connectivity | ❌ | ✅ |
| Button controls | ❌ | ✅ |
| LED status | ❌ | ✅ |
| Pump safety | ❌ | ✅ |
| Data logging | ❌ | ✅ |
| Remote monitoring | ❌ | ✅ |
| Cloud sync | ❌ | ✅ |
| Historical data | ❌ | ✅ |

## Thresholds and Timing

### Moisture Control Logic
- **Dry Threshold**: 480-520 (triggers pump activation)
- **Wet Threshold**: 420-440 (stops pump operation)
- **Pump Runtime**: 1-2 seconds per cycle
- **Wait Period**: 1-2 minutes between pump cycles

### WiFi Version Features
- **Button Controls**: Triple-press (WiFi reset), Long-press (clear fault), Short-press (manual water)
- **LED Patterns**: 8 different status indicators
- **Web Endpoints**: Dashboard, status JSON, manual controls
- **Safety Features**: Pump interval enforcement, fault detection
- **Firebase Integration**: Real-time cloud synchronization, historical data logging
- **Device ID**: Auto-generated from MAC address
- **Data Paths**: `plantData/{deviceId}/` structure
- **Collections**: Live status, historical logs, remote config, commands
- **Sync Intervals**: Status every 10s, config check every 30s

## Documentation

### WiFi Version Documentation
- **[HARDWARE_GUIDE.md](Wifi/HARDWARE_GUIDE.md)** - Complete hardware setup and wiring
- **[QUICK_REFERENCE.md](Wifi/QUICK_REFERENCE.md)** - Quick reference card for controls and commands
- **[TESTING_MANUAL.md](Wifi/TESTING_MANUAL.md)** - Manual testing procedures and checklists
- **[TESTING_SIMULATION.md](Wifi/TESTING_SIMULATION.md)** - Simulation testing guide

## Customization

### Adjusting Thresholds
Modify the constants in `main.cpp`:
```cpp
constexpr uint16_t DRY_THRESHOLD = 520;  // Your dry value
constexpr uint16_t WET_THRESHOLD = 420;  // Your wet value
```

### Timing Adjustments
```cpp
const unsigned long PUMP_RUN_TIME = 1000;    // Pump duration (ms)
const unsigned long PUMP_WAIT_TIME = 60000;  // Wait between cycles (ms)
```

### WiFi Version Configuration
- **Portal Timeout**: 5 minutes for configuration
- **WiFi Retry**: Smart exponential backoff (1h → 6h → 24h)
- **Data Logging**: Every 30 seconds when online

### Firebase Configuration (WiFi Version)
Update these values in the WiFi version for cloud features:
```cpp
String firebaseProjectId = "your-project-id";
String firebaseApiKey = "your-api-key";
String firebaseDatabaseURL = "your-database-url";
```

## Troubleshooting

### Common Issues
- **Pump not activating**: Check wiring and power supply
- **Erratic readings**: Ensure sensor is properly inserted in soil
- **WiFi connection fails**: Verify credentials and signal strength
- **Continuous pumping**: Check sensor calibration and thresholds
- **Firebase sync fails**: Verify project credentials and permissions

### Version-Specific Issues

#### WiFi Version
- **Portal not appearing**: Triple-press button to force portal mode
- **LED not responding**: Check polarity (long leg to D3, short leg to GND)
- **Button not working**: Verify D2 connection and pull-up resistor
- **Firebase sync fails**: Check Firebase credentials and network connectivity
- **Device not appearing in Firebase**: Verify Firestore security rules
- **Firebase commands not working**: Check device ID format and collection paths

### Debugging
- Enable serial monitoring for detailed system status
- Check sensor readings manually with multimeter
- Verify pump operation with direct power connection
- Use the testing guides in the WiFi documentation

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the terms specified in the LICENSE file.

## Acknowledgments

- Built using PlatformIO and Arduino framework
- Designed for ESP8266 microcontrollers
- Inspired by the need for automated plant care solutions
- Firebase integration using Firebase Arduino Client Library

## Companion Web App

**Repository:** [github.com/190-785/bloomwatch-webapp](https://github.com/190-785/bloomwatch-webapp)

The WiFi version of this firmware writes sensor data and pump status to Firebase Firestore under `plantData/{deviceId}`. The BloomWatch web app reads that same Firestore path to provide:

- **Real-time dashboard** — live moisture levels, pump status, connection state
- **Historical charts** — moisture trends over 24h / 7d / 30d
- **Remote control** — water now, clear faults, adjust thresholds from anywhere
- **Multi-device management** — monitor multiple plants from one interface
- **User authentication** — secure, role-based access

### How they connect

```
ESP8266 Firmware (this repo)        BloomWatch Web App
─────────────────────────────       ──────────────────────
Reads soil moisture sensor    ──►   Displays live readings
Controls water pump           ◄──   Sends waterNow / clearFault commands
Pushes logs to Firestore      ──►   Renders historical charts
Pulls config from Firestore   ◄──   Lets users adjust thresholds
```

Both repos share the same Firebase project (`bloom-watch-d6878`) and Firestore data structure. See the [BloomWatch README](https://github.com/190-785/bloomwatch-webapp#readme) for web app setup.

---

**Happy Growing! 🌱**

*Choose your version based on your needs:*
- **IO**: Simple, reliable, offline operation
- **WiFi**: Network connectivity, remote monitoring, and optional cloud features