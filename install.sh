#!/bin/bash

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="$SCRIPT_DIR/config"
SKETCH_DIR="$SCRIPT_DIR/sketch"
REQUIREMENTS_FILE="$SCRIPT_DIR/requirements.txt"
LLM_MODEL="Magistral-Small-2509-Q4_K_M.gguf"
LLM_URL="https://huggingface.co/unsloth/Magistral-Small-2509-GGUF/resolve/main/Magistral-Small-2509-Q4_K_M.gguf?download=true"
MMPROJ_MODEL="mmproj-F16.gguf"
MMPROJ_URL="https://huggingface.co/unsloth/Magistral-Small-2509-GGUF/resolve/main/mmproj-F16.gguf?download=true"
CONFIG_FILE="$CONFIG_DIR/hardware_config.json"
USERNAME=$(whoami)

# Error handling
error_exit() {
    echo "Error: $1" >&2
    exit 1
}

info() {
    echo "INFO: $1"
}

# Create directories if they don't exist
mkdir -p "$CONFIG_DIR"

# Function to download with resume capability
download_with_resume() {
    local url="$1"
    local output_file="$2"
    local temp_file="${output_file}.part"

    if [ -f "$output_file" ]; then
        info "File $output_file already exists, skipping download."
        return 0
    fi

    info "Downloading $url to $output_file..."
    local total_size=0
    local downloaded=0

    # Use curl with resume capability
    while true; do
        if [ -f "$temp_file" ]; then
            local resume_size=$(stat -c%s "$temp_file")
            info "Resuming download from byte $resume_size..."
            curl -fsSL -C - "$url" -o "$temp_file" || {
                rm -f "$temp_file"
                error_exit "Failed to resume download of $output_file."
            }
        else
            curl -fsSL "$url" -o "$temp_file" || {
                rm -f "$temp_file"
                error_exit "Failed to download $output_file."
            }
        fi

        # Check if download was complete
        local current_size=$(stat -c%s "$temp_file")
        if [ "$current_size" -gt 0 ] && [ "$current_size" -ne "$total_size" ]; then
            mv "$temp_file" "$output_file"
            info "Download completed successfully."
            return 0
        fi

        # If download wasn't complete, ask user if they want to retry
        read -p "Download incomplete. Retry? (y/n): " choice
        case "$(echo $choice | tr '[:upper:]' '[:lower:]')" in
            y|yes) continue ;;
            *) rm -f "$temp_file"; error_exit "Download cancelled." ;;
        esac
    done
}

# Function to list available Bluetooth devices with automatic scanning
list_bluetooth_devices() {
    local scan_attempt=0
    local max_scans=5

    while [ $scan_attempt -lt $max_scans ]; do
        info "Scanning for Bluetooth devices (attempt $((scan_attempt+1))...)"
        sudo systemctl restart bluetooth
        sleep 2

        info "Scanning for Bluetooth devices..."
        scan_output=$(echo -e "power on\nscan on" | bluetoothctl --timeout=10)
        sleep 5
        devices=$(echo -e "devices\nquit" | bluetoothctl | awk '/Device/ {print $2}')

        echo $devices

        if [ -z "$devices" ]; then
            info "No devices found. Please power on your devices and try again."
            read -p "Would you like to scan again? (y/n): " choice
            case "$(echo $choice | tr '[:upper:]' '[:lower:]')" in
                y|yes) ((scan_attempt++)) ;;
                *) error_exit "Bluetooth device scanning cancelled." ;;
            esac
        else
            echo "Available Bluetooth devices:"
            echo "0 - Rescan"
            echo "1 - None of the above"
            count=2
            while IFS= read -r line; do
                echo "$count - $line"
                ((count++))
            done <<< "$devices"
            return 0
        fi
    done

    error_exit "Maximum scan attempts reached. No Bluetooth devices found."
}

# Function to select and pair a Bluetooth device
select_bluetooth_device() {
    local device_type="$1"
    local devices_list="$2"
    local count=0

    while true; do
        echo "Select $device_type device:"
        count=0
        while IFS= read -r line; do
            echo "$count - $line"
            ((count++))
        done <<< "$devices_list"

        read -p "Enter the number of your choice: " choice
        case "$choice" in
            0) # Rescan
                devices_list=$(timeout 10 bluetoothctl devices | grep -v "Device" | grep -v "No" | awk '{print $2}' || echo "")
                ;;
            1) # None of the above
                return 1
                ;;
            *)
                if [ "$choice" -ge 0 ] && [ "$choice" -lt "$(echo "$devices_list" | grep -c .)" ]; then
                    local selected_device=$(echo "$devices_list" | sed -n "$((choice+1))p")
                    info "Attempting to pair with $selected_device..."
                    echo "pair $selected_device" | bluetoothctl
                    echo "trust $selected_device" | bluetoothctl
                    echo "connect $selected_device" | bluetoothctl
                    sleep 2
                    if bluetoothctl info "$selected_device" | grep -q "Connected: yes"; then
                        echo "$selected_device"
                        return 0
                    else
                        info "Failed to connect to $selected_device. Please check the device and try again."
                    fi
                else
                    info "Invalid selection. Please try again."
                fi
                ;;
        esac
    done
}

# Function to configure hardware ports
configure_hardware() {
    local config_content=$(cat <<EOF
{
    "bluetooth_devices": {
        "camera_board": {
            "address": "$1",
            "channel": 1,
            "description": "Camera module"
        },
        "main_board": {
            "address": "$2",
            "channel": 1,
            "description": "Main control board"
        }
    }
}
EOF
)

    echo "$config_content" > "$CONFIG_FILE"
    info "Hardware configuration saved to $CONFIG_FILE"
}

