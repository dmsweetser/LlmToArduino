#include <BluetoothSerial.h>
#include <Arduino.h>

// === Configuration ===
#define LED_BUILTIN 2

// === Camera UART Pins (Now from external ESP32) ===
#define CAM_TX_PIN 13  // Connected to RX on main ESP32
#define CAM_RX_PIN 14  // Connected to TX on main ESP32

// === Pins Configuration ===
#define FIXED_SERVO_PIN 25
#define TURN_SERVO_PIN 26
#define SHOOT_PIN 32
#define BUZZER_PIN 33

// === Motor Pins Configuration (Updated for your car shield) ===
#define MOTOR_FRONT_LEFT_FORWARD  9
#define MOTOR_FRONT_LEFT_BACKWARD 10
#define MOTOR_FRONT_RIGHT_FORWARD 12
#define MOTOR_FRONT_RIGHT_BACKWARD 13

// === Sensor Pins ===
#define LEFT_SENSOR 35
#define MIDDLE_SENSOR 36
#define RIGHT_SENSOR 39
#define TRIG_PIN 9
#define ECHO_PIN 8

// === Bluetooth Serial ===
BluetoothSerial SerialBT;

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
#include <ESP32Servo.h>
Servo fixedServo;
Servo turnServo;

// === Sensor Data ===
int leftSensorValue = 0;
int middleSensorValue = 0;
int rightSensorValue = 0;
int ultrasonicDistance = 0;
int temperature = 0;
int humidity = 0;

// === UART Buffer & Image Handling ===
#define UART_BAUD_RATE 115200
#define MAX_IMAGE_SIZE 60000
uint8_t imageBuffer[MAX_IMAGE_SIZE];
int imageIndex = 0;
bool imageReceived = false;
bool imageStarted = false;
uint32_t expectedImageSize = 0;
unsigned long lastImageTime = 0;

// === Command Processing ===
String inputBuffer = "";
bool commandProcessing = false;

// === Utility Functions ===
void processCommand(String command, String params);
void sendResponse(String status, String message);
void sendState();
void sendCapabilities();
void captureAndSendImage(); // Now sends image from UART
void performSelfTest();
void debugPrint(const char* message);

// === Servo Control Functions ===
void moveFixedServo(int angle);
void moveTurnServo(int angle);
void setServoAngle(int angle);

// === Sensor Functions ===
int readUltrasonic();
void readIRSensors();
void readEnvironmentalSensors();

// === Motor Control Functions ===
void moveForward(int speed);
void moveBackward(int speed);
void moveLeft(int speed);
void moveRight(int speed);
void moveStop();
void moveClockwise(int speed);
void moveCounterClockwise(int speed);

// === Actuator Functions ===
void triggerShoot();
void playBuzzer(int tone, int duration);

// === UART Image Parser ===
void handleUartImage();

