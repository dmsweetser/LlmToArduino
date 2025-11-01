#include "Arduino.h"
#include "esp_camera.h"
#include "WiFi.h"

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

// === UART for Communication (to main ESP32) ===
// HardwareSerial Serial2(2); // RX: GPIO14, TX: GPIO13

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
unsigned long captureStartTime = 0;

// === Setup Function ===
void setup() {
  // Start serial for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("Camera ESP32: Starting...");

  // Initialize camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
    while (1) delay(1000);
  }

  // Set resolution (optional)
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA); // 320x240

  // Start UART2 for communication with main ESP32
  Serial2.begin(115200, SERIAL_8N1, 14, 13); // RX:14, TX:13
  Serial.println("Camera ready. Waiting for commands...");

  // Turn off LED (optional)
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
}

// === Main Loop ===
void loop() {
  // Read command from main ESP32 (via Serial2)
  if (Serial2.available() > 0) {
    char cmd = Serial2.read();

    if (cmd == 'C') {
      // Start image capture
      isCapturing = true;
      captureStartTime = millis();
      Serial.println("Capture requested...");
      digitalWrite(13, LOW); // Turn on LED to indicate capture
    } else if (cmd == 'S') {
      // Stop capture
      isCapturing = false;
      digitalWrite(13, HIGH); // Turn off LED
      Serial.println("Capture stopped");
    }
  }

  // Capture image if requested
  if (isCapturing) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      // Send image size (4 bytes, big-endian)
      uint32_t size = fb->len;
      Serial2.write((size >> 24) & 0xFF);
      Serial2.write((size >> 16) & 0xFF);
      Serial2.write((size >> 8) & 0xFF);
      Serial2.write(size & 0xFF);

      // Send image data
      Serial2.write(fb->buf, fb->len);

      // Send end marker
      Serial2.write('E');

      // Release buffer
      esp_camera_fb_return(fb);

      Serial.println("Image sent!");
      isCapturing = false;
      digitalWrite(13, HIGH); // Turn off LED
    } else {
      Serial.println("Failed to get camera frame");
    }
  }

  delay(10); // Small delay
}