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
unsigned long lastPacketTime = 0; // For watchdog timer
const unsigned long TIMEOUT_MS = 500; // Stop motors if signal lost for >500ms

// ==========================================
// Function Prototypes
// ==========================================
void setMotorSpeeds(int leftSpeed, int rightSpeed);
void processDriveCommand(int throttle, int steering);

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

  // 6. Connect to Wi-Fi
  Serial.print(F("[WIFI] Connecting to "));
  Serial.println(SECRET_SSID);
  
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  
  Serial.println(F(" Connected!"));
  Serial.print(F("[WIFI] Rover IP Address: "));
  Serial.println(WiFi.localIP());

  // 7. Start UDP Listener
  Udp.begin(localPort);
  Serial.println(F("[UDP] Listening for Xbox Teleop Commands..."));
}

void loop() {
  int packetSize = Udp.parsePacket();
  
  if (packetSize) {
    // Read incoming packet
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0; // Null-terminate string
    }

    // Parse "throttle,steering" format (e.g. "180,-45")
    int throttle = 0;
    int steering = 0;
    if (sscanf(packetBuffer, "%d,%d", &throttle, &steering) == 2) {
      lastPacketTime = millis(); // Refresh watchdog timer
      digitalWrite(PIN_MOTOR_STBY, HIGH); // Enable motor driver
      processDriveCommand(throttle, steering);
    }
  }

  // Safety Watchdog: Shut off motors if control signal drops
  if (millis() - lastPacketTime > TIMEOUT_MS) {
    setMotorSpeeds(0, 0);
    digitalWrite(PIN_MOTOR_STBY, LOW); // Disable driver outputs
  }
}

// Differential Drive Kinematics (Skid-Steer Mixing)
void processDriveCommand(int throttle, int steering) {
  int leftSpeed = throttle + steering;
  int rightSpeed = throttle - steering;

  // Constrain PWM outputs between -255 and 255
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
