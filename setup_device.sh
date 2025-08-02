#!/bin/bash

# Bloom Watch Device Setup Script
# This script helps with building and uploading the Firebase-enabled IoT device code

echo "=== Bloom Watch Device Setup ==="
echo ""

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    echo "❌ PlatformIO not found. Please install PlatformIO first:"
    echo "   pip install platformio"
    echo "   or visit: https://platformio.org/install"
    exit 1
fi

echo "✅ PlatformIO found"

# Navigate to Firebase project directory
cd "$(dirname "$0")/Firebase" || {
    echo "❌ Firebase project directory not found"
    exit 1
}

echo "📁 Working directory: $(pwd)"

# Check if platformio.ini exists
if [ ! -f "platformio.ini" ]; then
    echo "❌ platformio.ini not found in Firebase directory"
    exit 1
fi

echo "✅ PlatformIO project found"

# Build the project
echo ""
echo "🔨 Building project..."
if pio run; then
    echo "✅ Build successful"
else
    echo "❌ Build failed"
    exit 1
fi

# Check for connected devices
echo ""
echo "🔍 Checking for connected ESP8266 devices..."
DEVICES=$(pio device list | grep -E "(USB|COM|tty)" | head -5)

if [ -z "$DEVICES" ]; then
    echo "⚠️  No devices found. Make sure your ESP8266 is connected via USB."
    echo "   If device is connected, try:"
    echo "   - Different USB cable"
    echo "   - Different USB port"
    echo "   - Install ESP8266 drivers"
    echo ""
    echo "   You can manually upload with:"
    echo "   pio run --target upload --upload-port [YOUR_PORT]"
else
    echo "📱 Found devices:"
    echo "$DEVICES"
    echo ""
    
    # Ask user if they want to upload
    read -p "🚀 Upload firmware to device? (y/N): " -n 1 -r
    echo
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "📤 Uploading firmware..."
        if pio run --target upload; then
            echo "✅ Upload successful!"
            echo ""
            echo "🎉 Next steps:"
            echo "1. Open serial monitor: pio device monitor"
            echo "2. Look for device ID in the logs (format: BW-XXXXXXXX)"
            echo "3. Connect device to WiFi via hotspot 'BloomWatch-[ID]'"
            echo "4. Update Firebase API key in main.cpp if needed"
            echo "5. Use device ID in mobile app to register device"
            echo ""
            echo "📖 For detailed setup instructions, see:"
            echo "   FIREBASE_INTEGRATION_GUIDE.md"
        else
            echo "❌ Upload failed"
            exit 1
        fi
    else
        echo "ℹ️  Skipping upload. You can upload later with:"
        echo "   pio run --target upload"
    fi
fi

echo ""
echo "💡 Useful commands:"
echo "   Monitor serial output:  pio device monitor"
echo "   Build project:          pio run"
echo "   Upload firmware:        pio run --target upload"
echo "   Clean build:            pio run --target clean"
echo ""
echo "🔗 Documentation:"
echo "   Setup Guide: ../FIREBASE_INTEGRATION_GUIDE.md"
echo "   PlatformIO:  https://docs.platformio.org/"
echo ""
echo "✨ Happy coding!"
