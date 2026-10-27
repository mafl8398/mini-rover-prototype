/*
================================================================================
  URC 2027 Mini-Rover Firmware (v6.1 - Unified GP-20U7 GPS & SH1107 Fix)
================================================================================
*/

#include <Wire.h>               // I2C communication library
#include <WiFiS3.h>             // Uno R4 WiFi core library
#include <WiFiUdp.h>            // UDP networking
#include <Adafruit_GFX.h>       // Core graphics drawing library
#include <Adafruit_SH110X.h>    // Fixed SH1107 OLED driver
#include <Adafruit_NeoPixel.h>  // RGB LED driver
#include <Adafruit_MPU6050.h>   // IMU driver
#include <Adafruit_Sensor.h>    // Unified sensor formatting

#include "pin_map.h"            // Custom hardware definitions

// ================================================================================
// WI-FI & UDP TELEOPERATION CONFIGURATION
// ================================================================================
char ssid[] = "Linksys Blue";      // Home router network name
char pass[] = "MF114363";          // Home router password
unsigned int localPort = 4210;     // UDP Port to listen for commands

WiFiUDP Udp;
char packetBuffer[255];

// ================================================================================
// C2 Telemetry Tracking
// ================================================================================
unsigned long lastSerialTelemetry = 0;
long lastCountFL = 0; // Tracks previous encoder count to calculate speed
const float WHEEL_CIRC_METERS = 0.2042; // 65mm diameter * Pi
const float ENCODER_PPR = 330.0; // UPDATE THIS to match your specific motor's encoder resolution

// ================================================================================
// URC LED MODES & HARDWARE INSTANCES
// ================================================================================
enum UrcLedMode {
  MODE_TELEOP,   // Solid Blue
  MODE_AUTO,     // Solid Red
  MODE_ARRIVED   // Flashing Green
};

UrcLedMode currentLedMode = MODE_TELEOP;

Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire1); // Using Qwiic Bus
Adafruit_MPU6050 mpu;

bool oledConnected = false;
bool mpuConnected = false;
bool gpsFix = false;
int oledScreenPage = 0; // Tracks the current screen page (0 to 4)

// GPS Variables
String gpsLatitude = "Searching...";
String gpsLongitude = "Searching...";
int gpsSatellites = 0;
String nmeaBuffer = ""; 

// Encoder Counters (volatile for hardware interrupts)
volatile long countFL = 0, countRL = 0, countFR = 0, countRR = 0;

// Non-blocking Timers
unsigned long lastOledUpdate = 0;
unsigned long lastGreenFlash = 0;
bool greenFlashState = false;

// Driver Telemetry
int currentThrottle = 0;
int currentSteering = 0;

// Interrupt Service Routines for Wheel Encoders
void ISR_FL() { countFL++; }
void ISR_RL() { countRL++; }
void ISR_FR() { countFR++; }
void ISR_RR() { countRR++; }

// Function Declarations
void readGpsData();
void parseNmeaSentence(String sentence);
void handleUdpPackets();
void handleSerialCommands();
void handleOledButton();
void updateUrcLed();
void executeMotorDrive(int throttle, int steering);
void stopMotors();
void renderOledTelemetry();

