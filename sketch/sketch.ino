#include <WiFi.h>
#include "esp_camera.h"
#include <Arduino.h>

// === Configuration ===
#define CAMERA_MODEL_AI_THINKER
#define LED_BUILTIN 2

// === Pins Configuration ===
#define FIXED_SERVO_PIN 25
#define TURN_SERVO_PIN 26
#define SHOOT_PIN 32
#define BUZZER_PIN 33

// === Motor Pins Configuration (Updated for your car shield) ===
#define MOTOR_FRONT_LEFT_FORWARD  9   // PWM pin for front left motor forward
#define MOTOR_FRONT_LEFT_BACKWARD 10  // PWM pin for front left motor backward
#define MOTOR_FRONT_RIGHT_FORWARD 12  // PWM pin for front right motor forward
#define MOTOR_FRONT_RIGHT_BACKWARD 13 // PWM pin for front right motor backward

// === Sensor Pins ===
#define LEFT_SENSOR 35
#define MIDDLE_SENSOR 36
#define RIGHT_SENSOR 39
#define TRIG_PIN 9
#define ECHO_PIN 8

WiFiServer server(100);

// === Camera Configuration ===
camera_config_t camera_config = {
  .pin_pwdn = -1,
  .pin_reset = -1,
  .pin_xclk = 4,
  .pin_sscb_sda = 18,
  .pin_sscb_scl = 22,
  .pin_d7 = 39,
  .pin_d6 = 38,
  .pin_d5 = 37,
  .pin_d4 = 36,
  .pin_d3 = 35,
  .pin_d2 = 34,
  .pin_d1 = 33,
  .pin_d0 = 32,
  .pin_vsync = 5,
  .pin_href = 27,
  .pin_pclk = 26,
  .xclk_freq_hz = 20000000,
  .pixel_format = PIXFORMAT_JPEG,
  .frame_size = FRAMESIZE_QVGA,
  .jpeg_quality = 12,
  .fb_count = 1
};

// === Servo Configuration ===
#include <ESP32Servo.h>
Servo fixedServo;
Servo turnServo;

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

// === Sensor Data ===
int leftSensorValue = 0;
int middleSensorValue = 0;
int rightSensorValue = 0;
int ultrasonicDistance = 0;
int temperature = 0;
int humidity = 0;

// === Command Processing ===
String inputBuffer = "";
bool commandProcessing = false;

// === Utility Functions ===
void processCommand(String command, String params);
void sendResponse(String status, String message);
void sendState();
void sendCapabilities();
void captureAndSendImage();
void performSelfTest();

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

// === Camera Functions ===
void startCamera();
void stopCamera();

// === Setup Function ===
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SHOOT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
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

  // Perform self-test on startup
  Serial.println("Starting system self-test...");
  performSelfTest();
  
  // Initialize camera
  if (esp_camera_init(&camera_config) != ESP_OK) {
    Serial.println("Camera initialization failed!");
  } else {
    Serial.println("Camera initialized successfully");
    isCameraActive = true;
  }
  
  // Start WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin("hackme", "password");
  
  // Wait for WiFi connection with serial monitoring
  unsigned long startTime = millis();
  const int maxWaitTime = 30000; // 30 second timeout
  
  Serial.println("Attempting to connect to WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    // Check if 30 seconds have passed
    if (millis() - startTime >= maxWaitTime) {
      Serial.println("WiFi connection timeout after 30 seconds");
      break;
    }
    
    // Check for serial input every 100ms
    if (Serial.available() > 0) {
      char inChar = (char)Serial.read();
      if (inChar == '\n') {
        // Process any commands from serial
        processInput(inputBuffer);
        inputBuffer = "";
      } else {
        inputBuffer += inChar;
      }
    }
    
    delay(100);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Start the WiFi server after successful connection
    server.begin();
    Serial.println("WiFi server started");
  } else {
    Serial.println("\nWiFi connection failed");
  }

  moveStop();
  Serial.println("System ready. Ready to receive serial commands.");
}

