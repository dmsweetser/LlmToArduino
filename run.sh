#!/bin/bash

# -----------------------------
# Run script for LlmToArduino
# -----------------------------

# Function to setup Bluetooth virtual serial ports
setup_bluetooth_serial() {
    echo "Setting up Bluetooth virtual serial ports..."

    # Check if rfcomm exists
    if ! command -v rfcomm &> /dev/null; then
        echo "rfcomm command not found. Installing bluez-utils..."
        sudo apt-get install -y bluez-utils
    fi

    # Check if bluetoothctl exists
    if ! command -v bluetoothctl &> /dev/null; then
        echo "bluetoothctl command not found. Installing bluez-tools..."
        sudo apt-get install -y bluez-tools
    fi

    # Load configuration
    if [ ! -f "config/hardware_config.json" ]; then
        echo "Error: hardware_config.json not found. Please run install.sh first."
        exit 1
    fi

    # Parse configuration
    CAMERA_CHANNEL=$(jq -r '.bluetooth_devices.camera_board.channel' config/hardware_config.json)
    MAIN_CHANNEL=$(jq -r '.bluetooth_devices.main_board.channel' config/hardware_config.json)

    echo "Camera device will use /dev/ttyRFCOMM${CAMERA_CHANNEL}"
    echo "Main device will use /dev/ttyRFCOMM${MAIN_CHANNEL}"

    # Check if we're already connected
    if rfcomm show | grep -q "RFCOMM${CAMERA_CHANNEL}"; then
        echo "Bluetooth serial ports already configured."
        return 0
    fi

    # Power on Bluetooth
    echo "Powering on Bluetooth..."
    rfkill unblock bluetooth
    hciconfig hci0 up

    # Scan and connect to devices
    echo "Scanning for Bluetooth devices..."
    timeout 10 bluetoothctl scan on &
    sleep 10

    # Connect to camera device
    echo "Connecting to camera device..."
    bluetoothctl connect $(jq -r '.bluetooth_devices.camera_board.address' config/hardware_config.json) &
    sleep 5

    # Connect to main device
    echo "Connecting to main device..."
    bluetoothctl connect $(jq -r '.bluetooth_devices.main_board.address' config/hardware_config.json) &
    sleep 5

    # Bind RFCOMM channels
    echo "Binding RFCOMM channels..."
    rfcomm bind 0 $(jq -r '.bluetooth_devices.camera_board.address' config/hardware_config.json) ${CAMERA_CHANNEL} || {
        echo "Error: Failed to bind RFCOMM channel ${CAMERA_CHANNEL}"
        exit 1
    }
    rfcomm bind 1 $(jq -r '.bluetooth_devices.main_board.address' config/hardware_config.json) ${MAIN_CHANNEL} || {
        echo "Error: Failed to bind RFCOMM channel ${MAIN_CHANNEL}"
        exit 1
    }

    # Verify connections
    echo "Verifying connections..."
    if ! rfcomm show | grep -q "RFCOMM${CAMERA_CHANNEL}"; then
        echo "Error: Camera device connection failed"
        exit 1
    fi

    if ! rfcomm show | grep -q "RFCOMM${MAIN_CHANNEL}"; then
        echo "Error: Main device connection failed"
        exit 1
    fi

    echo "Bluetooth virtual serial ports setup complete."
}

# Activate virtual environment
if [ ! -d "venv" ]; then
    echo "Error: virtual environment not found. Run ./install.sh first."
    exit 1
fi

# Check if jq is installed
if ! command -v jq &> /dev/null; then
    echo "Installing jq for JSON parsing..."
    sudo apt-get install -y jq
fi

# Setup Bluetooth serial ports
setup_bluetooth_serial

# Activate virtual environment and run the app
source venv/bin/activate
python3 app.py

exit $?