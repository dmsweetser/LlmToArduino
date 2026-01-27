#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="$SCRIPT_DIR/config"
SKETCH_DIR="$SCRIPT_DIR/sketch"
REQUIREMENTS_FILE="$SCRIPT_DIR/requirements.txt"
LLM_MODEL="Magistral-Small-2509-Q4_K_M.gguf"
LLM_URL="https://huggingface.co/unsloth/Magistral-Small-2509-GGUF/resolve/main/Magistral-Small-2509-Q4_K_M.gguf?download=true"
MMPROJ_MODEL="mmproj-F16.gguf"
MMPROJ_URL="https://huggingface.co/unsloth/Magistral-Small-2509-GGUF/resolve/main/mmproj-F16.gguf?download=true"

error_exit() {
    echo "Error: $1" >&2
    exit 1
}

info() {
    echo "INFO: $1"
}

info "Checking for Python..."
if ! command -v python3 &> /dev/null; then
    error_exit "Python3 is not installed. Please install Python3 before running this script."
fi

if [ ! -d "venv" ]; then
    info "Creating virtual environment..."
    python3 -m venv venv
    if [ $? -ne 0 ]; then
        error_exit "Failed to create virtual environment."
    fi
    info "Virtual environment created successfully."
else
    info "Virtual environment already exists."
fi

info "Activating virtual environment..."
source venv/bin/activate
if [ $? -ne 0 ]; then
    error_exit "Failed to activate virtual environment."
fi
info "Virtual environment activated successfully."

info "Installing required Python packages..."
if [ ! -f "$REQUIREMENTS_FILE" ]; then
    error_exit "requirements.txt not found in the current directory."
fi

pip install -r "$REQUIREMENTS_FILE"
if [ $? -ne 0 ]; then
    error_exit "Failed to install required Python packages."
fi
info "Python packages installed successfully."

info "Installing Arduino CLI..."

if ! command -v arduino-cli &> /dev/null; then
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
else
    info "Arduino CLI already installed."
fi

info "Installing Arduino AVR core..."
if ! arduino-cli core install arduino:avr; then
    error_exit "Failed to install arduino:avr core."
fi

info "Installing LedControl library..."
if ! arduino-cli lib install LedControl; then
    error_exit "Failed to install LedControl library."
fi

info "Installing BluetoothSerial library..."
if ! arduino-cli lib install BluetoothSerial; then
    error_exit "Failed to install BluetoothSerial library."
fi

info "Installing ESP32Servo library..."
if ! arduino-cli lib install ESP32Servo; then
    error_exit "Failed to install ESP32Servo library."
fi

info "Arduino CLI setup complete."

if [ ! -f "$LLM_MODEL" ]; then
    info "Downloading LLM model..."
    if ! curl -fsSL "$LLM_URL" -o "$LLM_MODEL"; then
        error_exit "Failed to download LLM model."
    fi
    info "LLM model downloaded successfully."
else
    info "LLM model already exists."
fi

if [ ! -f "$MMPROJ_MODEL" ]; then
    info "Downloading MMPROJ model..."
    if ! curl -fsSL "$MMPROJ_URL" -o "$MMPROJ_MODEL"; then
        error_exit "Failed to download MMPROJ model."
    fi
    info "MMPROJ model downloaded successfully."
else
    info "MMPROJ model already exists."
fi

read -p "Please connect the camera board and press enter..." wait

info "Compiling camera sketch..."
if ! arduino-cli compile --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/camera"; then
    error_exit "Failed to compile camera sketch."
fi

info "Uploading camera sketch..."
if ! arduino-cli upload -p "/dev/ttyUSB0" --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/camera"; then
    error_exit "Failed to upload camera sketch."
fi

read -p "Please connect the main board, disconnect the camera board from the main board, and press enter..." wait

info "Compiling main sketch..."
if ! arduino-cli compile --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/main"; then
    error_exit "Failed to compile main sketch."
fi

info "Uploading main sketch..."
if ! arduino-cli upload -p "/dev/ttyUSB0" --fqbn "esp32:esp32:esp32" "$SKETCH_DIR/camera"; then
    error_exit "Failed to upload main sketch."
fi

info "Sketches uploaded successfully."

info "Installation and upload completed successfully."