Here’s your revised **README.md**, updated to reflect the installation and usage details from your batch scripts, and to ensure clarity and accuracy for users:

---

# LlmToArduino

**LlmToArduino** is a Python-based project that bridges the power of Large Language Models (LLMs) with Arduino hardware. It enables natural language interaction with Arduino devices, allowing users to control sensors, actuators, and displays using conversational commands. The system is designed to be extensible, creative, and user-friendly, making it ideal for hobbyists, educators, and developers interested in AI and embedded systems.

---

## Features

- **Natural Language Control**: Issue commands to your Arduino in plain English (e.g., "Turn on the LED" or "What's the temperature?").
- **Real-Time Interaction**: Optionally uses speech recognition and text-to-speech for hands-free operation.
- **State Management**: Maintains conversation history, mood, and directives for a personalized experience.
- **Extensible Architecture**: Easily add new Arduino commands or LLM capabilities.
- **Offline Operation**: Works locally with open-source LLMs (e.g., Mistral) and Whisper for speech recognition.
- **Logging and Debugging**: Detailed logs for troubleshooting and development.

---

## Hardware Requirements

- Arduino board (e.g., Arduino Uno, Mega)
- Current Arduino sketch expects:
  - MAX7219 LED matrix (optional, for visual feedback)
  - Ultrasonic sensor (HC-SR04)
  - CNT5 temperature/humidity sensor (or similar)
- USB connection for serial communication

---

## Software Requirements

- Python 3.8+
- Arduino IDE or Arduino CLI
- Libraries: `pyserial`, `pyttsx3`, `whisper`, `llama-cpp-python`, `pyaudio`, `wave`

---

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/dmsweetser/LlmToArduino.git
cd LlmToArduino
```

### 2. Run the Install Script

Double-click or run the `install.bat` script. This will:
- Create a virtual environment.
- Install all required Python packages.
- Install Arduino CLI and the `LedControl` library.
- Download the LLM model (if not present).
- Detect your Arduino board and COM port.
- Compile and upload the sketch to your Arduino.

**Note:** Ensure your Arduino is connected via USB before running the script.

---

## Usage

### Console Mode

Run the assistant in console mode for text-based interaction:

```bash
run_console.bat
```
- Type commands directly into the console.

### Voice Mode

Run the assistant in voice mode for hands-free operation:

```bash
run_voice.bat
```
- Speak commands naturally (e.g., "What's the distance?" or "Draw a smiley face").

---

## Customization

### Adding New Commands

1. **Arduino Side**: Add a new command handler in the `processCommand` function in the `.ino` file and the `getCapabilities` information command.
2. **Python Side**: This should automatically detect and work with any new capabilities added above.

### Changing the LLM Model

- Replace the `Mistral-7B-Instruct-v0.3-IQ4_XS.gguf` file with your preferred model and update the `model_path` in `script.py`.

---

## Troubleshooting

- **Serial Port Issues**: Ensure the correct COM port is specified in `config/port.txt`.
- **Missing Dependencies**: Run `pip install -r requirements.txt` to install all required libraries.
- **Speech Recognition**: Check your microphone settings and ensure `whisper` is properly installed.
- **Arduino Upload Issues**: Ensure Arduino CLI is installed and the correct FQBN is set in `config/fqbn.txt`.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

## Contributing

Contributions are welcome! Open an issue or submit a pull request for bug fixes, new features, or improvements.

---

## Acknowledgments

- [Mistral AI](https://mistral.ai/) for the LLM model.
- [Whisper](https://github.com/openai/whisper) for speech recognition.
- [Arduino](https://www.arduino.cc/) for the hardware platform.

---