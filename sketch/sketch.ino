#include <BluetoothSerial.h>
#include <Arduino.h>
#include <ESP32Servo.h>

// === No #define: All values are inline ===

// === Bluetooth Serial ===
BluetoothSerial SerialBT;

const int LED_BUILTIN = 2;

// === Motor Pins (Direct Control - No #define) ===
const int MOTOR_FL_FORWARD = 9;
const int MOTOR_FL_BACKWARD = 10;
const int MOTOR_FR_FORWARD = 12;
const int MOTOR_FR_BACKWARD = 13;

// === Servo Pins ===
const int FIXED_SERVO_PIN = 25;
const int TURN_SERVO_PIN = 26;

// === Actuator Pins ===
const int SHOOT_PIN = 32;
const int BUZZER_PIN = 33;

// === Sensor Pins ===
const int LEFT_SENSOR = 35;
const int MIDDLE_SENSOR = 36;
const int RIGHT_SENSOR = 39;
const int TRIG_PIN = 9;
const int ECHO_PIN = 8;

// === Global Variables ===
bool isCameraActive = false;
bool isTracking = false;
bool isAvoiding = false;
bool isFollowing = false;
bool isShooting = false;
bool isBuzzerPlaying = false;
int servoAngle = 90;
int shootDuration = 200;
int buzzerTone = 0;
int buzzerDuration = 1000;
int ledPacing = 1000;
unsigned long lastBlinkTime = 0;
bool ledState = LOW;

// === UART Buffer & Image Handling ===
const int UART_BAUD_RATE = 115200;
const int MAX_IMAGE_SIZE = 60000;
uint8_t imageBuffer[MAX_IMAGE_SIZE];
int imageIndex = 0;
bool imageReceived = false;
bool imageStarted = false;
uint32_t expectedImageSize = 0;
unsigned long lastImageTime = 0;

// === Servos ===
Servo fixedServo;
Servo turnServo;

// === Sensor Data ===
int leftSensorValue = 0;
int middleSensorValue = 0;
int rightSensorValue = 0;
int ultrasonicDistance = 0;

// === Script Execution ===
String scriptBuffer = "";
String currentLine = "";
int scriptIndex = 0;
bool scriptRunning = false;

// === Labels (Map label name to line number) ===
struct Label {
  String name;
  int line;
};
Label labels[10];  // Max 10 labels
int labelCount = 0;

// === Motor Control (Inline, No #define) ===
void moveForward(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FL_FORWARD, speed);
  analogWrite(MOTOR_FL_BACKWARD, 0);
  analogWrite(MOTOR_FR_FORWARD, speed);
  analogWrite(MOTOR_FR_BACKWARD, 0);
}

void moveBackward(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FL_FORWARD, 0);
  analogWrite(MOTOR_FL_BACKWARD, speed);
  analogWrite(MOTOR_FR_FORWARD, 0);
  analogWrite(MOTOR_FR_BACKWARD, speed);
}

void moveLeft(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FL_FORWARD, 0);
  analogWrite(MOTOR_FL_BACKWARD, speed);
  analogWrite(MOTOR_FR_FORWARD, speed);
  analogWrite(MOTOR_FR_BACKWARD, 0);
}

void moveRight(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FL_FORWARD, speed);
  analogWrite(MOTOR_FL_BACKWARD, 0);
  analogWrite(MOTOR_FR_FORWARD, 0);
  analogWrite(MOTOR_FR_BACKWARD, speed);
}

void moveStop() {
  analogWrite(MOTOR_FL_FORWARD, 0);
  analogWrite(MOTOR_FL_BACKWARD, 0);
  analogWrite(MOTOR_FR_FORWARD, 0);
  analogWrite(MOTOR_FR_BACKWARD, 0);
}

void moveClockwise(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FL_FORWARD, 0);
  analogWrite(MOTOR_FL_BACKWARD, speed);
  analogWrite(MOTOR_FR_FORWARD, 0);
  analogWrite(MOTOR_FR_BACKWARD, speed);
}

void moveCounterClockwise(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FL_FORWARD, speed);
  analogWrite(MOTOR_FL_BACKWARD, 0);
  analogWrite(MOTOR_FR_FORWARD, speed);
  analogWrite(MOTOR_FR_BACKWARD, 0);
}

// === Servo Control ===
void moveFixedServo(int angle) {
  fixedServo.write(angle);
}

void moveTurnServo(int angle) {
  turnServo.write(angle);
}

void setServoAngle(int angle) {
  moveFixedServo(angle);
  moveTurnServo(angle);
  servoAngle = angle;
}

// === Actuators ===
void triggerShoot() {
  digitalWrite(SHOOT_PIN, HIGH);
  delay(shootDuration);
  digitalWrite(SHOOT_PIN, LOW);
}

