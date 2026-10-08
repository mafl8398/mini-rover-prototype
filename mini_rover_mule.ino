/*
 * 4WD Mini Rover Development Mule - Main Firmware
 * Hardware: Arduino Uno R4 WiFi + Dual TB6612FNG + 12V N20 Motors
 */

#include "pin_map.h"
#include <Wire.h>

void setup() {
  // 1. Initialize Serial Communication for Telemetry / Debug
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait up to 3s for USB Serial connection
  Serial.println(F("[SYSTEM] Mini Rover Initialization Starting..."));

  // 2. Configure Motor Driver Control Pins as OUTPUT
  pinMode(PIN_LEFT_DIR1, OUTPUT);
  pinMode(PIN_LEFT_PWM, OUTPUT);
  pinMode(PIN_LEFT_DIR2, OUTPUT);

  pinMode(PIN_RIGHT_PWM, OUTPUT);
  pinMode(PIN_RIGHT_DIR1, OUTPUT);
  pinMode(PIN_RIGHT_DIR2, OUTPUT);

  // Keep motor driver in STANDBY (Disabled) during startup for safety
  pinMode(PIN_MOTOR_STBY, OUTPUT);
  digitalWrite(PIN_MOTOR_STBY, LOW);

  // 3. Configure Encoder Pins as INPUT_PULLUP
  pinMode(PIN_ENC_FL_A, INPUT_PULLUP);
  pinMode(PIN_ENC_FL_B, INPUT_PULLUP);
  pinMode(PIN_ENC_RL_A, INPUT_PULLUP);
  pinMode(PIN_ENC_RL_B, INPUT_PULLUP);
  pinMode(PIN_ENC_FR_A, INPUT_PULLUP);
  pinMode(PIN_ENC_FR_B, INPUT_PULLUP);
  pinMode(PIN_ENC_RR_A, INPUT_PULLUP);
  pinMode(PIN_ENC_RR_B, INPUT_PULLUP);

  // 4. Configure Ultrasonic Sensor Pins (RCWL-1601 / HC-SR04)
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  digitalWrite(PIN_SONAR_TRIG, LOW);
  pinMode(PIN_SONAR_ECHO, INPUT);

  // 5. Initialize Shared I2C Bus (Pins A4/A5) for MPU6050 & OLED
  Wire.begin();

  Serial.println(F("[SYSTEM] Hardware Pins Configured. Motors in Safe Standby."));
}

void loop() {
  // System Heartbeat - Prints to Serial Monitor every 2 seconds
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 2000) {
    lastHeartbeat = millis();
    Serial.println(F("[STATUS] Rover Online - Awaiting Teleop Commands..."));
  }
}