// === Setup Function ===
void setup() {
  // Initialize serial communication at 115200 baud
  Serial.begin(115200);
  delay(1000);
  debugPrint("ESP32 Serial started at 115200 baud");

  // Set up LED pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);

  // Set up output pins
  pinMode(SHOOT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Set up input pins
  pinMode(LEFT_SENSOR, INPUT);
  pinMode(MIDDLE_SENSOR, INPUT);
  pinMode(RIGHT_SENSOR, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize motor pins
  pinMode(MOTOR_FRONT_LEFT_FORWARD, OUTPUT);
  pinMode(MOTOR_FRONT_LEFT_BACKWARD, OUTPUT);
  pinMode(MOTOR_FRONT_RIGHT_FORWARD, OUTPUT);
  pinMode(MOTOR_FRONT_RIGHT_BACKWARD, OUTPUT);

  // Initialize servos
  fixedServo.attach(FIXED_SERVO_PIN);
  turnServo.attach(TURN_SERVO_PIN);
  moveFixedServo(90);
  moveTurnServo(90);

  // Initialize UART to receive from camera ESP32
  Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  debugPrint("UART (Serial2) initialized for camera communication");

  // Initialize Bluetooth Serial
  SerialBT.begin("ESP32-Camera-Rover");
  debugPrint("Bluetooth Serial started");
  debugPrint("Device name: ESP32-Camera-Rover");

  // Perform self-test on startup
  debugPrint("Starting system self-test...");
  performSelfTest();

  moveStop();
  debugPrint("System ready. Ready to receive Bluetooth commands.");
}

// === Main Loop ===
void loop() {
  // Handle UART image data (from camera ESP32)
  handleUartImage();

  // Handle Bluetooth client commands
  handleBluetoothClient();

  // Update sensor data
  readUltrasonic();
  readIRSensors();

  // Update LED blinking
  unsigned long currentMillis = millis();
  if (currentMillis - lastBlinkTime >= ledPacing) {
    lastBlinkTime = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }

  // Process any incoming serial commands (for debugging)
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      processInput(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += inChar;
    }
  }
}

// === Debug Function ===
void debugPrint(const char* message) {
  Serial.print(message);
  Serial.flush();
}

// === Self-Test Function ===
void performSelfTest() {
  // Test LED
  debugPrint("Testing LED...");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  debugPrint("LED test passed");

  // Test servos
  debugPrint("Testing servos...");
  moveFixedServo(0);
  delay(1000);
  moveFixedServo(90);
  delay(1000);
  moveFixedServo(180);
  delay(1000);
  moveFixedServo(90);
  debugPrint("Servo test passed");

  // Test buzzer
  debugPrint("Testing buzzer...");
  tone(BUZZER_PIN, 1000);
  delay(500);
  noTone(BUZZER_PIN);
  delay(500);
  tone(BUZZER_PIN, 500);
  delay(500);
  noTone(BUZZER_PIN);
  debugPrint("Buzzer test passed");

  // Test motors - move forward
  debugPrint("Testing motors (forward)...");
  moveForward(100);
  delay(1000);
  moveStop();
  debugPrint("Motor forward test passed");

  // Test motors - move backward
  debugPrint("Testing motors (backward)...");
  moveBackward(100);
  delay(1000);
  moveStop();
  debugPrint("Motor backward test passed");

  // Test motors - turn left
  debugPrint("Testing motors (left turn)...");
  moveLeft(100);
  delay(1000);
  moveStop();
  debugPrint("Motor left turn test passed");

  // Test motors - turn right
  debugPrint("Testing motors (right turn)...");
  moveRight(100);
  delay(1000);
  moveStop();
  debugPrint("Motor right turn test passed");

  // Test shooting mechanism
  debugPrint("Testing shooting mechanism...");
  triggerShoot();
  debugPrint("Shooting test passed");

  // Test ultrasonic sensor
  debugPrint("Testing ultrasonic sensor...");
  int distance = readUltrasonic();
  if (distance > 0 && distance < 200) {
    debugPrint("Ultrasonic sensor test passed. Distance: ");
    debugPrint(String(distance).c_str());
    debugPrint(" cm");
  } else {
    debugPrint("Ultrasonic sensor test failed. Distance: ");
    debugPrint(String(distance).c_str());
    debugPrint(" cm");
  }

  // Test IR sensors
  debugPrint("Testing IR sensors...");
  readIRSensors();
  if (leftSensorValue > 100 && middleSensorValue > 100 && rightSensorValue > 100) {
    debugPrint("IR sensors test passed");
  } else {
    debugPrint("IR sensors test failed. Values - Left: ");
    debugPrint(String(leftSensorValue).c_str());
    debugPrint(", Middle: ");
    debugPrint(String(middleSensorValue).c_str());
    debugPrint(", Right: ");
    debugPrint(String(rightSensorValue).c_str());
  }
}

// === Bluetooth Client Handling ===
void handleBluetoothClient() {
  if (SerialBT.available()) {
    char c = SerialBT.read();
    Serial.write(c); // Echo to serial for debugging

    if (c == '\n') {
      processInput(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }
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

// === Command Processing Functions ===
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

void sendResponse(String status, String message) {
  String response = status + ":" + message + "\n";
  SerialBT.print(response);
  SerialBT.flush();
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

void getStatus() {
  sendState();
}

void echo(String params) {
  sendResponse("OK", params);
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

// === Motor Control Functions ===
void move(String params) {
  if (params == "forward") {
    moveForward(180);
    sendResponse("OK", "Moving forward");
  } else if (params == "backward") {
    moveBackward(180);
    sendResponse("OK", "Moving backward");
  } else if (params == "left") {
    moveLeft(180);
    sendResponse("OK", "Turning left");
  } else if (params == "right") {
    moveRight(180);
    sendResponse("OK", "Turning right");
  } else if (params == "stop") {
    moveStop();
    sendResponse("OK", "Stopped");
  } else if (params == "clockwise") {
    moveClockwise(180);
    sendResponse("OK", "Rotating clockwise");
  } else if (params == "counter-clockwise") {
    moveCounterClockwise(180);
    sendResponse("OK", "Rotating counter-clockwise");
  } else {
    sendResponse("ERROR", "Unknown direction");
  }
}

void moveForward(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);
  debugPrint("Moving forward at speed ");
  debugPrint(String(speed).c_str());
}

void moveBackward(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, speed);
  debugPrint("Moving backward at speed ");
  debugPrint(String(speed).c_str());
}

void moveLeft(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);
  debugPrint("Turning left at speed ");
  debugPrint(String(speed).c_str());
}

void moveRight(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, speed);
  debugPrint("Turning right at speed ");
  debugPrint(String(speed).c_str());
}

void moveStop() {
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);
  debugPrint("Stopping");
}

void moveClockwise(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, speed);
  debugPrint("Rotating clockwise at speed ");
  debugPrint(String(speed).c_str());
}