void playBuzzer(int newTone, int duration) {
  tone(BUZZER_PIN, newTone);
  delay(duration);
  noTone(BUZZER_PIN);
}

// === Sensors ===
int readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  ultrasonicDistance = duration * 0.034 / 2;
  return ultrasonicDistance;
}

void readIRSensors() {
  leftSensorValue = analogRead(LEFT_SENSOR);
  middleSensorValue = analogRead(MIDDLE_SENSOR);
  rightSensorValue = analogRead(RIGHT_SENSOR);
}

// === UART Image Handler ===
void handleUartImage() {
  while (Serial2.available() > 0) {
    char c = Serial2.read();

    if (c == 'S' && !imageStarted) {
      imageStarted = true;
      imageIndex = 0;
      expectedImageSize = 0;
      debugPrint("📷 Image start detected");
    } else if (imageStarted && imageIndex == 0) {
      // Read 4-byte image length
      expectedImageSize = (uint32_t(c) << 24);
      imageIndex++;
    } else if (imageStarted && imageIndex == 1) {
      expectedImageSize |= (uint32_t(c) << 16);
      imageIndex++;
    } else if (imageStarted && imageIndex == 2) {
      expectedImageSize |= (uint32_t(c) << 8);
      imageIndex++;
    } else if (imageStarted && imageIndex == 3) {
      expectedImageSize |= (uint32_t(c));
      imageIndex++;
      debugPrint("📏 Expected image size: ");
      debugPrint(String(expectedImageSize).c_str());
      debugPrint(" bytes");
    } else if (imageStarted && imageIndex > 3) {
      if (imageIndex - 4 < MAX_IMAGE_SIZE) {
        imageBuffer[imageIndex - 4] = c;
      } else {
        debugPrint("Image buffer overflow!");
        imageStarted = false;
        imageIndex = 0;
        continue;
      }

      if (imageIndex - 4 >= expectedImageSize) {
        // End of image received
        if (c == 'E') {
          imageReceived = true;
          debugPrint("Full image received and stored");
          lastImageTime = millis();
          isCameraActive = true;
        } else {
          debugPrint("Unexpected end marker");
        }
        imageStarted = false;
        imageIndex = 0;
      }
    }
  }

  // If image is ready, send it over Bluetooth
  if (imageReceived && SerialBT.availableForWrite() > 0) {
    // Send image over Bluetooth (chunked)
    for (int i = 0; i < expectedImageSize; i++) {
      SerialBT.write(imageBuffer[i]);
    }
    SerialBT.write('E'); // End marker
    SerialBT.flush();
    debugPrint("Image sent over Bluetooth");
    imageReceived = false;
  }
}

void setLED(String params) {
  unsigned int newPace = params.toInt();
  if (newPace > 0) {
    ledPacing = newPace;
    sendResponse("OK", "LED pacing set to " + String(ledPacing));
  } else {
    sendResponse("ERROR", "Invalid pacing value");
  }
}

// === Response Helpers ===
void sendResponse(String status, String message) {
  String response = status + ":" + message + "\n";
  SerialBT.print(response);
  SerialBT.flush();
}

void echo(String params) {
  sendResponse("OK", params);
}

void sendState() {
  String state = "led_pacing:" + String(ledPacing) + "\n";
  state += "led_state:" + String(ledState) + "\n";
  state += "distance:" + String(ultrasonicDistance) + "\n";
  state += "left_sensor:" + String(leftSensorValue) + "\n";
  state += "middle_sensor:" + String(middleSensorValue) + "\n";
  state += "right_sensor:" + String(rightSensorValue) + "\n";
  state += "servo_angle:" + String(servoAngle) + "\n";
  state += "is_camera_active:" + String(isCameraActive) + "\n";
  state += "is_tracking:" + String(isTracking) + "\n";
  state += "is_avoiding:" + String(isAvoiding) + "\n";
  state += "is_following:" + String(isFollowing) + "\n";
  state += "is_shooting:" + String(isShooting) + "\n";
  state += "is_buzzer_playing:" + String(isBuzzerPlaying) + "\n";
  SerialBT.print(state);
  SerialBT.flush();
}

void sendCapabilities() {
  String capabilitiesStr =
    "setLED:pacing\n"
    "echo:message\n"
    "getStatus:\n"
    "getCapabilities:\n"
    "move:direction,speed\n"
    "servo:angle\n"
    "shoot:duration\n"
    "buzzer:tone,duration\n"
    "camera:action\n"
    "track:mode\n"
    "avoid:mode\n"
    "follow:mode\n"
    "stop:\n"
    "snapshot:\n";

  SerialBT.print(capabilitiesStr);
  SerialBT.flush();
}

