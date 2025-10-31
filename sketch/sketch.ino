#include"Arduino.h"
#include <BluetoothSerial.h>

const int M1_Forward        = 128;
const int M1_Backward       = 64;
const int M2_Forward        = 32;
const int M2_Backward       = 16;
const int M3_Forward        = 2;
const int M3_Backward       = 4;
const int M4_Forward        = 1;
const int M4_Backward       = 8;

BluetoothSerial SerialBT;

void setup() {
  pinMode(18, OUTPUT);    // SHCP_PIN
  pinMode(16, OUTPUT);    // EN_PIN
  pinMode(5, OUTPUT);     // DATA_PIN
  pinMode(17, OUTPUT);    // STCP_PIN
  pinMode(19, OUTPUT);    // PWM1_PIN

  SerialBT.begin("ESP32-LED-Test");
}

//Define the motor pins and speed of the expansion board
void Move(int Dir, int Speed) 
{
  digitalWrite(16, LOW);    // EN_PIN
  analogWrite(19, Speed);   // PWM1_PIN

  digitalWrite(17, LOW);    // STCP_PIN
  shiftOut(5, 18, MSBFIRST, Dir);   // DATA_PIN, SHCP_PIN, MSBFIRST, Dir
  digitalWrite(17, HIGH);   // STCP_PIN
}

void loop() {

    if (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == 'f') {
      Move(M4_Forward + M3_Forward + M2_Forward + M1_Forward,255);   //Motor forward rotation
      SerialBT.println("forward");
    } else if (c == 'b') {
      Move(M4_Backward + M3_Backward + M2_Backward + M1_Backward,-255);   //Motor reverse rotation
      SerialBT.println("backward");
    } else if (c == 's') {
      Move(0,-255);   //Stop
      SerialBT.println("stop");
    }
  }  
}