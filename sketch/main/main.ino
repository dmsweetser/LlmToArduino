#include <BluetoothSerial.h>
#include <Arduino.h>
#include <ESP32Servo.h>

String commandBuffer = "";

// === Motor Pins ===
const int M1_Forward = 128;
const int M1_Backward = 64;
const int M2_Forward = 32;
const int M2_Backward = 16;
const int M3_Forward = 2;
const int M3_Backward = 4;
const int M4_Forward = 1;
const int M4_Backward = 8;

// === Bluetooth Serial ===
BluetoothSerial SerialBT;

// === Servo Control ===
Servo cameraServo;
const int SERVO_PIN = 26;

// === Line Tracking & Ultrasonic ===
const int LINE_TRACKING_PIN = 34;
const int ULTRASONIC_TRIG = 35;
const int ULTRASONIC_ECHO = 36;

// === LED & Blinking ===
const int LED_BUILTIN = 2;
bool ledState = false;
int ledPacing = 1000;
unsigned long lastBlinkTime = 0;

// === Script Engine ===
String scriptBuffer = "";
int scriptIndex = 0;
bool scriptRunning = false;

// === Labels ===
struct Label {
  String name;
  int line;
};
Label labels[10];
int labelCount = 0;

// === Debug Print Helper ===
void debugPrint(const char* message) {
  Serial.print(message);
  Serial.flush();
}

// === Motor Control ===
void Move(int Dir, int Speed) {
  digitalWrite(16, LOW);
  analogWrite(19, Speed);
  digitalWrite(17, LOW);
  shiftOut(5, 18, MSBFIRST, Dir);
  digitalWrite(17, HIGH);
}

// === Send Response Over Bluetooth ===
void sendResponse(String status, String message) {
  String response = status + ":" + message + "\n";
  SerialBT.print(response);
  SerialBT.flush();
}

// === Process Command ===
void processCommand(String cmd, String params) {
  if (cmd == "forward") {
    int speed = params.toInt();
    Move(M4_Forward + M3_Forward + M2_Forward + M1_Forward, speed);
    sendResponse("OK", "forward: " + params);
  } else if (cmd == "backward") {
    int speed = params.toInt();
    Move(M4_Backward + M3_Backward + M2_Backward + M1_Backward, speed);
    sendResponse("OK", "backward: " + params);
  } else if (cmd == "stop") {
    Move(0, 0);
    sendResponse("OK", "stop");
  } else if (cmd == "getCapabilities") {
    String caps =
      "forward:speed\n"
      "backward:speed\n"
      "stop:\n"
      "getStatus:\n"
      "getCapabilities:\n"
      "echo:message\n"
      "setLED:pacing\n"
      "servo:angle\n"
      "delay:ms\n"
      "run:script\n"
      "label:name\n"
      "goto:name\n"
      "lineTracking:enable\n"
      "ultrasonic:read\n";
    SerialBT.print(caps);
  } else if (cmd == "getStatus") {
    String caps =
      "led_pacing:" + String(ledPacing) + ", "
      "servo_angle:" + String(cameraServo.read()) + ", "
      "line_tracking:" + String(digitalRead(LINE_TRACKING_PIN)) + ", "
      "ultrasonic:" + String(getUltrasonicDistance());
    sendResponse("OK", caps);
  } else if (cmd == "echo") {
    sendResponse("OK", params);
  } else if (cmd == "setLED") {
    int pace = params.toInt();
    if (pace > 0) {
      ledPacing = pace;
      sendResponse("OK", "LED pacing set to " + String(ledPacing));
    } else {
      sendResponse("ERROR", "Invalid pacing value");
    }
  } else if (cmd == "servo") {
    int angle = params.toInt();
    if (angle >= 0 && angle <= 180) {
      cameraServo.write(angle);
      sendResponse("OK", "servo: " + String(angle));
    } else {
      sendResponse("ERROR", "Angle must be 0-180");
    }
  } else if (cmd == "delay") {
    int ms = params.toInt();
    if (ms > 0) {
      debugPrint("Delaying for ");
      debugPrint(String(ms).c_str());
      debugPrint(" ms\n");
      delay(ms);
      sendResponse("OK", "delay: " + params);
    } else {
      sendResponse("ERROR", "Invalid delay value");
    }
  } else if (cmd == "run") {
    scriptBuffer = params;
    scriptRunning = true;
    scriptIndex = 0;
    labelCount = 0;
    debugPrint("Script loaded. Starting execution.\n");
    sendResponse("OK", "Script loaded and running");
  } else if (cmd == "lineTracking") {
    if (params == "enable") {
      pinMode(LINE_TRACKING_PIN, INPUT);
      sendResponse("OK", "Line tracking enabled");
    } else if (params == "disable") {
      pinMode(LINE_TRACKING_PIN, INPUT_PULLUP);
      sendResponse("OK", "Line tracking disabled");
    } else {
      sendResponse("ERROR", "Invalid line tracking parameter");
    }
  } else if (cmd == "ultrasonic") {
    if (params == "read") {
      float distance = getUltrasonicDistance();
      sendResponse("OK", "ultrasonic:" + String(distance));
    } else {
      sendResponse("ERROR", "Invalid ultrasonic parameter");
    }
  } else {
    sendResponse("ERROR", "Unknown command: " + cmd);
  }
}