void moveCounterClockwise(int speed) {
  speed = constrain(speed, 0, 255);
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);
  debugPrint("Rotating counter-clockwise at speed ");
  debugPrint(String(speed).c_str());
}

// === Servo Control Functions ===
void servo(String params) {
  int angle = params.toInt();
  if (angle >= 0 && angle <= 180) {
    servoAngle = angle;
    setServoAngle(angle);
    sendResponse("OK", "Servo angle set to " + String(angle));
  } else {
    sendResponse("ERROR", "Angle must be between 0 and 180");
  }
}

void setServoAngle(int angle) {
  moveFixedServo(angle);
  moveTurnServo(angle);
}

void moveFixedServo(int angle) {
  fixedServo.write(angle);
  debugPrint("Fixed servo moved to ");
  debugPrint(String(angle).c_str());
  debugPrint(" degrees");
}

void moveTurnServo(int angle) {
  turnServo.write(angle);
  debugPrint("Turn servo moved to ");
  debugPrint(String(angle).c_str());
  debugPrint(" degrees");
}

// === Actuator Functions ===
void shoot(String params) {
  int duration = params.toInt();
  if (duration > 0) {
    shootDuration = duration;
    triggerShoot();
    isShooting = true;
    sendResponse("OK", "Shooting triggered for " + String(duration) + "ms");
  } else {
    sendResponse("ERROR", "Invalid duration");
  }
}

void triggerShoot() {
  digitalWrite(SHOOT_PIN, HIGH);
  delay(shootDuration);
  digitalWrite(SHOOT_PIN, LOW);
  debugPrint("Shooting completed");
}

void buzzer(String params) {
  int tone = 0;
  int duration = 1000;

  int commaIndex = params.indexOf(',');
  if (commaIndex != -1) {
    tone = params.substring(0, commaIndex).toInt();
    duration = params.substring(commaIndex + 1).toInt();
  } else {
    tone = params.toInt();
  }

  if (tone > 0 && duration > 0) {
    playBuzzer(tone, duration);
    isBuzzerPlaying = true;
    sendResponse("OK", "Buzzer playing tone " + String(tone) + " for " + String(duration) + "ms");
  } else {
    sendResponse("ERROR", "Invalid tone or duration");
  }
}

void playBuzzer(int buzzerTone, int duration) {
  tone(BUZZER_PIN, buzzerTone);
  delay(duration);
  noTone(BUZZER_PIN);
  debugPrint("Buzzer played tone ");
  debugPrint(String(buzzerTone).c_str());
  debugPrint(" for ");
  debugPrint(String(duration).c_str());
  debugPrint("ms");
}

// === Camera Functions (Now UART-based) ===
void camera(String params) {
  if (params == "start") {
    sendResponse("OK", "Camera ESP32 already running");
  } else if (params == "stop") {
    sendResponse("OK", "Camera ESP32 stopped");
  } else {
    sendResponse("ERROR", "Unknown camera action");
  }
}

// === Tracking, Avoidance, Follow, Stop ===
void track(String params) {
  if (params == "1" || params == "2") {
    isTracking = true;
    isAvoiding = false;
    isFollowing = false;
    sendResponse("OK", "Tracking mode " + params + " activated");
  } else {
    sendResponse("ERROR", "Unknown tracking mode");
  }
}

void avoid(String params) {
  isAvoiding = true;
  isTracking = false;
  isFollowing = false;
  sendResponse("OK", "Obstacle avoidance mode activated");
}

void follow(String params) {
  isFollowing = true;
  isTracking = false;
  isAvoiding = false;
  sendResponse("OK", "Follow mode activated");
}

void stop() {
  moveStop();
  isTracking = false;
  isAvoiding = false;
  isFollowing = false;
  sendResponse("OK", "All operations stopped");
}

// === Sensor Functions ===
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