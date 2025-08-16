#include <LedControl.h>

// Global variables
unsigned long lastBlinkTime = 0;
bool ledState = LOW;
unsigned int ledPacing = 1000; // Default blink interval in milliseconds
String inputBuffer = "";
int sentResponse = 0;

// MAX7219 display setup
LedControl lc = LedControl(12, 11, 10, 4); // DIN, CS, CLK, Number of devices
bool ledStates[32][8] = {{false}}; // Track the state of each LED

// Ultrasonic Sensor Pins
const int trigPin = 9;
const int echoPin = 8;

// CNT5 Sensor Pins
const int cnt5Pin = A0; // Analog pin for CNT5 sensor

// Variables for Ultrasonic Sensor
long duration;
int distance;

// Variables for CNT5 Sensor
float temperature;
float humidity;

void setup() {
  Serial.begin(9600); // Use USB for serial communication
  pinMode(LED_BUILTIN, OUTPUT);
  mapAndSetLed(0, 0, true);
  mapAndSetLed(7, 31, true);
  delay(3000);

  // Initialize the MAX7219 display
  for (int i = 0; i < 4; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 2); // Set brightness level (0 is min, 15 is max)
    lc.clearDisplay(i); // Clear the display without setting fixed points
  }

  // Initialize Ultrasonic Sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  delay(2000);
}

void loop() {
  // Read incoming serial data
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      processInput(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += inChar;
    }
  }

  // Read Ultrasonic Sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  // Read CNT5 Sensor
  int sensorValue = analogRead(cnt5Pin);
  temperature = sensorValue / 10.0; // Example conversion, adjust as per your sensor's datasheet
  humidity = sensorValue / 5.0; // Example conversion, adjust as per your sensor's datasheet

  // Blink the onboard LED at the configured pace
  unsigned long currentMillis = millis();
  if (currentMillis - lastBlinkTime >= ledPacing) {
    lastBlinkTime = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

void processInput(String commandLine) {
  commandLine.trim();
  if (commandLine.length() == 0) {
    return;
  }

  int colonIndex = commandLine.indexOf(':');
  if (colonIndex == -1) {
    sendResponse("ERROR", "Invalid command format");
    return;
  }

  String command = commandLine.substring(0, colonIndex);
  String params = commandLine.substring(colonIndex + 1);

  processCommand(command, params);
}

void processCommand(String command, String params) {
  if (command == "setLED") {
    setLED(params);
  } else if (command == "echo") {
    echo(params);
  } else if (command == "getStatus") {
    getStatus();
  } else if (command == "getCapabilities") {
    sendCapabilities();
  } else if (command == "draw") {
    draw(params);
  } else if (command == "getSensorData") {
    getSensorData();
  } else {
    sendResponse("ERROR", "Unknown command type");
  }
}

void setLED(String params) {
  unsigned int newPace = params.toInt();
  if (newPace > 0) {
    ledPacing = newPace;
    sendResponse("OK", "LED pacing set to " + String(ledPacing));
  }
}

void echo(String params) {
  sendResponse("OK", params);
}

void getStatus() {
  sendState();
}

void draw(String params) {
  // Clear the display
  for (int i = 0; i < 4; i++) {
    lc.clearDisplay(i);
  }

  // Reset the LED states
  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 8; j++) {
      ledStates[i][j] = false;
    }
  }

  // Parse the points string and set the LEDs accordingly
  int pointStart = 0;
  while (pointStart < params.length()) {
    int pointEnd = params.indexOf(';', pointStart);
    if (pointEnd == -1) break;
    String pointStr = params.substring(pointStart, pointEnd);
    int commaIndex = pointStr.indexOf(',');
    if (commaIndex == -1) break;
    int x = pointStr.substring(0, commaIndex).toInt();
    int y = pointStr.substring(commaIndex + 1).toInt();

    // Set the LED
    mapAndSetLed(x, y, true);
    ledStates[x][y] = true;
    pointStart = pointEnd + 1;
  }
  sendResponse("OK", "LED display updated");
}

void mapAndSetLed(int x, int y, bool state) {
  int device = x / 8;
  int deviceRow = y;
  int deviceCol = x % 8;
  lc.setLed(device, deviceRow, deviceCol, state);
}

void sendResponse(String status, String message) {
  String response = status + ":" + message + "\n";
  Serial.print(response);
}

void sendState() {
  String state = "led_pacing:" + String(ledPacing) + "\n";
  state += "led_state:" + String(ledState) + "\n";
  state += "distance:" + String(distance) + "\n";
  state += "temperature:" + String(temperature) + "\n";
  state += "humidity:" + String(humidity) + "\n";

  // Add LED panel state to the status
  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 8; j++) {
      if (ledStates[i][j]) {
        state += "led:" + String(i) + "," + String(j) + "\n";
      }
    }
  }
  Serial.print(state);
}

void sendCapabilities() {
  String capabilitiesStr =
    "setLED:pacing\n"
    "echo:message\n"
    "getStatus:\n"
    "getCapabilities:\n"
    "draw:points\n"
    "getSensorData:\n";

  Serial.print(capabilitiesStr);
}

void getSensorData() {
  String sensorData = "distance:" + String(distance) + "\n";
  sensorData += "temperature:" + String(temperature) + "\n";
  sensorData += "humidity:" + String(humidity) + "\n";
  Serial.print(sensorData);
}
