#ifndef CONFIG_H
#define CONFIG_H

// ===== FIREBASE CONFIGURATION ===== //
// Update these values according to your Firebase project
#define FIREBASE_PROJECT_ID "bloom-watch-d6878"
#define FIREBASE_API_KEY "AIzaSyCt74gYV9dmCm84lBK6RFBP4z7gLOrjjdo"

// For production, you should use Firebase Authentication
// For now, we'll use Firestore with public rules for simplicity
// Make sure to update your Firestore Security Rules to:
/*
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    match /devices/{deviceId} {
      allow read, write: if true;
    }
    match /users/{userId} {
      allow read, write: if request.auth != null && request.auth.uid == userId;
    }
  }
}
*/

// ===== DEVICE CONFIGURATION ===== //
#define DEVICE_VERSION "1.0.0"
#define DEVICE_TYPE "plant-watering-system"

// ===== HARDWARE CONFIGURATION ===== //
#define PUMP_CTRL_PIN D1
#define SENSOR_PIN A0
#define STATUS_LED_PIN D4

// ===== DEFAULT THRESHOLDS ===== //
#define DEFAULT_DRY_THRESHOLD 520
#define DEFAULT_WET_THRESHOLD 420
#define DEFAULT_PUMP_RUN_TIME 2000

// ===== TIMING CONFIGURATION ===== //
#define DISPLAY_INTERVAL 3000
#define FIREBASE_UPDATE_INTERVAL 5000
#define CONFIG_SYNC_INTERVAL 30000
#define HEARTBEAT_INTERVAL 60000
#define PUMP_WAIT_TIME 60000

#endif // CONFIG_H