// === Blink LED ===
void blinkLED() {
  unsigned long now = millis();
  if (now - lastBlinkTime >= ledPacing) {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
    lastBlinkTime = now;
  }
}

// === Ultrasonic Distance Measurement ===
float getUltrasonicDistance() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);

  unsigned long duration = pulseIn(ULTRASONIC_ECHO, HIGH);
  float distance = duration * 0.034 / 2;
  return distance;
}

// === Parse and Execute One Script Line ===
void parseCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  int colon = line.indexOf(':');
  if (colon == -1) {
    if (line.startsWith("goto:")) {
      String labelName = line.substring(4);
      labelName.trim();
      for (int i = 0; i < labelCount; i++) {
        if (labels[i].name == labelName) {
          scriptIndex = labels[i].line;
          debugPrint("GOTO to ");
          debugPrint(labelName.c_str());
          debugPrint("\n");
          return;
        }
      }
      debugPrint("Label not found: ");
      debugPrint(labelName.c_str());
      debugPrint("\n");
    }
    return;
  }

  String cmd = line.substring(0, colon);
  String params = line.substring(colon + 1);

  if (cmd == "label") {
    String labelName = params;
    labelName.trim();
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
    processCommand(cmd, params);
  }
}

// === Execute Script Line-by-Line ===
void executeScript() {
  if (scriptBuffer.length() == 0) {
    scriptRunning = false;
    return;
  }

  int start = 0;
  int end = 0;
  while (end < scriptBuffer.length()) {
    if (scriptBuffer[end] == '\n' || end == scriptBuffer.length() - 1) {
      String line = scriptBuffer.substring(start, end + 1);
      parseCommand(line);
      start = end + 1;
      if (end == scriptBuffer.length() - 1) break;
    }
    end++;
  }

  scriptRunning = false;
  debugPrint("🏁 Script execution complete.\n");
}

// === Main Setup ===
void setup() {
  // Motor Pins
  pinMode(18, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(17, OUTPUT);
  pinMode(19, OUTPUT);

  // LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);

  // Servo
  cameraServo.attach(SERVO_PIN);
  cameraServo.write(90);

  // Line Tracking
  pinMode(LINE_TRACKING_PIN, INPUT);

  // Ultrasonic
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);

  // Bluetooth
  SerialBT.begin("ESP32-Car");
  debugPrint("Bluetooth Serial started\n");
  debugPrint("ESP32-Car: Ready.\n");
  debugPrint("Send: getCapabilities to see all commands.\n");
}

// === Main Loop ===
void loop() {
  // Handle Bluetooth input
  if (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n') {
      int colon = commandBuffer.indexOf(':');
      if (colon != -1) {
        String cmd = commandBuffer.substring(0, colon);
        String params = commandBuffer.substring(colon + 1);
        processCommand(cmd, params);
      } else {
        sendResponse("ERROR", "Invalid format: missing ':'");
      }
      commandBuffer = "";
    } else {
      commandBuffer += c;
    }
  }

  // Blink LED
  blinkLED();

  // If script is running, execute it
  if (scriptRunning) {
    executeScript();
  }
}