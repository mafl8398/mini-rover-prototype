/*
char ssid[] = "Linksys Blue";       // Wi-Fi SSID
char pass[] = "MF114363";          // Wi-Fi Password
unsigned int localPort = 4210;     // UDP Port to listen on

================================================================================
  URC 2027 Mini-Rover Firmware (v6 - SH1107 OLED & NeoPixel Timing Fix)
  ================================================================================
  Target Architecture: Arduino Uno R4 WiFi
  
  Updates in v6:
    - Updated OLED driver to Adafruit_SH1107 to eliminate display static/noise.
    - Added display.setRotation(1) for proper 128x64 landscape orientation.
    - Updated display macros to SH110X_WHITE.
    - Optimized updateUrcLed() state tracking to eliminate constant pixel.show()
      calls, preventing UDP packet drops and encoder interrupt misses.
  ================================================================================
*/

#include <Wire.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include "pin_map.h"

// ================================================================================
// WI-FI & UDP TELEOPERATION CONFIGURATION
// ================================================================================
char ssid[] = "Linksys Blue";       // Wi-Fi SSID
char pass[] = "MF114363";          // Wi-Fi Password
unsigned int localPort = 4210;     // UDP Port to listen on

WiFiUDP Udp;
char packetBuffer[255];
IPAddress roverIP;

// ================================================================================
// URC LED MODES & HARDWARE INSTANCES
// ================================================================================
enum UrcLedMode {
  MODE_TELEOP,   // Solid Blue (Manually driving)
  MODE_AUTO,     // Solid Red (Autonomous operation)
  MODE_ARRIVED   // Flashing Green (Target arrived)
};

UrcLedMode currentLedMode = MODE_TELEOP;

// Hardware Drivers
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Using Wire1 for Arduino Uno R4 WiFi Qwiic STEMMA QT Connector with SH1107 OLED
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire1);
Adafruit_MPU6050 mpu;

bool oledConnected = false;
bool mpuConnected = false;
int oledScreenPage = 0; // 0=Power/Network, 1=IMU, 2=Motors/Encoders, 3=Sonar

// Encoder Pulse Counters
volatile long countFL = 0, countRL = 0, countFR = 0, countRR = 0;

// Non-blocking Timing Variables
unsigned long lastOledUpdate = 0;
unsigned long lastGreenFlash = 0;
bool greenFlashState = false;

// Driver Telemetry
int currentThrottle = 0;
int currentSteering = 0;

// Interrupt Service Routines
void ISR_FL() { countFL++; }
void ISR_RL() { countRL++; }
void ISR_FR() { countFR++; }
void ISR_RR() { countRR++; }

// Function Declarations
void handleUdpPackets();
void handleSerialCommands();
void handleOledButtons();
void updateUrcLed();
void executeMotorDrive(int throttle, int steering);
void stopMotors();
void renderOledTelemetry();

// ================================================================================
// SETUP
// ================================================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Give Serial Monitor time to connect

  Serial.println(F("\n=============================================="));
  Serial.println(F("   URC MINI-ROVER FIRMWARE V6 (SH1107 FIX)    "));
  Serial.println(F("=============================================="));

  // Initialize Pin Modes
  pinMode(PIN_MOTOR_L_IN1, OUTPUT);
  pinMode(PIN_MOTOR_L_PWM, OUTPUT);
  pinMode(PIN_MOTOR_L_IN2, OUTPUT);
  pinMode(PIN_MOTOR_R_PWM, OUTPUT);
  pinMode(PIN_MOTOR_R_IN1, OUTPUT);
  pinMode(PIN_MOTOR_R_IN2, OUTPUT);
  pinMode(PIN_MOTOR_STBY, OUTPUT);

  digitalWrite(PIN_MOTOR_STBY, LOW); // Drivers in Standby

  pinMode(PIN_SONAR_TRIG, OUTPUT);
  pinMode(PIN_SONAR_ECHO, INPUT);

  pinMode(PIN_OLED_BTN_A, INPUT_PULLUP);
  pinMode(PIN_OLED_BTN_B, INPUT_PULLUP);
  pinMode(PIN_OLED_BTN_C, INPUT_PULLUP);

  // Encoder Interrupts
  pinMode(PIN_ENC_FL, INPUT_PULLUP);
  pinMode(PIN_ENC_RL, INPUT_PULLUP);
  pinMode(PIN_ENC_FR, INPUT_PULLUP);
  pinMode(PIN_ENC_RR, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_FL), ISR_FL, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RL), ISR_RL, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_FR), ISR_FR, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RR), ISR_RR, RISING);

  // Initialize NeoPixel
  pixel.begin();
  pixel.setBrightness(50);
  updateUrcLed();

  // Initialize Wire1 (Uno R4 WiFi Qwiic Bus)
  Wire1.begin();

  // Initialize SH1107 OLED on Wire1 (Address 0x3C)
  if (display.begin(0x3C, true)) {
    oledConnected = true;
    display.setRotation(1); // Set to 128x64 landscape orientation
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("URC MINI-ROVER v6"));
    display.println(F("OLED: SH1107 Wire1 OK"));
    display.println(F("Initializing..."));
    display.display();
    Serial.println(F("[OLED] SH1107 connected successfully on Qwiic (&Wire1) @ 0x3C"));
  } else {
    Serial.println(F("[OLED WARNING] SH1107 display not found on Wire1 0x3C!"));
  }

  // Initialize MPU6050 on standard Wire bus (Pins A4/A5)
  Wire.begin();
  if (mpu.begin()) {
    mpuConnected = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println(F("[IMU] MPU6050 Connected on Wire (A4/A5)"));
  } else {
    Serial.println(F("[IMU WARNING] MPU6050 not detected on A4/A5"));
  }

  // Connect to existing Home Wi-Fi Router
  Serial.print(F("[WIFI] Connecting to Home Router: "));
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  
  Udp.begin(localPort);
  Serial.println(F("\n[WIFI] Connected to Home Network!"));
  Serial.print(F(" -> ROVER IP ADDR:  ")); Serial.println(WiFi.localIP());
  Serial.print(F(" -> UDP PORT:       ")); Serial.println(localPort);

  Serial.println(F("=============================================="));
  Serial.println(F("Ready! Use Python script or Serial ('X','B','A')."));
}

