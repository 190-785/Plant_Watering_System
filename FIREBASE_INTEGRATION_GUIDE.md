# IoT Device Firebase Firestore Integration Setup Guide

This guid5. Device will restart and connect to your WiFi

## Step 5: Get Device IDill help you connect your ESP8266-based plant watering system to the Bloom Watch mobile app using Firebase Firestore.

## Prerequisites

1. **Hardware Setup**: ESP8266 NodeMCU with moisture sensor and water pump
2. **Development Environment**: PlatformIO IDE or Arduino IDE with ESP8266 board support
3. **Firebase Project**: Bloom Watch Firebase project with Firestore enabled
4. **Mobile App**: Bloom Watch app installed and user account created

## Step 1: Firebase Firestore Setup

1. Go to Firebase Console → Firestore Database
2. Create a Firestore database (start in test mode for development)
3. Update the security rules:

```javascript
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    match /devices/{deviceId} {
      allow read, write: if true;
    }
    match /users/{userId} {
      allow read, write: if request.auth != null && request.auth.uid == userId;
      match /devices/{deviceId} {
        allow read, write: if request.auth != null && request.auth.uid == userId;
        match /sensorHistory/{historyId} {
          allow read, write: if request.auth != null && request.auth.uid == userId;
        }
      }
    }
  }
}
```

4. Get your Web API Key from Project Settings → General → Web API Key

## Step 2: Configure Device Code

1. Open the `Plant_Watering_System/Firebase/src/main.cpp` file
2. Update the Firebase configuration:
   ```cpp
   #define FIREBASE_PROJECT_ID "bloom-watch-d6878"
   #define FIREBASE_API_KEY "your-web-api-key-here"
   ```
3. Replace `"your-web-api-key-here"` with your actual Web API Key from Firebase Console

## Step 3: Upload IoT Device Code

1. Open the `Plant_Watering_System/Firebase/` project in PlatformIO
2. Install required libraries (they will be automatically installed based on platformio.ini)
3. Upload the code to your ESP8266:
   ```bash
   cd Plant_Watering_System/Firebase/
   pio run --target upload
   ```

## Step 4: Configure WiFi on Device

1. After uploading, the device will create a WiFi hotspot
2. Connect to WiFi network: `BloomWatch-[CHIP_ID]`
3. Password: `bloom123456`
4. Open browser and go to: `http://192.168.4.1`
5. Configure your home WiFi credentials
6. Device will restart and connect to your WiFi

## Step 4: Get Device ID

The device ID is automatically generated based on the ESP8266 chip ID in format: `BW-[HEX_CHIP_ID]`

To find your device ID:

### Method 1: Serial Monitor
1. Open PlatformIO Serial Monitor
2. Look for the device ID in the startup logs:
   ```
   === Bloom Watch Plant Watering System ===
   Device ID: BW-A1B2C3D4
   Version: 1.0.0
   ```

### Method 2: Web Interface
1. Find the device's IP address from your router or serial monitor
2. Open browser and go to: `http://[DEVICE_IP]/`
3. The device ID will be displayed on the web page

### Method 3: WiFi Network Name
The device ID is also shown in the WiFi hotspot name: `BloomWatch-[DEVICE_ID]`

## Step 6: Register Device in Mobile App

1. Open Bloom Watch mobile app
2. Go to "Add Device" screen
3. Enter the Device ID (e.g., `BW-A1B2C3D4`)
4. Tap "Search Device" to verify the device exists
5. If found, fill in the device details:
   - **Device Name**: Give your device a friendly name
   - **Location**: Where the device is located
   - **Plant Type**: Type of plant being monitored
   - **Watering Thresholds**: Adjust moisture levels for watering
6. Tap "Register Device"

## Step 7: Verify Connection

After registration, you should see:

1. **In the app**: Device appears in your device list with real-time data
2. **Device serial monitor**: Shows "Device registered to user: [USER_ID]"
3. **Device status LED**: Indicates connection status
   - **Solid ON**: Device online and registered
   - **Blinking**: Connecting to WiFi/Firestore
   - **OFF**: Device offline

## Device Features

### Automatic Plant Watering
- Monitors soil moisture continuously
- Automatically waters when soil is too dry
- Stops watering when optimal moisture reached
- Configurable thresholds via mobile app

### Real-time Monitoring
- Live moisture readings
- Pump status updates
- Connection status
- Device health monitoring

### Remote Configuration
- Adjust watering thresholds from app
- Change pump run times
- Monitor device status remotely

## Troubleshooting

### Device Not Found
1. Ensure device is powered on and connected to WiFi
2. Check WiFi connection on device (LED status)
3. Verify device ID is correct (case-sensitive)
4. Try restarting the device

### Device Already Registered
- Each device can only be registered to one user
- To transfer ownership, the current owner must remove it first
- Contact support if device ownership needs to be transferred

### Connection Issues
1. Check WiFi credentials
2. Verify Firebase project URL
3. Ensure device has internet access
4. Check device logs via serial monitor

### Firestore Connection Errors
1. Verify Firestore is enabled in Firebase console
2. Check API key is correct in device code
3. Ensure device has stable internet connection
4. Check device logs via serial monitor

## Device Web Interface

Each device provides a web interface accessible at its IP address:

- **Home Page**: Device status and information
- **`/status`**: JSON status data
- **`/reset`**: Reset device (use carefully)

## Technical Specifications

- **Microcontroller**: ESP8266 NodeMCU
- **Moisture Sensor**: Analog soil moisture sensor
- **Pump Control**: Relay module for water pump
- **Power**: 5V USB or external power supply
- **Connectivity**: WiFi 802.11 b/g/n
- **Database**: Firebase Firestore

## Security Notes

- Device uses test mode for Firebase authentication
- For production use, implement proper Firebase Authentication
- Keep Firebase project credentials secure
- Regularly update device firmware

## Support

For technical support or issues:
1. Check device serial monitor logs
2. Verify Firebase console for device data
3. Test device web interface
4. Contact app support team

## Next Steps

Once your device is connected:
1. Monitor plant watering in the app dashboard
2. Adjust thresholds based on plant needs
3. Set up notifications for device status
4. Add multiple devices for different plants

Happy growing! 🌱