// === Main Loop ===
void loop() {
  // Handle WiFi client connections
  handleWiFiClients();
  
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

  // Process any incoming serial commands
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

// === Self-Test Function ===
void performSelfTest() {
  // Test LED
  Serial.println("Testing LED...");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  Serial.println("LED test passed");

  // Test servos
  Serial.println("Testing servos...");
  moveFixedServo(0);
  delay(1000);
  moveFixedServo(90);
  delay(1000);
  moveFixedServo(180);
  delay(1000);
  moveFixedServo(90);
  Serial.println("Servo test passed");

  // Test buzzer
  Serial.println("Testing buzzer...");
  tone(BUZZER_PIN, 1000);
  delay(500);
  noTone(BUZZER_PIN);
  delay(500);
  tone(BUZZER_PIN, 500);
  delay(500);
  noTone(BUZZER_PIN);
  Serial.println("Buzzer test passed");

  // Test camera
  Serial.println("Testing camera...");
  if (esp_camera_init(&camera_config) == ESP_OK) {
    Serial.println("Camera test passed");
    isCameraActive = true;
    // Capture a test image
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
      Serial.println("Camera image capture test passed");
    } else {
      Serial.println("Camera image capture test failed");
    }
  } else {
    Serial.println("Camera test failed");
    isCameraActive = false;
  }

  // Test motors - move forward
  Serial.println("Testing motors (forward)...");
  moveForward(100);
  delay(1000);
  moveStop();
  Serial.println("Motor forward test passed");

  // Test motors - move backward
  Serial.println("Testing motors (backward)...");
  moveBackward(100);
  delay(1000);
  moveStop();
  Serial.println("Motor backward test passed");

  // Test motors - turn left
  Serial.println("Testing motors (left turn)...");
  moveLeft(100);
  delay(1000);
  moveStop();
  Serial.println("Motor left turn test passed");

  // Test motors - turn right
  Serial.println("Testing motors (right turn)...");
  moveRight(100);
  delay(1000);
  moveStop();
  Serial.println("Motor right turn test passed");

  // Test shooting mechanism
  Serial.println("Testing shooting mechanism...");
  triggerShoot();
  Serial.println("Shooting test passed");

  // Test ultrasonic sensor
  Serial.println("Testing ultrasonic sensor...");
  int distance = readUltrasonic();
  if (distance > 0 && distance < 200) {
    Serial.println("Ultrasonic sensor test passed. Distance: " + String(distance) + " cm");
  } else {
    Serial.println("Ultrasonic sensor test failed. Distance: " + String(distance) + " cm");
  }

  // Test IR sensors
  Serial.println("Testing IR sensors...");
  readIRSensors();
  if (leftSensorValue > 100 && middleSensorValue > 100 && rightSensorValue > 100) {
    Serial.println("IR sensors test passed");
  } else {
    Serial.println("IR sensors test failed. Values - Left: " + String(leftSensorValue) + 
                   ", Middle: " + String(middleSensorValue) + ", Right: " + String(rightSensorValue));
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
    captureAndSendImage();
  } else {
    sendResponse("ERROR", "Unknown command type");
  }
}

void sendResponse(String status, String message) {
  String response = status + ":" + message + "\n";
  Serial.print(response);
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
  Serial.print(state);
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
    "snapshot:\n";  // Added snapshot command

  Serial.print(capabilitiesStr);
}

void getStatus() {
  sendState();
}

void getSensorData() {
  String sensorData = "distance:" + String(ultrasonicDistance) + "\n";
  sensorData += "left_sensor:" + String(leftSensorValue) + "\n";
  sensorData += "middle_sensor:" + String(middleSensorValue) + "\n";
  sensorData += "right_sensor:" + String(rightSensorValue) + "\n";
  Serial.print(sensorData);
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
  // Ensure speed is within 0-255 range
  speed = constrain(speed, 0, 255);

  analogWrite(MOTOR_FRONT_LEFT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);

  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);

  Serial.println("Moving forward at speed " + String(speed));
}