// ================================================================================
// SETUP
// ================================================================================
void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println(F("\n=============================================="));
  Serial.println(F(" URC MINI-ROVER FIRMWARE V6.1 (UNIFIED GPS)   "));
  Serial.println(F("=============================================="));

  Serial1.begin(GPS_BAUD_RATE); // Start Hardware Serial1 for GP-20U7 GPS

  // Configure Motor Pins
  pinMode(PIN_MOTOR_L_IN1, OUTPUT);
  pinMode(PIN_MOTOR_L_PWM, OUTPUT);
  pinMode(PIN_MOTOR_L_IN2, OUTPUT);
  pinMode(PIN_MOTOR_R_PWM, OUTPUT);
  pinMode(PIN_MOTOR_R_IN1, OUTPUT);
  pinMode(PIN_MOTOR_R_IN2, OUTPUT);
  pinMode(PIN_MOTOR_STBY, OUTPUT);

  digitalWrite(PIN_MOTOR_STBY, LOW); // Start safely disabled

  // Configure Sonar
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  pinMode(PIN_SONAR_ECHO, INPUT);

  // Configure single OLED Button
  pinMode(PIN_OLED_BTN_A, INPUT_PULLUP);

  // Configure Encoders
  pinMode(PIN_ENC_FL, INPUT_PULLUP);
  pinMode(PIN_ENC_RL, INPUT_PULLUP);
  pinMode(PIN_ENC_FR, INPUT_PULLUP);
  pinMode(PIN_ENC_RR, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_FL), ISR_FL, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RL), ISR_RL, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_FR), ISR_FR, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RR), ISR_RR, RISING);

  // Initialize LED
  pixel.begin();
  pixel.setBrightness(50);
  updateUrcLed();

  // Initialize SH1107 OLED
  Wire1.begin();
  if (display.begin(0x3C, true)) {
    oledConnected = true;
    display.setRotation(1); // Fixes orientation bug
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE); 
    display.setCursor(0, 0);
    display.println(F("URC MINI-ROVER v6.1"));
    display.println(F("OLED: SH1107 Wire1 OK"));
    display.display();
  }

  // Initialize MPU6050
  Wire.begin();
  if (mpu.begin()) {
    mpuConnected = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  // Connect to Home Wi-Fi
  Serial.print(F("[WIFI] Connecting to Home Router: "));
  Serial.println(ssid);
  WiFi.begin(ssid, pass); 
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  
  Udp.begin(localPort); 
  Serial.println(F("\n[WIFI] Connected to Home Network!"));
  Serial.print(F(" -> ROVER IP ADDR:  ")); Serial.println(WiFi.localIP());
}

// ================================================================================
// MAIN LOOP
// ================================================================================
void loop() {
  readGpsData();          // Parse NMEA GPS strings
  handleUdpPackets();     // Check for network driving commands
  handleSerialCommands(); // Check for computer keyboard input
  handleOledButton();     // Check physical button press
  updateUrcLed();         // Refresh NeoPixel

  // 150ms non-blocking OLED refresh
  if (oledConnected && (millis() - lastOledUpdate > 150)) {
    lastOledUpdate = millis();
    renderOledTelemetry();
  }
  if (millis() - lastSerialTelemetry > 1000) {
    lastSerialTelemetry = millis();
    printC2Telemetry();  
  }
}

// ================================================================================
// GP-20U7 NMEA GPS PARSER
// ================================================================================
void readGpsData() {
  while (Serial1.available()) { 
    char c = Serial1.read();    
    if (c == '\n') {            
      parseNmeaSentence(nmeaBuffer); 
      nmeaBuffer = "";          
    } else if (c != '\r') {     
      nmeaBuffer += c;          
    }
  }
}

void parseNmeaSentence(String sentence) {
  if (sentence.startsWith("$GPGGA") || sentence.startsWith("$GNGGA")) {
    int count = 0;
    String tokens[15]; 
    
    for (int i = 0; i < sentence.length(); i++) {
      if (sentence.charAt(i) == ',') {
        count++;
      } else {
        if (count < 15) tokens[count] += sentence.charAt(i);
      }
    }

    if (tokens[6].toInt() > 0) { // Fix quality check
      gpsFix = true;
      gpsLatitude = tokens[2] + " " + tokens[3]; 
      gpsLongitude = tokens[4] + " " + tokens[5]; 
      gpsSatellites = tokens[7].toInt();         
    } else {
      gpsFix = false;
      gpsLatitude = "No Fix";
      gpsLongitude = "No Fix";
      gpsSatellites = tokens[7].toInt();
    }
  }
}

// ================================================================================
// URC LED MODE HANDLER (OPTIMIZED)
// ================================================================================
void updateUrcLed() {
  static int lastRenderedMode = -1;

  switch (currentLedMode) {
    case MODE_ARRIVED:
      if (millis() - lastGreenFlash > 250) { 
        lastGreenFlash = millis();
        greenFlashState = !greenFlashState; 
        pixel.setPixelColor(0, greenFlashState ? pixel.Color(0, 255, 0) : pixel.Color(0, 0, 0));
        pixel.show();
      }
      lastRenderedMode = MODE_ARRIVED;
      break;

    case MODE_TELEOP:
      if (currentLedMode != lastRenderedMode) {
        pixel.setPixelColor(0, pixel.Color(0, 0, 255));
        pixel.show();
        lastRenderedMode = MODE_TELEOP;
      }
      break;

    case MODE_AUTO:
      if (currentLedMode != lastRenderedMode) {
        pixel.setPixelColor(0, pixel.Color(255, 0, 0));
        pixel.show();
        lastRenderedMode = MODE_AUTO;
      }
      break;
  }
}