// ================================================================================
// MAIN LOOP
// ================================================================================
void loop() {
  handleUdpPackets();
  handleSerialCommands();
  handleOledButtons();
  updateUrcLed();

  // Refresh OLED telemetry every 150ms if display detected
  if (oledConnected && (millis() - lastOledUpdate > 150)) {
    lastOledUpdate = millis();
    renderOledTelemetry();
  }
}

// ================================================================================
// URC LED MODE HANDLER (STATE-CHANGE OPTIMIZED)
// ================================================================================
void updateUrcLed() {
  static int lastRenderedMode = -1;

  if (currentLedMode == MODE_ARRIVED) {
    if (millis() - lastGreenFlash > 250) { // Flashing Green
      lastGreenFlash = millis();
      greenFlashState = !greenFlashState;
      pixel.setPixelColor(0, greenFlashState ? pixel.Color(0, 255, 0) : pixel.Color(0, 0, 0));
      pixel.show();
    }
    lastRenderedMode = MODE_ARRIVED;
  } 
  else if (currentLedMode != lastRenderedMode) {
    if (currentLedMode == MODE_TELEOP) {
      pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Solid Blue
    } else if (currentLedMode == MODE_AUTO) {
      pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Solid Red
    }
    pixel.show();
    lastRenderedMode = currentLedMode;
  }
}

// ================================================================================
// UDP PACKET PARSER
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
// SERIAL COMMAND PARSER
// ================================================================================
void handleSerialCommands() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'x' || c == 'X') {
      currentLedMode = MODE_TELEOP;
      Serial.println(F("[MODE] Switched to Teleop Mode (Solid Blue)"));
    } else if (c == 'b' || c == 'B') {
      currentLedMode = MODE_AUTO;
      Serial.println(F("[MODE] Switched to Autonomous Mode (Solid Red)"));
    } else if (c == 'a' || c == 'A') {
      currentLedMode = MODE_ARRIVED;
      Serial.println(F("[MODE] Target Arrival Triggered (Flashing Green)"));
    }
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
// OLED BUTTON HANDLING
// ================================================================================
void handleOledButtons() {
  static bool lastBtnA = HIGH, lastBtnB = HIGH, lastBtnC = HIGH;
  bool btnA = digitalRead(PIN_OLED_BTN_A);
  bool btnB = digitalRead(PIN_OLED_BTN_B);
  bool btnC = digitalRead(PIN_OLED_BTN_C);

  if (btnA == LOW && lastBtnA == HIGH) oledScreenPage = (oledScreenPage + 3) % 4;
  if (btnB == LOW && lastBtnB == HIGH) oledScreenPage = (oledScreenPage + 1) % 4;
  if (btnC == LOW && lastBtnC == HIGH) {
    if (currentLedMode == MODE_TELEOP) currentLedMode = MODE_AUTO;
    else if (currentLedMode == MODE_AUTO) currentLedMode = MODE_ARRIVED;
    else currentLedMode = MODE_TELEOP;
  }

  lastBtnA = btnA; lastBtnB = btnB; lastBtnC = btnC;
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
      display.setCursor(0, 38); display.print(F("LED: "));
      if (currentLedMode == MODE_TELEOP) display.println(F("BLUE (TELEOP)"));
      else if (currentLedMode == MODE_AUTO) display.println(F("RED (AUTO)"));
      else display.println(F("FLASH GREEN (ARRIVED)"));
      break;

    case 1:
      display.setCursor(0, 0); display.println(F("=== MPU6050 MOTION ==="));
      if (mpuConnected) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        display.setCursor(0, 16);
        display.print(F("Accel X: ")); display.print(a.acceleration.x, 1); display.println(F(" m/s2"));
        display.print(F("Accel Y: ")); display.print(a.acceleration.y, 1); display.println(F(" m/s2"));
        display.print(F("Gyro  Z: ")); display.print(g.gyro.z, 1); display.println(F(" rad/s"));
      } else {
        display.setCursor(0, 24); display.println(F("IMU Status: N/C"));
      }
      break;

    case 2:
      display.setCursor(0, 0); display.println(F("=== WHEEL ENCODERS ==="));
      display.setCursor(0, 16);
      display.print(F("FL: ")); display.print(countFL); display.print(F("  FR: ")); display.println(countFR);
      display.print(F("RL: ")); display.print(countRL); display.print(F("  RR: ")); display.println(countRR);
      display.setCursor(0, 40);
      display.print(F("STBY Pin: ")); display.println(digitalRead(PIN_MOTOR_STBY) ? "ENABLED" : "STANDBY");
      break;

    case 3:
      display.setCursor(0, 0); display.println(F("=== HC-SR04 SONAR ==="));
      display.setCursor(0, 24); display.println(F("Sonar: Pins A2/A3 Configured"));
      display.println(F("Ready for sensor plug-in"));
      break;
  }

  display.drawFastHLine(0, 54, 128, SH110X_WHITE);
  display.setCursor(0, 56);
  display.print(F("P")); display.print(oledScreenPage + 1); display.print(F("/4 | Btn A/B:Nav C:Mode"));
  display.display();
}
