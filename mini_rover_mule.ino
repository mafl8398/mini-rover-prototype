/*
 * 4WD Mini Rover Development Mule - Main Firmware
 * Hardware: Arduino Uno R4 WiFi + Dual TB6612FNG + 12V N20 Motors
 */

#include "pin_map.h"
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Wire.h>

// ==========================================
// Network Credentials & UDP Config
// ==========================================
const char* SECRET_SSID = "YOUR_WIFI_SSID";     // Change to your Wi-Fi name
const char* SECRET_PASS = "YOUR_WIFI_PASSWORD"; // Change to your Wi-Fi password

unsigned int localPort = 4210;
char packetBuffer[255]; // Buffer to hold incoming packet

WiFiUDP Udp;
unsigned long lastPacketTime = 0;     // For watchdog timer
const unsigned long TIMEOUT_MS = 500; // Stop motors if signal lost for >500ms

// ==========================================
// Function Prototypes
// ==========================================
void setMotorSpeeds(int leftSpeed, int rightSpeed);
void processDriveCommand(int throttle, int steering);

void setup() {
  // 1. Initialize Serial Communication for Debugging
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait up to 3s for USB Serial
  Serial.println(F("\n[SYSTEM] Mini Rover Initialization Starting..."));

  // 2. Configure Motor Driver Control Pins as OUTPUT
  pinMode(PIN_LEFT_DIR1, OUTPUT);
  pinMode(PIN_LEFT_PWM, OUTPUT);
  pinMode(PIN_LEFT_DIR2, OUTPUT);

  pinMode(PIN_RIGHT_PWM, OUTPUT);
  pinMode(PIN_RIGHT_DIR1, OUTPUT);
  pinMode(PIN_RIGHT_DIR2, OUTPUT);

  // Safety: Keep motor driver in STANDBY (Disabled) during startup
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

  // 4. Configure Ultrasonic Sensor Pins
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  digitalWrite(PIN_SONAR_TRIG, LOW);
  pinMode(PIN_SONAR_ECHO, INPUT);

  // 5. Initialize I2C Bus
  Wire.begin();
  Serial.println(F("[SYSTEM] Hardware Pins Configured. Motors in Safe Standby."));

  // 6. Connect to Wi-Fi with DHCP Verification
  Serial.print(F("[WIFI] Connecting to SSID: "));
  Serial.println(SECRET_SSID);
  
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  
  int attempts = 0;
  // Wait for network connection and valid DHCP lease
  while ((WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) && attempts < 30) { 
    delay(500);
    Serial.print(F("."));
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    Serial.println(F("\n[WIFI] Connected Successfully!"));
    Serial.print(F("[WIFI] Rover IP Address: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\n[WIFI] Connection or DHCP failed! Check SSID/Password or network settings."));
  }

  // 7. Start UDP Listener
  Udp.begin(localPort);
  Serial.print(F("[UDP] Listening for teleop commands on port: "));
  Serial.println(localPort);
}

void loop() {
  int packetSize = Udp.parsePacket();
  
  if (packetSize) {
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0; // Null-terminate buffer string
    }

    int throttle = 0;
    int steering = 0;
    if (sscanf(packetBuffer, "%d,%d", &throttle, &steering) == 2) {
      lastPacketTime = millis();           // Reset safety watchdog timer
      digitalWrite(PIN_MOTOR_STBY, HIGH);   // Enable motor driver
      processDriveCommand(throttle, steering);
      
      // Print incoming commands directly to Serial Monitor
      Serial.print(F("[UDP RECV] Throttle: "));
      Serial.print(throttle);
      Serial.print(F(" | Steering: "));
      Serial.println(steering);
    }
  }

  // Safety Watchdog: Shut off motors if control signal drops
  if (millis() - lastPacketTime > TIMEOUT_MS) {
    setMotorSpeeds(0, 0);
    digitalWrite(PIN_MOTOR_STBY, LOW); // Force motor driver into Standby
  }
}

// Differential Drive Kinematics (Skid-Steer Mixing)
void processDriveCommand(int throttle, int steering) {
  int leftSpeed = throttle + steering;
  int rightSpeed = throttle - steering;

  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  setMotorSpeeds(leftSpeed, rightSpeed);
}

// Low-Level Motor Driver Hardware Control
void setMotorSpeeds(int leftSpeed, int rightSpeed) {
  // --- Left Side Motors ---
  if (leftSpeed > 0) {
    digitalWrite(PIN_LEFT_DIR1, HIGH);
    digitalWrite(PIN_LEFT_DIR2, LOW);
    analogWrite(PIN_LEFT_PWM, leftSpeed);
  } else if (leftSpeed < 0) {
    digitalWrite(PIN_LEFT_DIR1, LOW);
    digitalWrite(PIN_LEFT_DIR2, HIGH);
    analogWrite(PIN_LEFT_PWM, -leftSpeed);
  } else {
    digitalWrite(PIN_LEFT_DIR1, LOW);
    digitalWrite(PIN_LEFT_DIR2, LOW);
    analogWrite(PIN_LEFT_PWM, 0);
  }

  // --- Right Side Motors ---
  if (rightSpeed > 0) {
    digitalWrite(PIN_RIGHT_DIR1, HIGH);
    digitalWrite(PIN_RIGHT_DIR2, LOW);
    analogWrite(PIN_RIGHT_PWM, rightSpeed);
  } else if (rightSpeed < 0) {
    digitalWrite(PIN_RIGHT_DIR1, LOW);
    digitalWrite(PIN_RIGHT_DIR2, HIGH);
    analogWrite(PIN_RIGHT_PWM, -rightSpeed);
  } else {
    digitalWrite(PIN_RIGHT_DIR1, LOW);
    digitalWrite(PIN_RIGHT_DIR2, LOW);
    analogWrite(PIN_RIGHT_PWM, 0);
  }
}
