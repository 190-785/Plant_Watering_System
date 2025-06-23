# Plant Watering System 🌱💧

An automated irrigation system using ESP8266 microcontrollers with soil moisture monitoring and intelligent watering control.

## Overview

This project consists of two main components:

### 1. IO Module (Basic Version)
A standalone irrigation controller that:
- Monitors soil moisture levels using an analog sensor
- Automatically controls a water pump based on moisture thresholds
- Provides real-time serial monitoring of system status
- Uses simple threshold-based logic for pump control

### 2. WiFi Module (Advanced Version)
An enhanced version with networking capabilities that includes:
- All basic irrigation functionality from the IO module
- WiFi connectivity with web-based configuration portal
- Data logging and remote monitoring capabilities
- Web server for system status and configuration
- JSON-based configuration management

## Hardware Requirements

### Core Components
- **Microcontroller**: ESP8266 (NodeMCU v2/v3)
- **Moisture Sensor**: Analog soil moisture sensor
- **Water Pump**: Small DC water pump (5V/12V)
- **Pump Driver**: ULN2003 or similar relay/driver circuit
- **Power Supply**: Appropriate for your pump voltage

### Wiring
- **Pump Control**: GPIO D1 (Pin 5) → ULN2003 IN1
- **Moisture Sensor**: A0 (Analog pin) → Sensor output
- **Power**: 3.3V/5V and GND connections as required

## Software Features

### Moisture Control Logic
- **Dry Threshold**: 520 (triggers pump activation)
- **Wet Threshold**: 420 (stops pump operation)
- **Pump Runtime**: 1-2 seconds per cycle
- **Wait Period**: 1-2 minutes between pump cycles

### WiFi Features (WiFi Module Only)
- **Web Portal**: Configuration and monitoring interface
- **Data Logging**: Periodic sensor data recording
- **Remote Access**: Monitor system status remotely
- **Flexible Configuration**: JSON-based settings management

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
│   └── lib/               # Local libraries
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
   git clone <https://github.com/190-785/Plant_Watering_System>
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

### Configuration (WiFi Version)

1. **Initial Setup**: On first boot, the device creates a WiFi access point
2. **Connect**: Join the AP and navigate to the configuration portal
3. **Configure**: Set WiFi credentials and system parameters
4. **Deploy**: The system will connect to your network and begin operation

## Usage

### Basic Operation
- Power on the device near your plant
- Insert the moisture sensor into the soil
- Position the water pump in your water reservoir
- The system will automatically maintain optimal soil moisture

### Monitoring
- **Serial Monitor**: Real-time status updates via USB serial
- **Web Interface** (WiFi version): Access via device IP address
- **Status Indicators**: LED feedback and serial logging

## Thresholds and Timing

| Parameter | Value | Description |
|-----------|-------|-------------|
| Dry Threshold | 520 | Moisture level that triggers watering |
| Wet Threshold | 420 | Moisture level that stops watering |
| Pump Runtime | 1-2 seconds | Duration of each watering cycle |
| Wait Period | 1-2 minutes | Delay between pump cycles |
| Display Update | 3 seconds | Serial monitor update frequency |

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

## Troubleshooting

### Common Issues
- **Pump not activating**: Check wiring and power supply
- **Erratic readings**: Ensure sensor is properly inserted in soil
- **WiFi connection fails**: Verify credentials and signal strength
- **Continuous pumping**: Check sensor calibration and thresholds

### Debugging
- Enable serial monitoring for detailed system status
- Check sensor readings manually with multimeter
- Verify pump operation with direct power connection

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

---

**Happy Growing! 🌱**