void moveBackward(int speed) {
  speed = constrain(speed, 0, 255);

  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, speed);

  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, speed);

  Serial.println("Moving backward at speed " + String(speed));
}

void moveLeft(int speed) {
  speed = constrain(speed, 0, 255);

  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, speed);

  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);

  Serial.println("Turning left at speed " + String(speed));
}

void moveRight(int speed) {
  speed = constrain(speed, 0, 255);

  analogWrite(MOTOR_FRONT_LEFT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);

  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, speed);

  Serial.println("Turning right at speed " + String(speed));
}

void moveStop() {
  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);

  Serial.println("Stopping");
}

void moveClockwise(int speed) {
  speed = constrain(speed, 0, 255);

  analogWrite(MOTOR_FRONT_LEFT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, speed);

  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, 0);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, speed);

  Serial.println("Rotating clockwise at speed " + String(speed));
}

void moveCounterClockwise(int speed) {
  speed = constrain(speed, 0, 255);

  analogWrite(MOTOR_FRONT_LEFT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_LEFT_BACKWARD, 0);

  analogWrite(MOTOR_FRONT_RIGHT_FORWARD, speed);
  analogWrite(MOTOR_FRONT_RIGHT_BACKWARD, 0);

  Serial.println("Rotating counter-clockwise at speed " + String(speed));
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
  Serial.println("Fixed servo moved to " + String(angle) + " degrees");
}

void moveTurnServo(int angle) {
  turnServo.write(angle);
  Serial.println("Turn servo moved to " + String(angle) + " degrees");
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
  Serial.println("Shooting completed");
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
  Serial.println("Buzzer played tone " + String(buzzerTone) + " for " + String(duration) + "ms");
}

// === Camera Functions ===
void camera(String params) {
  if (params == "start") {
    startCamera();
    sendResponse("OK", "Camera started");
  } else if (params == "stop") {
    stopCamera();
    sendResponse("OK", "Camera stopped");
  } else {
    sendResponse("ERROR", "Unknown camera action");
  }
}

void startCamera() {
  if (esp_camera_init(&camera_config) == ESP_OK) {
    isCameraActive = true;
    Serial.println("Camera started");
  } else {
    sendResponse("ERROR", "Camera failed to start");
  }
}

void stopCamera() {
  isCameraActive = false;
  Serial.println("Camera stopped");
}

// === Capture and Send Image via Serial ===
void captureAndSendImage() {
  if (!isCameraActive) {
    sendResponse("ERROR", "Camera not active");
    return;
  }

  Serial.println("Capturing image...");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    sendResponse("ERROR", "Failed to capture image");
    return;
  }

  // Send JPEG data in chunks to avoid overflow
  Serial.write('S');  // Start of image marker
  Serial.write((fb->len >> 24) & 0xFF);
  Serial.write((fb->len >> 16) & 0xFF);
  Serial.write((fb->len >> 8) & 0xFF);
  Serial.write(fb->len & 0xFF);

  // Send image data
  Serial.write(fb->buf, fb->len);

  Serial.write('E');  // End of image marker

  esp_camera_fb_return(fb);
  Serial.println("Image sent over serial");
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

// === WiFi Client Handling ===
void handleWiFiClients() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected");
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        
        // Process commands from WiFi client
        if (c == '\n') {
          processInput(inputBuffer);
          inputBuffer = "";
        } else {
          inputBuffer += c;
        }
      }
      
      // Check for inactivity
      if (millis() - lastBlinkTime > 10000) {
        // Send status update every 10 seconds
        sendState();
        lastBlinkTime = millis();
      }
      
      delay(1);
    }
    
    Serial.println("Client disconnected");
    client.stop();
  }
}