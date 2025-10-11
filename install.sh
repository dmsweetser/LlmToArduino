#!/bin/bash

set -euo pipefail

# === Configuration ===
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="$SCRIPT_DIR/config"
SKETCH_DIR="$SCRIPT_DIR/sketch"
REQUIREMENTS_FILE="$SCRIPT_DIR/requirements.txt"
LLM_MODEL="Mistral-7B-Instruct-v0.3-IQ4_XS.gguf"
LLM_URL="https://huggingface.co/bartowski/Mistral-7B-Instruct-v0.3-GGUF/resolve/main/Mistral-7B-Instruct-v0.3-IQ4_XS.gguf?download=true"

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
    info "Downloading Arduino CLI..."
    if ! curl -fsSL "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz" -o arduino-cli.tar.gz; then
        error_exit "Failed to download Arduino CLI."
    fi

    # Extract
    info "Extracting Arduino CLI..."
    if ! tar -xzf arduino-cli.tar.gz; then
        error_exit "Failed to extract Arduino CLI."
    fi

    # Move to a known location (optional: add to PATH)
    mv arduino-cli "$SCRIPT_DIR/arduino-cli"
    chmod +x "$SCRIPT_DIR/arduino-cli"

    # Add to PATH temporarily
    export PATH="$SCRIPT_DIR/arduino-cli:$PATH"
else
    info "Arduino CLI already installed."
fi

# === Step 6: Install Arduino cores and libraries ===
info "Installing Arduino AVR core..."
if ! "$SCRIPT_DIR/arduino-cli" core install arduino:avr; then
    error_exit "Failed to install arduino:avr core."
fi

info "Installing LedControl library..."
if ! "$SCRIPT_DIR/arduino-cli" lib install LedControl; then
    error_exit "Failed to install LedControl library."
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

# === Step 8: Detect Arduino board and port ===
info "Detecting connected Arduino board..."

# Save board list to config file
if ! "$SCRIPT_DIR/arduino-cli" board list > "$CONFIG_DIR/board_list.txt"; then
    error_exit "Failed to detect Arduino board."
fi

# Parse board list to find FQBN and port
FOUND_BOARD=0
while IFS= read -r line; do
    # Skip header line (first line)
    if [[ "$line" == *"Port"* ]] || [[ "$line" == *"Board"* ]]; then
        continue
    fi

    # Look for "arduino:" in the line
    if [[ "$line" == *"arduino:"* ]]; then
        # Extract port (first field)
        port=$(echo "$line" | awk '{print $1}')

        # Extract FQBN (last two non-empty fields)
        fqbn=$(echo "$line" | awk '{for(i=1;i<=NF;i++) if($i ~ /^arduino:/) {f=$i; if(i<NF) f=$i" "$i+1} } END{print f}')

        if [[ -n "$port" && -n "$fqbn" ]]; then
            echo "$port" > "$CONFIG_DIR/port.txt"
            echo "$fqbn" > "$CONFIG_DIR/fqbn.txt"
            info "Board detected on port $port with FQBN $fqbn"
            FOUND_BOARD=1
            break
        fi
    fi
done < "$CONFIG_DIR/board_list.txt"

if [ "$FOUND_BOARD" -eq 0 ]; then
    error_exit "No Arduino board found."
fi

info "Board detection complete. Port and FQBN saved to config/port.txt and config/fqbn.txt."

# === Step 9: Compile and upload sketch ===
info "Compiling sketch..."
if ! "$SCRIPT_DIR/arduino-cli" compile --fqbn "$fqbn" "$SKETCH_DIR"; then
    error_exit "Failed to compile sketch."
fi

info "Uploading sketch..."
if ! "$SCRIPT_DIR/arduino-cli" upload -p "$port" --fqbn "$fqbn" "$SKETCH_DIR"; then
    error_exit "Failed to upload sketch."
fi

info "Sketch uploaded successfully."

# === Final message ===
info "Installation and upload completed successfully."