// === Debug Print ===
void debugPrint(const char* message) {
  Serial.print(message);
  Serial.flush();
}

// === Self-Test ===
void performSelfTest() {
  debugPrint("Testing LED...");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  debugPrint("LED test passed");

  debugPrint("Testing servos...");
  moveFixedServo(0);
  delay(1000);
  moveFixedServo(90);
  delay(1000);
  moveFixedServo(180);
  delay(1000);
  moveFixedServo(90);
  debugPrint("Servo test passed");

  debugPrint("Testing buzzer...");
  tone(BUZZER_PIN, 1000);
  delay(500);
  noTone(BUZZER_PIN);
  delay(500);
  tone(BUZZER_PIN, 500);
  delay(500);
  noTone(BUZZER_PIN);
  debugPrint("Buzzer test passed");

  debugPrint("Testing motors (forward)...");
  moveForward(100);
  delay(1000);
  moveStop();
  debugPrint("Motor forward test passed");

  debugPrint("Testing motors (backward)...");
  moveBackward(100);
  delay(1000);
  moveStop();
  debugPrint("Motor backward test passed");

  debugPrint("Testing motors (left)...");
  moveLeft(100);
  delay(1000);
  moveStop();
  debugPrint("Motor left test passed");

  debugPrint("Testing motors (right)...");
  moveRight(100);
  delay(1000);
  moveStop();
  debugPrint("Motor right test passed");

  debugPrint("Testing shooting...");
  triggerShoot();
  debugPrint("Shooting test passed");

  debugPrint("Testing ultrasonic...");
  int dist = readUltrasonic();
  if (dist > 0 && dist < 200) {
    debugPrint("Distance: ");
    debugPrint(String(dist).c_str());
    debugPrint(" cm");
  } else {
    debugPrint("Ultrasonic failed");
  }

  debugPrint("Testing IR sensors...");
  readIRSensors();
  if (leftSensorValue > 100 && middleSensorValue > 100 && rightSensorValue > 100) {
    debugPrint("IR sensors test passed");
  } else {
    debugPrint("IR sensors failed");
  }
}

// === Bluetooth Input Handler ===
void processBluetoothInput() {
  if (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n') {
      processInput(scriptBuffer);
      scriptBuffer = "";
    } else {
      scriptBuffer += c;
    }
  }
}

void getStatus() {
  sendState();
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
  } else if (command == "move") {
    move(params);
  } else if (command == "servo") {
    servo(params);
  } else if (command == "shoot") {
    shoot(params);
  } else if (command == "buzzer") {
    buzzer(params);
  } else if (command == "camera") {
    camera(params);
  } else if (command == "track") {
    track(params);
  } else if (command == "avoid") {
    avoid(params);
  } else if (command == "follow") {
    follow(params);
  } else if (command == "stop") {
    stop();
  } else if (command == "snapshot") {
    // Send command to camera ESP32 to capture
    Serial2.write('C'); // Trigger capture
    debugPrint("Sent 'C' to camera ESP32");
    sendResponse("OK", "Snapshot requested");
  } else {
    sendResponse("ERROR", "Unknown command type");
  }
}

// === Parse Script Input ===
void processInput(String commandLine) {
  commandLine.trim();
  if (commandLine.length() == 0) return;

  int colonIndex = commandLine.indexOf(':');
  if (colonIndex == -1) {
    sendResponse("ERROR", "Invalid command format");
    return;
  }

  String command = commandLine.substring(0, colonIndex);
  String params = commandLine.substring(colonIndex + 1);

  if (command == "run") {
    scriptBuffer = params;  // Load script
    scriptRunning = true;
    scriptIndex = 0;
    labelCount = 0;
    debugPrint("Script loaded. Starting execution.\n");
  } else if (command == "stop") {
    scriptRunning = false;
    sendResponse("OK", "Script stopped");
  } else {
    processCommand(command, params);
  }
}

