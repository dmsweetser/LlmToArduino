#!/bin/bash

set -euo pipefail

# === Configuration ===
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="$SCRIPT_DIR/config"
SKETCH_DIR="$SCRIPT_DIR/sketch"
REQUIREMENTS_FILE="$SCRIPT_DIR/requirements.txt"
LLM_MODEL="Qwen2.5-7B-Instruct-IQ4_XS.gguf"
LLM_URL="https://huggingface.co/bartowski/Qwen2.5-7B-Instruct-GGUF/resolve/main/Qwen2.5-7B-Instruct-IQ4_XS.gguf?download=true"

# === Functions ===

error_exit() {
    echo "Error: $1" >&2
    exit 1
}

info() {
    echo "INFO: $1"
}

# === Step 1: Create config directory ===
info "Creating config directory..."
mkdir -p "$CONFIG_DIR"

# === Step 2: Check for Python and virtual environment ===
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

# === Step 3: Activate virtual environment ===
info "Activating virtual environment..."
source venv/bin/activate
if [ $? -ne 0 ]; then
    error_exit "Failed to activate virtual environment."
fi
info "Virtual environment activated successfully."

# === Step 4: Install Python packages ===
info "Installing required Python packages..."
if [ ! -f "$REQUIREMENTS_FILE" ]; then
    error_exit "requirements.txt not found in the current directory."
fi

pip install -r "$REQUIREMENTS_FILE"
if [ $? -ne 0 ]; then
    error_exit "Failed to install required Python packages."
fi
info "Python packages installed successfully."

# === Step 5: Install Arduino CLI ===
info "Installing Arduino CLI..."

# Check if arduino-cli is already installed
if ! command -v arduino-cli &> /dev/null; then
    # Download arduino-cli
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
else
    info "Arduino CLI already installed."
fi

# === Step 6: Install Arduino cores and libraries ===
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

info "Arduino CLI setup complete."

# === Step 7: Download LLM model ===
info "Downloading LLM model..."

if [ ! -f "$LLM_MODEL" ]; then
    info "Downloading LLM model..."
    if ! curl -fsSL "$LLM_URL" -o "$LLM_MODEL"; then
        error_exit "Failed to download LLM model."
    fi
    info "LLM model downloaded successfully."
else
    info "LLM model already exists."
fi

# === Step 8: Display board list and prompt user for port and FQBN ===
info "Detecting connected Arduino boards..."

# Get the board list and display it
if ! arduino-cli board list; then
    error_exit "Failed to detect Arduino boards."
fi

# Prompt user for port and FQBN
read -p "Enter the port (e.g., /dev/ttyUSB0, COM3): " port
read -p "Enter the FQBN (e.g., arduino:avr:mega): " fqbn

# Validate input
if [[ -z "$port" || -z "$fqbn" ]]; then
    error_exit "Port and FQBN are required."
fi

# Save to config files
echo "$port" > "$CONFIG_DIR/port.txt"
echo "$fqbn" > "$CONFIG_DIR/fqbn.txt"

info "Port and FQBN saved to config directory."

# === Step 9: Compile and upload sketch ===
info "Compiling sketch..."
if ! arduino-cli compile --fqbn "$fqbn" "$SKETCH_DIR"; then
    error_exit "Failed to compile sketch."
fi

info "Uploading sketch..."
if ! arduino-cli upload -p "$port" --fqbn "$fqbn" "$SKETCH_DIR"; then
    error_exit "Failed to upload sketch."
fi

info "Sketch uploaded successfully."

# === Final message ===
info "Installation and upload completed successfully."