// ================================================================================
// OLED BUTTON HANDLING
// ================================================================================
void handleOledButton() {
  static bool lastBtnA = HIGH;
  bool btnA = digitalRead(PIN_OLED_BTN_A);

  // Advance to the next screen page if pressed (0 -> 1 -> 2 -> 3 -> 4 -> 0)
  if (btnA == LOW && lastBtnA == HIGH) {
    oledScreenPage = (oledScreenPage + 1) % 5; 
  }
  
  lastBtnA = btnA; 
}

// ================================================================================
// UDP & SERIAL PACKET PARSERS
// ================================================================================
void handleUdpPackets() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;

    String pktStr = String(packetBuffer);
    pktStr.trim();

    if (pktStr == "MODE:TELEOP" || pktStr == "BTN:X") {
      currentLedMode = MODE_TELEOP;
      digitalWrite(PIN_MOTOR_STBY, HIGH);
    } 
    else if (pktStr == "MODE:AUTO" || pktStr == "BTN:B") {
      currentLedMode = MODE_AUTO;
      digitalWrite(PIN_MOTOR_STBY, HIGH);
    } 
    else if (pktStr == "MODE:ARRIVED" || pktStr == "BTN:A") {
      currentLedMode = MODE_ARRIVED;
      digitalWrite(PIN_MOTOR_STBY, LOW);
      stopMotors();
    } 
    else if (pktStr.indexOf(',') > 0) {
      int commaIndex = pktStr.indexOf(',');
      currentThrottle = pktStr.substring(0, commaIndex).toInt();
      currentSteering = pktStr.substring(commaIndex + 1).toInt();
      executeMotorDrive(currentThrottle, currentSteering);
    }
  }
}

// ================================================================================
// SERIAL COMMAND PARSER (BASIC KEYBOARD CONTROLS)
// ================================================================================
void handleSerialCommands() {
  if (Serial.available()) {
    char c = Serial.read();
    
    // LED Status Modes (Changed 'a' to 'q' to free up the 'a' key for turning)
    if (c == 'x' || c == 'X') currentLedMode = MODE_TELEOP;
    else if (c == 'b' || c == 'B') currentLedMode = MODE_AUTO;
    else if (c == 'q' || c == 'Q') currentLedMode = MODE_ARRIVED;
    
    // Basic WASD Driving Commands (Instant on/off)
    else if (c == 'w' || c == 'W') executeMotorDrive(255, 0);  // Full Forward
    else if (c == 's' || c == 'S') executeMotorDrive(-255, 0); // Full Reverse
    else if (c == 'a' || c == 'A') executeMotorDrive(0, -255); // Spin Left
    else if (c == 'd' || c == 'D') executeMotorDrive(0, 255);  // Spin Right
    else if (c == ' ') stopMotors();                           // Spacebar to stop
  }
}

// ================================================================================
// MOTOR DRIVE CONTROL
// ================================================================================
void executeMotorDrive(int throttle, int steering) {
  int leftSpeed = constrain(throttle + steering, -255, 255);
  int rightSpeed = constrain(throttle - steering, -255, 255);

  digitalWrite(PIN_MOTOR_L_IN1, leftSpeed >= 0 ? HIGH : LOW);
  digitalWrite(PIN_MOTOR_L_IN2, leftSpeed >= 0 ? LOW : HIGH);
  analogWrite(PIN_MOTOR_L_PWM, abs(leftSpeed));

  digitalWrite(PIN_MOTOR_R_IN1, rightSpeed >= 0 ? HIGH : LOW);
  digitalWrite(PIN_MOTOR_R_IN2, rightSpeed >= 0 ? LOW : HIGH);
  analogWrite(PIN_MOTOR_R_PWM, abs(rightSpeed));
}

void stopMotors() {
  analogWrite(PIN_MOTOR_L_PWM, 0);
  analogWrite(PIN_MOTOR_R_PWM, 0);
}

