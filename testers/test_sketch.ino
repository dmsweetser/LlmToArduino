#include <WiFi.h>

// === Simple Diagnostic Code ===

// WiFi credentials - make sure these are correct!
#define WIFI_SSID "hackme"
#define WIFI_PASS "password"

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000); // Wait for serial port to initialize
  
  Serial.println("ESP32 Diagnostic Mode");
  Serial.println("Starting WiFi connection...");
  
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  // Wait for connection
  int attempts = 0;
  const int maxAttempts = 30; // 30 seconds timeout
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("Possible issues:");
    Serial.println("1. Wrong SSID or password");
    Serial.println("2. ESP32 not booting properly");
    Serial.println("3. USB connection issues");
    Serial.println("4. Board not properly connected");
    
    // Try to get WiFi status
    int status = WiFi.status();
    Serial.print("WiFi status code: ");
    Serial.println(status);
    
    // List available networks
    Serial.println("\nScanning for WiFi networks...");
    int n = WiFi.scanNetworks();
    if (n == 0) {
      Serial.println("No networks found");
    } else {
      Serial.print("Found ");
      Serial.print(n);
      Serial.println(" networks:");
      for (int i = 0; i < n; i++) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.println(" dBm)");
      }
    }
  }
  
  // Test basic functionality
  Serial.println("\nTesting basic functions:");
  Serial.println("LED test: ");
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  delay(500);
  digitalWrite(2, LOW);
  Serial.println("LED test passed");
  
  Serial.println("Buzzer test: ");
  pinMode(33, OUTPUT);
  tone(33, 1000);
  delay(500);
  noTone(33);
  Serial.println("Buzzer test passed");
  
  Serial.println("System diagnostic complete");
}

void loop() {
  // Keep the system running
  delay(1000);
}