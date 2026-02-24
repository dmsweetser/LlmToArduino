#!/bin/bash

# -----------------------------
# Run script for LlmToArduino
# -----------------------------

# Activate virtual environment
if [ ! -d "venv" ]; then
    echo "Error: virtual environment not found. Run ./install.sh first."
    exit 1
fi

source venv/bin/activate

python3 app.py

exit $?
