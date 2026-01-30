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

# Function to list available serial devices
list_serial_devices() {
    info "Available serial devices:"
    ls -l /dev/tty* | grep -E "tty(ACM|USB|AMA)" | awk '{print NR " - " $9 " (" $10 ")"}'
}

# Function to list available Bluetooth devices
list_bluetooth_devices() {
    info "Available Bluetooth devices:"
    bluetoothctl devices | grep -v "Device" | awk '{print NR " - " $3 " " $2}'
}

# Function to configure hardware ports
configure_hardware() {
    local config_content=$(cat <<EOF
{
    "hardware_devices": {
        "camera_board": {
            "port": "$1",
            "type": "camera",
            "description": "Camera module"
        },
        "main_board": {
            "port": "$2",
            "type": "main",
            "description": "Main control board"
        }
    },
    "bluetooth_devices": {
        "device1": {
            "address": "$3",
            "channel": 1,
            "description": "Bluetooth device 1"
        },
        "device2": {
            "address": "$4",
            "channel": 1,
            "description": "Bluetooth device 2"
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
        arduino-cli \
        bluetooth \
        bluez \
        bluez-tools \
        rfcomm

    # Add user to dialout group
    info "Adding user to dialout group..."
    sudo usermod -aG dialout "$USERNAME"
    info "You may need to log out and back in for group changes to take effect."

    # Set up Arduino CLI
    info "Configuring Arduino CLI..."
    mkdir -p ~/.arduino15
    arduino-cli config init
    arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    arduino-cli core install arduino:avr
    arduino-cli core install esp32:esp32
    arduino-cli lib install LedControl
    arduino-cli lib install BluetoothSerial
    arduino-cli lib install ESP32Servo

    # Create virtual environment and install Python dependencies
    if [ ! -d "venv" ]; then
        info "Creating virtual environment..."
        python3 -m venv venv
    fi

    info "Activating virtual environment and installing Python packages..."
    source venv/bin/activate
    pip install --upgrade pip
    pip install -r "$REQUIREMENTS_FILE"

    # Download LLM models with resume capability
    info "Downloading LLM models..."
    download_with_resume "$LLM_URL" "$LLM_MODEL"
    download_with_resume "$MMPROJ_URL" "$MMPROJ_MODEL"

    # Hardware configuration
    info "Hardware Configuration Setup"

    # List available serial devices
    list_serial_devices

    # Prompt for camera board
    read -p "Enter the number of the camera board device: " camera_num
    camera_port=$(ls -l /dev/tty* | grep -E "tty(ACM|USB|AMA)" | sed -n "${camera_num}p" | awk '{print $9}')

    # Upload camera sketch
    info "Uploading camera sketch to $camera_port..."
    arduino-cli compile --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/camera"
    arduino-cli upload -p "$camera_port" --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/camera"

    # Disconnect camera board
    info "Please disconnect the camera board, connect the main board and press enter..."
    read -p "" dummy

    # List available serial devices
    list_serial_devices

    # Prompt for main board
    read -p "Enter the number of the main board device: " main_num
    main_port=$(ls -l /dev/tty* | grep -E "tty(ACM|USB|AMA)" | sed -n "${main_num}p" | awk '{print $9}')

    # Upload main sketch
    info "Uploading main sketch to $main_port..."
    arduino-cli compile --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/main"
    arduino-cli upload -p "$main_port" --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/main"

    # Power on the equipment
    info "Please power on the hardware now..."
    read -p "" dummy

    # Bluetooth configuration
    info "Bluetooth Configuration Setup"

    # List available Bluetooth devices
    list_bluetooth_devices

    # Prompt for first Bluetooth device
    read -p "Enter the number of the first Bluetooth device: " bt1_num
    bt1_addr=$(bluetoothctl devices | grep -v "Device" | sed -n "${bt1_num}p" | awk '{print $2}')

    # Prompt for second Bluetooth device
    read -p "Enter the number of the second Bluetooth device: " bt2_num
    bt2_addr=$(bluetoothctl devices | grep -v "Device" | sed -n "${bt2_num}p" | awk '{print $2}')

    # Configure hardware
    configure_hardware "$camera_port" "$main_port" "$bt1_addr" "$bt2_addr"

    # Connect to Bluetooth devices
    info "Connecting to Bluetooth devices..."
    sudo rfcomm bind /dev/rfcomm0 "$bt1_addr" 1
    sudo rfcomm bind /dev/rfcomm1 "$bt2_addr" 1

    info "Installation and configuration completed successfully."
    info "Configuration saved to $CONFIG_FILE"
}

# Run main function
main