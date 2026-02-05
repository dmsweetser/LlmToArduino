#include "Arduino.h"
#include "esp_camera.h"
#include <BluetoothSerial.h>

String commandBuffer = "";

// === Camera Pin Mapping (ESP32-CAM) ===
#define PWDN    -1
#define RESET   -1
#define XCLK    4
#define SIO_D0  18
#define SIO_D1  23
#define SIO_D2  3
#define SIO_D3  5
#define SIO_D4  17
#define SIO_D5  16
#define SIO_D6  2
#define SIO_D7  0
#define VSYNC   12
#define HREF    26
#define PCLK    27

// === Bluetooth Serial ===
BluetoothSerial SerialBT;

// === Camera Configuration ===
camera_config_t config = {
  .pin_pwdn = PWDN,
  .pin_reset = RESET,
  .pin_xclk = XCLK,
  .pin_sscb_sda = SIO_D0,
  .pin_sscb_scl = SIO_D1,
  .pin_d7 = SIO_D2,
  .pin_d6 = SIO_D3,
  .pin_d5 = SIO_D4,
  .pin_d4 = SIO_D5,
  .pin_d3 = SIO_D6,
  .pin_d2 = SIO_D7,
  .pin_d1 = 2,
  .pin_d0 = 0,
  .pin_vsync = VSYNC,
  .pin_href = HREF,
  .pin_pclk = PCLK,
  .xclk_freq_hz = 20000000,
  .pixel_format = PIXFORMAT_JPEG,
  .frame_size = FRAMESIZE_QVGA,
  .jpeg_quality = 12,
  .fb_count = 1
};

// === State Flags ===
bool isCapturing = false;
bool isStreaming = false;
unsigned long captureStartTime = 0;
const int CAPTURE_TIMEOUT = 5000;
const int STREAM_TIMEOUT = 30000;

// === Debug Print Helper ===
void debugPrint(const char* message) {
  Serial.print(message);
  Serial.flush();
}

// === Send Response Over Bluetooth ===
void sendResponse(String status, String message) {
  String response = status + ":" + message + "\n";
  SerialBT.print(response);
  SerialBT.flush();
}

// === Process Command ===
void processCommand(String cmd, String params) {
  if (cmd == "getCapabilities") {
    String caps =
      "snapshot:\n"
      "stream:start\n"
      "stream:stop\n"
      "getStatus:\n";
    SerialBT.print(caps);
  } else if (cmd == "getStatus") {
    String caps =
      "is_streaming:" + String(isStreaming);
    sendResponse("OK", caps);
  } else if (cmd == "snapshot") {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      // Send image size (4 bytes, big-endian)
      uint32_t size = fb->len;
      SerialBT.write((size >> 24) & 0xFF);
      SerialBT.write((size >> 16) & 0xFF);
      SerialBT.write((size >> 8) & 0xFF);
      SerialBT.write(size & 0xFF);

      // Send image data
      SerialBT.write(fb->buf, fb->len);

      // Send end marker
      SerialBT.write('E');

      // Release buffer
      esp_camera_fb_return(fb);

      debugPrint("Snapshot sent\n");
    } else {
      debugPrint("Failed to get camera frame\n");
    }
  } else if (cmd == "stream") {
    if (params == "start") {
      isStreaming = true;
      captureStartTime = millis();
      debugPrint("Streaming started\n");
      sendResponse("OK", "Streaming started");
    } else if (params == "stop") {
      isStreaming = false;
      debugPrint("Streaming stopped\n");
      sendResponse("OK", "Streaming stopped");
    } else {
      sendResponse("ERROR", "Invalid stream parameter");
    }
  } else {
    sendResponse("ERROR", "Unknown command: " + cmd);
  }
}

// === Capture and Stream Images ===
void captureAndStream() {
  if (isStreaming) {
    if (millis() - captureStartTime > STREAM_TIMEOUT) {
      debugPrint("Streaming timeout reached\n");
      isStreaming = false;
      return;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      // Send image size (4 bytes, big-endian)
      uint32_t size = fb->len;
      SerialBT.write((size >> 24) & 0xFF);
      SerialBT.write((size >> 16) & 0xFF);
      SerialBT.write((size >> 8) & 0xFF);
      SerialBT.write(size & 0xFF);

      // Send image data
      SerialBT.write(fb->buf, fb->len);

      // Send end marker
      SerialBT.write('E');

      // Release buffer
      esp_camera_fb_return(fb);

      debugPrint("Stream frame sent\n");
      captureStartTime = millis(); // Reset timeout
    } else {
      debugPrint("Failed to get camera frame\n");
      delay(100);
    }
  }
}

// === Main Setup ===
void setup() {
  // Initialize camera
  if (esp_camera_init(&config) != ESP_OK) {
    debugPrint("Camera init failed!\n");
    while (1) delay(1000);
  }

  // Set resolution
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  // Initialize Bluetooth
  SerialBT.begin("ESP32-Camera");
  debugPrint("Bluetooth Serial started\n");
  debugPrint("Camera ready. Waiting for commands...\n");
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

  // Capture and stream if needed
  captureAndStream();

  delay(10);
}