// ================================================================================
// OLED RENDERING
// ================================================================================
void renderOledTelemetry() {
  display.clearDisplay();
  display.setTextSize(1);

  float rawA1 = analogRead(PIN_BATTERY_SENSE);
  float batteryVoltage = ((rawA1 / 1023.0) * 5.0) * (13.3 / 3.3);

  switch (oledScreenPage) {
    case 0:
      display.setCursor(0, 0);  display.println(F("=== POWER & NETWORK ==="));
      display.setCursor(0, 14); display.print(F("IP:  ")); display.println(WiFi.localIP());
      display.setCursor(0, 26); display.print(F("Bat: ")); display.print(batteryVoltage, 2); display.println(F(" V"));
      break;

    case 1:
      display.setCursor(0, 0); display.println(F("=== MPU6050 MOTION ==="));
      if (mpuConnected) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        display.setCursor(0, 16);
        display.print(F("Accel X: ")); display.print(a.acceleration.x, 1); display.println(F(" m/s2"));
      } else {
        display.setCursor(0, 24); display.println(F("IMU Status: N/C"));
      }
      break;

    case 2:
      display.setCursor(0, 0); display.println(F("=== WHEEL ENCODERS ==="));
      display.setCursor(0, 16);
      display.print(F("FL: ")); display.print(countFL); display.print(F("  FR: ")); display.println(countFR);
      display.print(F("RL: ")); display.print(countRL); display.print(F("  RR: ")); display.println(countRR);
      break;

    case 3:
      display.setCursor(0, 0); display.println(F("=== HC-SR04 SONAR ==="));
      break;

    case 4:
      display.setCursor(0, 0); display.println(F("=== GP-20U7 GPS NAV ==="));
      display.setCursor(0, 14); display.print(F("Fix:  ")); display.println(gpsFix ? F("3D FIX OK") : F("SEARCHING"));
      display.setCursor(0, 26); display.print(F("Lat:  ")); display.println(gpsLatitude);
      display.setCursor(0, 38); display.print(F("Lon:  ")); display.println(gpsLongitude);
      display.setCursor(0, 50); display.print(F("Sats: ")); display.println(gpsSatellites);
      break;
  }

  display.drawFastHLine(0, 54, 128, SH110X_WHITE);
  display.setCursor(0, 56);
  display.print(F("P")); display.print(oledScreenPage + 1); display.print(F("/5 | Btn A: Next Page"));
  display.display();
}

// ================================================================================
// C2 SERIAL TELEMETRY STREAM
// ================================================================================
void printC2Telemetry() {
  // 1. Calculate Wheel Speed (Front Left Wheel)
  long currentCountFL = countFL; 
  long ticksThisSecond = currentCountFL - lastCountFL;
  lastCountFL = currentCountFL;
  
  // Speed = (Revolutions per second) * Circumference
  float speedMs = (ticksThisSecond / ENCODER_PPR) * WHEEL_CIRC_METERS;

  // 2. Read Battery
  float rawA1 = analogRead(PIN_BATTERY_SENSE);
  float batteryVoltage = ((rawA1 / 1023.0) * 5.0) * (13.3 / 3.3);

  // 3. Translate Enum to String for readability
  String modeStr;
  if (currentLedMode == MODE_TELEOP) modeStr = "TELEOP (Blue)";
  else if (currentLedMode == MODE_AUTO) modeStr = "AUTONOMOUS (Red)";
  else modeStr = "ARRIVED (Green)";

  // 4. Print Data Block
  Serial.println(F("\n================ C2 STATION TELEMETRY ================"));
  Serial.print(F("SYS MODE:    ")); Serial.println(modeStr);
  Serial.print(F("POWER:       ")); Serial.print(batteryVoltage, 2); Serial.println(F(" V"));
  Serial.print(F("SPEED (FL):  ")); Serial.print(speedMs, 2); Serial.println(F(" m/s"));
  Serial.print(F("MOTORS:      Thr: ")); Serial.print(currentThrottle); Serial.print(F(" | Str: ")); Serial.println(currentSteering);
  
  // GPS Data
  Serial.print(F("GPS FIX:     ")); Serial.println(gpsFix ? F("ACTIVE 3D FIX") : F("SEARCHING..."));
  Serial.print(F("GPS COORDS:  ")); Serial.print(gpsLatitude); Serial.print(F(", ")); Serial.println(gpsLongitude);
  Serial.print(F("SATELLITES:  ")); Serial.println(gpsSatellites);
  Serial.println(F("======================================================"));
}
