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
#define LED_MODULE1 2
#define LED_MODULE2 12

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
bool isLED1On = false;
bool isLED2On = false;
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
void updateLEDs();
void captureAndSendImage();

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
void toggleLED1();
void toggleLED2();
void setLED1(bool state);
void setLED2(bool state);

// === Camera Functions ===
void startCamera();
void stopCamera();

// === Setup Function ===
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SHOOT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_MODULE1, OUTPUT);
  pinMode(LED_MODULE2, OUTPUT);
  pinMode(LEFT_SENSOR, INPUT);
  pinMode(MIDDLE_SENSOR, INPUT);
  pinMode(RIGHT_SENSOR, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize servos
  fixedServo.attach(FIXED_SERVO_PIN);
  turnServo.attach(TURN_SERVO_PIN);
  moveFixedServo(90);
  moveTurnServo(90);

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
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());


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
  moveStop();
  Serial.println("Moving forward at speed " + String(speed));
}

void moveBackward(int speed) {
  moveStop();
  Serial.println("Moving backward at speed " + String(speed));
}

void moveLeft(int speed) {
  moveStop();
  Serial.println("Turning left at speed " + String(speed));
}

void moveRight(int speed) {
  moveStop();
  Serial.println("Turning right at speed " + String(speed));
}

void moveStop() {
  Serial.println("Stopping");
}

void moveClockwise(int speed) {
  Serial.println("Rotating clockwise at speed " + String(speed));
}

void moveCounterClockwise(int speed) {
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
