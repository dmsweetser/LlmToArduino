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

# Function to check if arduino-cli is installed
is_arduino_cli_installed() {
    if command -v arduino-cli &> /dev/null; then
        return 0
    fi
    return 1
}

# Function to install arduino-cli if needed
install_arduino_cli() {
    if is_arduino_cli_installed; then
        info "Arduino CLI is already installed."
        return 0
    fi

    info "Installing Arduino CLI..."
    mkdir -p ~/local/bin
    echo 'export PATH="$HOME/local/bin:$PATH"' >> ~/.bashrc
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/local/bin sh
}

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

# Function to configure hardware with manual MAC entry
configure_hardware() {
    # Start bluetoothctl and scan
    info "Starting Bluetooth scan for 10 seconds..."
    bluetoothctl power on &
    bluetoothctl agent on &
    timeout 60 bluetoothctl scan on &
    sleep 10

    # Show available devices
    info "Available Bluetooth devices:"
    bluetoothctl devices

    # Get camera MAC address
    read -p "Enter the MAC address of your camera module: " camera_mac
    if [ -z "$camera_mac" ]; then
        error_exit "Camera MAC address is required."
    fi

    # Validate MAC format
    if ! [[ "$camera_mac" =~ ^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$ ]]; then
        error_exit "Invalid MAC address format for camera. Expected format: XX:XX:XX:XX:XX:XX"
    fi

    # Get main board MAC address
    read -p "Enter the MAC address of your main board: " main_mac
    if [ -z "$main_mac" ]; then
        error_exit "Main board MAC address is required."
    fi

    # Validate MAC format
    if ! [[ "$main_mac" =~ ^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$ ]]; then
        error_exit "Invalid MAC address format for main board. Expected format: XX:XX:XX:XX:XX:XX"
    fi

    # Create config file
    local config_content=$(cat <<EOF
{
    "bluetooth_devices": {
        "camera_board": {
            "address": "$camera_mac",
            "channel": 1,
            "description": "Camera module"
        },
        "main_board": {
            "address": "$main_mac",
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

    # Install Arduino CLI only if needed
    install_arduino_cli

    # Add user to dialout group
    info "Adding user to dialout group..."
    sudo usermod -aG dialout "$USERNAME"
    info "You may need to log out and back in for group changes to take effect."

    # Set up Arduino CLI if it was installed
    if is_arduino_cli_installed; then
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
    fi

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

        # Simple Bluetooth configuration with manual entry
        configure_hardware

        info "Installation and configuration completed successfully."
        info "Configuration saved to $CONFIG_FILE"
    fi
}

# Run main function
main