# Main installation function
main() {
    # Check if running as root
    if [ "$EUID" -eq 0 ]; then
        error_exit "Please do not run this script as root."
    fi

    # Update system
    info "Updating system packages..."
    sudo apt update -y
    sudo apt upgrade -y

    # Install dependencies
    info "Installing required system dependencies..."
    sudo apt install -y \
        portaudio19-dev \
        python3-dev \
        python3-pip \
        curl \
        espeak-ng \
        arduino \
        bluetooth \
        bluez \
        bluez-tools \
        libbluetooth-dev \
        rfkill \
        pulseaudio-module-bluetooth \
        python3.11-venv \
        python3-bluez

    mkdir -p ~/local/bin
    echo 'export PATH="$HOME/local/bin:$PATH"' >> ~/.bashrc
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/local/bin sh

    # Add user to dialout group
    info "Adding user to dialout group..."
    sudo usermod -aG dialout "$USERNAME"
    info "You may need to log out and back in for group changes to take effect."

    # Set up Arduino CLI
    info "Configuring Arduino CLI..."
    mkdir -p ~/.arduino15
    ~/local/bin/arduino-cli config init --overwrite
    ~/local/bin/arduino-cli config set network.connection_timeout 480s
    ~/local/bin/arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    ~/local/bin/arduino-cli core install arduino:avr
    ~/local/bin/arduino-cli core install esp32:esp32
    ~/local/bin/arduino-cli lib install LedControl
    ~/local/bin/arduino-cli lib install BluetoothSerial
    ~/local/bin/arduino-cli lib install ESP32Servo

    # Create virtual environment and install Python dependencies
    if [ ! -d "venv" ]; then
        info "Creating virtual environment..."
        python3 -m venv venv
    fi

    info "Activating virtual environment and installing Python packages..."
    source venv/bin/activate
    pip install --upgrade setuptools pip wheel
    pip install -r "$REQUIREMENTS_FILE"
    pip install git+https://github.com/pybluez/pybluez.git

    # Download LLM models with resume capability
    info "Downloading LLM models..."
    download_with_resume "$LLM_URL" "$LLM_MODEL"
    download_with_resume "$MMPROJ_URL" "$MMPROJ_MODEL"

    # Prompt the user and read their response into the 'response' variable
    read -p "Install Arduino scripts? (y/n): " response

    # Check the value of the 'response' variable using an if statement
    if [[ "$response" == "y" || "$response" == "Y" ]]; then

        # Hardware configuration
        info "Hardware Configuration Setup"

        info "Please connect the camera board now and press Enter..."
        read -p "" dummy

        # Get serial devices
        info "Available serial devices"
        serial_devices=($(ls -l /dev/tty* | grep -E "tty(ACM|USB|AMA)" | awk '{print $10}'))

        # List available serial devices
        for i in "${!serial_devices[@]}"; do
            echo "$((i+1)) - ${serial_devices[i]}"
        done

        # Prompt for camera board
        read -p "Enter the number of the camera board device: " camera_num
        camera_port="${serial_devices[$((camera_num-1))]}"
        echo "Camera port ${camera_port} selected"

        # Upload camera sketch
        info "Uploading camera sketch to $camera_port..."
        ~/local/bin/arduino-cli compile --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/camera"
        ~/local/bin/arduino-cli upload -p "$camera_port" --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/camera"

        # Disconnect camera board
        info "Please disconnect the camera board, connect the main board and press enter..."
        read -p "" dummy

        # Get serial devices
        info "Available serial devices"
        serial_devices=($(ls -l /dev/tty* | grep -E "tty(ACM|USB|AMA)" | awk '{print $10}'))

        # List available serial devices
        for i in "${!serial_devices[@]}"; do
            echo "$((i+1)) - ${serial_devices[i]}"
        done

        # Prompt for camera board
        read -p "Enter the number of the main board device: " main_num
        main_port="${serial_devices[$((main_num-1))]}"
        echo "Main port ${main_port} selected"

        # Upload main sketch
        info "Uploading main sketch to $main_port..."
        ~/local/bin/arduino-cli compile --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/main"
        ~/local/bin/arduino-cli upload -p "$main_port" --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/main"
    fi

    # Prompt the user and read their response into the 'response' variable
    read -p "Configure Bluetooth devices? (y/n): " response

    # Check the value of the 'response' variable using an if statement
    if [[ "$response" == "y" || "$response" == "Y" ]]; then
        # Power on the equipment
        info "Please power on the hardware now..."
        read -p "" dummy

        # Bluetooth configuration
        info "Bluetooth Configuration Setup"

        # Power on the camera module
        info "Please power on the camera module..."
        read -p "" dummy



        # Get Bluetooth devices
        devices_list=$(list_bluetooth_devices)

        # Select camera board
        bt1_addr=$(select_bluetooth_device "camera" "$devices_list")
        if [ -z "$bt1_addr" ]; then
            error_exit "Camera board selection cancelled."
        fi

        # Power on the main board
        info "Please power on the main board..."
        read -p "" dummy

        # Get Bluetooth devices again
        devices_list=$(list_bluetooth_devices)

        # Select main board
        bt2_addr=$(select_bluetooth_device "main" "$devices_list")
        if [ -z "$bt2_addr" ]; then
            error_exit "Main board selection cancelled."
        fi

        # Configure hardware
        configure_hardware "$bt1_addr" "$bt2_addr"

        # Connect to Bluetooth devices
        info "Connecting to Bluetooth devices..."
        sudo rfcomm bind /dev/rfcomm0 "$bt1_addr" 1
        sudo rfcomm bind /dev/rfcomm1 "$bt2_addr" 1

        info "Installation and configuration completed successfully."
        info "Configuration saved to $CONFIG_FILE"
    fi
}

# Run main function
main