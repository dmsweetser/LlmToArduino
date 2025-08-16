@echo off
setlocal enabledelayedexpansion

REM Create config directory if it doesn't exist
if not exist config (
    mkdir config
)

echo Setting up Python virtual environment...

REM Check if Python is installed
where py > nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Python is not installed. Please install Python before running this script.
    exit /b 1
)

REM Check if virtual environment directory already exists
if not exist venv (
    REM Create a virtual environment
    py -m venv venv
    if %errorlevel% neq 0 (
        echo Error: Unable to create virtual environment.
        exit /b 1
    )
    echo Virtual environment created successfully.
) else (
    echo Virtual environment already exists.
)

REM Activate the virtual environment
call venv\Scripts\Activate.bat
if %errorlevel% neq 0 (
    echo Error: Unable to activate virtual environment.
    exit /b 1
)
echo Virtual environment activated successfully.

echo Installing required Python packages...

REM Install required Python packages
pip install -r requirements.txt
if %errorlevel% neq 0 (
    echo Error: Unable to install required Python packages.
    exit /b 1
)

echo Required Python packages installed successfully.

echo Installing Arduino CLI...

REM Download Arduino CLI
curl -fsSL https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip -o arduino-cli.zip
if %errorlevel% neq 0 (
    echo Error: Failed to download Arduino CLI.
    exit /b 1
)

REM Extract Arduino CLI using PowerShell
powershell -Command "Expand-Archive -Path 'arduino-cli.zip' -Force -DestinationPath '%~dp0arduino-cli'"
if %errorlevel% neq 0 (
    echo Error: Failed to extract Arduino CLI.
    exit /b 1
)

REM Add Arduino CLI to PATH temporarily for the session
set PATH=%PATH%;%~dp0arduino-cli

arduino-cli core install arduino:avr
if %errorlevel% neq 0 (
    echo Error: Failed to install arduino:avr core framework
    exit /b 1
)

arduino-cli lib install LedControl
if %errorlevel% neq 0 (
    echo Error: Failed to install arduino:avr LedControl library
    exit /b 1
)

echo Arduino CLI installed successfully.
echo Arduino CLI installed successfully.

echo Downloading LLM model if not present...

if not exist Mistral-7B-Instruct-v0.3-IQ4_XS.gguf (
    curl -fsSL "https://huggingface.co/bartowski/Mistral-7B-Instruct-v0.3-GGUF/resolve/main/Mistral-7B-Instruct-v0.3-IQ4_XS.gguf?download=true" -o Mistral-7B-Instruct-v0.3-IQ4_XS.gguf
    if %errorlevel% neq 0 (
        echo Error: Failed to download LLM model.
        exit /b 1
    )
) else (
    echo LLM model already exists in the config folder.
)

echo LLM model is ready.

echo Detecting Arduino board FQBN and COM port...

REM Use Arduino CLI to detect connected boards and store the output in config\board_list.txt
arduino-cli board list > config\board_list.txt
if %errorlevel% neq 0 (
    echo Error: Unable to detect Arduino board or port.
    exit /b 1
)

echo Detecting Arduino board FQBN and COM port from config\board_list.txt...

set "FOUND_BOARD=0"

REM Process every line from config\board_list.txt skipping the header.
for /f "skip=1 delims=" %%L in (config\board_list.txt) do (
    echo %%L | findstr /i "arduino:" >nul
    if not errorlevel 1 (
        REM Found a line that includes "arduino:".
        REM Get the port (first token of the line)
        for /f "tokens=1 delims= " %%P in ("%%L") do (
            set "port=%%P"
        )

        REM Iterate over each word (token) in the line.
        REM By "remembering" the last two tokens, we capture the FQBN.
        set "last1="
        set "last2="
        for %%T in (%%L) do (
            set "last2=!last1!"
            set "last1=%%T"
        )
        REM Now, !last2! should be the full FQBN (e.g. arduino:avr:mega)
        echo Board detected on port !port! with FQBN !last2!
        echo !port! > config\port.txt
        echo !last2! > config\fqbn.txt
        set "FOUND_BOARD=1"
        goto :end
    )
)

if "%FOUND_BOARD%"=="0" (
    echo Error: No Arduino board found.
    exit /b 1
)

:end
echo Detection complete. Port and FQBN saved to config\port.txt and config\fqbn.txt.

echo Compiling sketch for Arduino...
arduino-cli compile --fqbn arduino:avr:mega sketch
echo Uploading sketch to Arduino
arduino-cli upload -p %port% --fqbn arduino:avr:mega sketch
echo Sketch uploaded successfully.

echo Install completed successfully.