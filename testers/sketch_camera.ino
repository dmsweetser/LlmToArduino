#include <esp_camera.h>
#include <Arduino.h>

// === Camera Model ===
#define CAMERA_MODEL_AI_THINKER

// === Camera Pin Configuration ===
// These pins are for the AI-Thinker ESP32-CAM module (OV3660)
camera_config_t camera_config = {
  .pin_pwdn    = -1,
  .pin_reset   = -1,
  .pin_xclk    = 4,
  .pin_sscb_sda = 18,
  .pin_sscb_scl = 22,
  .pin_d7      = 39,
  .pin_d6      = 38,
  .pin_d5      = 37,
  .pin_d4      = 36,
  .pin_d3      = 35,
  .pin_d2      = 34,
  .pin_d1      = 33,
  .pin_d0      = 32,
  .pin_vsync   = 5,
  .pin_href    = 27,
  .pin_pclk    = 26,
  .xclk_freq_hz = 20000000,
  .pixel_format = PIXFORMAT_JPEG,
  .frame_size = FRAMESIZE_QVGA,  // 320x240
  .jpeg_quality = 12,
  .fb_count = 1
};

// === UART Pins ===
#define UART_TX_PIN 13  // Connected to RX on main ESP32
#define UART_RX_PIN 14  // Connected to TX on main ESP32

// === Buffer for UART communication ===
#define UART_BAUD_RATE 115200
#define MAX_IMAGE_SIZE 60000  // Max ~60KB JPEG (adjust if needed)

// === Global Variables ===
bool imageCaptureRequested = false;
unsigned long lastImageSent = 0;
const unsigned long imageSendDelay = 500; // Prevent flooding

void setup() {
  // Initialize Serial for debugging (optional)
  Serial.begin(115200);
  delay(1000);
  Serial.println("Camera ESP32: Booted and ready");

  // Initialize UART for communication with main ESP32
  Serial2.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  // Initialize camera
  if (esp_camera_init(&camera_config) != ESP_OK) {
    Serial.println("Camera initialization failed!");
    while (1) delay(1000);
  }
  Serial.println("Camera initialized successfully");

  // Set frame size and quality (optional tuning)
  // (Already set in camera_config)

  Serial.println("Waiting for image capture commands via UART...");
}

void loop() {
  // Check if a command was received via UART
  if (Serial2.available() > 0) {
    char c = Serial2.read();

    // Look for a command marker (e.g., 'C' to capture image)
    if (c == 'C') {
      Serial.println("Capture command received");
      imageCaptureRequested = true;
    }
  }

  // Process image capture if requested
  if (imageCaptureRequested) {
    // Avoid sending images too frequently
    if (millis() - lastImageSent >= imageSendDelay) {
      captureAndSendImage();
      lastImageSent = millis();
      imageCaptureRequested = false; // Reset
    }
  }

  // Optional: keep loop responsive
  delay(10);
}

// === Capture and Send Image Over UART ===
void captureAndSendImage() {
  Serial.println("Capturing image...");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get camera frame");
    return;
  }

  Serial.println("Sending image data...");

  // Send image start marker
  Serial2.write('S');
  // Send image length (4 bytes: big-endian)
  Serial2.write((fb->len >> 24) & 0xFF);
  Serial2.write((fb->len >> 16) & 0xFF);
  Serial2.write((fb->len >> 8) & 0xFF);
  Serial2.write(fb->len & 0xFF);

  // Send image data in chunks to avoid buffer overflow
  const int chunkSize = 128;
  int sent = 0;
  while (sent < fb->len) {
    int toSend = min((size_t)chunkSize, fb->len - sent);
    Serial2.write(fb->buf + sent, toSend);
    sent += toSend;
    delay(1); // Small delay to prevent UART buffer overflow
  }

  // Send end marker
  Serial2.write('E');

  Serial.println("Image sent over UART");

  // Return frame buffer
  esp_camera_fb_return(fb);
}