// === Execute Script Line by Line ===
void executeScript() {
  if (scriptBuffer.length() == 0) {
    scriptRunning = false;
    return;
  }

  // Split script into lines
  int start = 0;
  int end = 0;
  while (end < scriptBuffer.length()) {
    if (scriptBuffer[end] == '\n' || end == scriptBuffer.length() - 1) {
      String line = scriptBuffer.substring(start, end + 1);
      line.trim();

      if (line.length() > 0) {
        // Check for label
        if (line.startsWith("label:")) {
          String labelName = line.substring(5).trim();
          if (labelName.length() > 0) {
            if (labelCount < 10) {
              labels[labelCount].name = labelName;
              labels[labelCount].line = scriptIndex;
              labelCount++;
              debugPrint("Label added: ");
              debugPrint(labelName.c_str());
              debugPrint("\n");
            }
          }
        } else {
          // Parse command
          int colon = line.indexOf(':');
          if (colon != -1) {
            String cmd = line.substring(0, colon);
            String params = line.substring(colon + 1);
            parseCommand(cmd, params);
          } else {
            // Check for GOTO
            if (line.startsWith("goto:")) {
              String labelName = line.substring(4).trim();
              for (int i = 0; i < labelCount; i++) {
                if (labels[i].name == labelName) {
                  scriptIndex = labels[i].line;
                  debugPrint("GOTO to ");
                  debugPrint(labelName.c_str());
                  debugPrint("\n");
                  return; // Jump
                }
              }
              debugPrint("Label not found: ");
              debugPrint(labelName.c_str());
              debugPrint("\n");
            }
          }
        }
      }

      start = end + 1;
      if (end == scriptBuffer.length() - 1) break;
    }
    end++;
  }

  // Done with script
  scriptRunning = false;
  debugPrint("Script execution complete.\n");
}

// === Parse and Execute Individual Command ===
void parseCommand(String command, String params) {
  if (command == "forward") {
    int speed = params.toInt();
    moveForward(speed);
  } else if (command == "backward") {
    int speed = params.toInt();
    moveBackward(speed);
  } else if (command == "left") {
    int speed = params.toInt();
    moveLeft(speed);
  } else if (command == "right") {
    int speed = params.toInt();
    moveRight(speed);
  } else if (command == "stop") {
    moveStop();
  } else if (command == "clockwise") {
    int speed = params.toInt();
    moveClockwise(speed);
  } else if (command == "counter-clockwise") {
    int speed = params.toInt();
    moveCounterClockwise(speed);
  } else if (command == "servo") {
    int angle = params.toInt();
    setServoAngle(angle);
  } else if (command == "shoot") {
    int duration = params.toInt();
    if (duration > 0) {
      shootDuration = duration;
      triggerShoot();
    }
  } else if (command == "buzzer") {
    int tone = 0;
    int duration = 1000;
    int comma = params.indexOf(',');
    if (comma != -1) {
      tone = params.substring(0, comma).toInt();
      duration = params.substring(comma + 1).toInt();
    } else {
      tone = params.toInt();
    }
    if (tone > 0 && duration > 0) {
      playBuzzer(tone, duration);
    }
  } else if (command == "delay") {
    int ms = params.toInt();
    delay(ms);
  } else if (command == "echo") {
    sendResponse("OK", params);
  } else {
    sendResponse("ERROR", "Unknown command: " + command);
  }
}










// === Setup Function ===
void setup() {
  Serial.begin(115200);
  delay(1000);
  debugPrint("ESP32 Serial started at 115200 baud");

  // LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);

  // Actuators
  pinMode(SHOOT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Motor Pins
  pinMode(MOTOR_FL_FORWARD, OUTPUT);
  pinMode(MOTOR_FL_BACKWARD, OUTPUT);
  pinMode(MOTOR_FR_FORWARD, OUTPUT);
  pinMode(MOTOR_FR_BACKWARD, OUTPUT);

  // Sensor Inputs
  pinMode(LEFT_SENSOR, INPUT);
  pinMode(MIDDLE_SENSOR, INPUT);
  pinMode(RIGHT_SENSOR, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Servos
  fixedServo.attach(FIXED_SERVO_PIN);
  turnServo.attach(TURN_SERVO_PIN);
  moveFixedServo(90);
  moveTurnServo(90);

  // Bluetooth
  SerialBT.begin("ESP32-Script-Rover");
  debugPrint("Bluetooth Serial started");

  // UART for camera (from other ESP32)
  Serial2.begin(115200, SERIAL_8N1, 14, 13);  // RX:14, TX:13
  debugPrint("UART (Serial2) initialized for camera");

  // Self-test
  debugPrint("Starting system self-test...");
  performSelfTest();

  moveStop();
  debugPrint("System ready. Send script via Bluetooth.");
}

// === Main Loop ===
void loop() {
  // Handle UART image (from camera ESP32)
  handleUartImage();

  // Handle Bluetooth script input
  processBluetoothInput();

  // Update sensors
  readUltrasonic();
  readIRSensors();

  // Blink LED
  unsigned long currentMillis = millis();
  if (currentMillis - lastBlinkTime >= ledPacing) {
    lastBlinkTime = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }

  // Process serial debug input
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      processInput(scriptBuffer);
      scriptBuffer = "";
    } else {
      scriptBuffer += inChar;
    }
  }

  // If script is running, execute it
  if (scriptRunning) {
    executeScript();
  }
}
