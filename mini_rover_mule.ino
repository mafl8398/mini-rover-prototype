/*
================================================================================
  URC 2027 Mini-Rover Firmware (v6 - SH1107 OLED & NeoPixel Timing Fix)
  Target Architecture: Arduino Uno R4 WiFi
  
  Updates:
    - Implemented switch-case LED handler with static state tracking.
    - Preserved non-blocking millis() logic to prevent dropped UDP packets.
    - Fully commented for educational reference and hardware debugging.
================================================================================
*/

#include <Wire.h>               // I2C communication library (used for sensors/displays)
#include <WiFiS3.h>             // Core Wi-Fi library for the Arduino Uno R4
#include <WiFiUdp.h>            // Enables sending and receiving UDP packets
#include <Adafruit_GFX.h>       // Core graphics drawing library for OLED
#include <Adafruit_SH110X.h>    // Specific driver for the SH1107 OLED screen
#include <Adafruit_NeoPixel.h>  // Library to control the WS2812 RGB LED
#include <Adafruit_MPU6050.h>   // Library for the gyroscope/accelerometer
#include <Adafruit_Sensor.h>    // Unified sensor formatting for Adafruit libraries

#include "pin_map.h"            // Custom header containing your specific pin numbers

// ================================================================================
// WI-FI & UDP TELEOPERATION CONFIGURATION
// ================================================================================
char ssid[] = "Linksys Blue";      // Your local Wi-Fi network name
char pass[] = "MF114363";          // Your local Wi-Fi password
unsigned int localPort = 4210;     // The UDP port the rover listens to for commands

WiFiUDP Udp;                       // Creates a UDP instance to handle incoming network data
char packetBuffer[255];            // Array to store incoming network messages
IPAddress roverIP;                 // Stores the IP address assigned by the router

// ================================================================================
// URC LED MODES & HARDWARE INSTANCES
// ================================================================================
// Enums give readable names to states instead of just using random numbers (0, 1, 2)
enum UrcLedMode {
  MODE_TELEOP,   // State 0: Solid Blue (Manual driving)
  MODE_AUTO,     // State 1: Solid Red (Autonomous navigation)
  MODE_ARRIVED   // State 2: Flashing Green (Target reached)
};

UrcLedMode currentLedMode = MODE_TELEOP; // Set the default startup state

// Initialize the NeoPixel (1 LED, on the defined pin, using standard GRB 800KHz data)
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Initialize the OLED screen. 64x128 resolution, connected to the Qwiic bus (&Wire1)
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire1);

// Initialize the IMU (Inertial Measurement Unit)
Adafruit_MPU6050 mpu;

// System status flags used to prevent the code from crashing if hardware unplugs
bool oledConnected = false;
bool mpuConnected = false;
int oledScreenPage = 0; // Tracks which telemetry page to show (0 to 3)

// 'volatile' tells the compiler these variables change inside hardware interrupts.
// This prevents the compiler from optimizing them out or caching old values.
volatile long countFL = 0, countRL = 0, countFR = 0, countRR = 0;

// Non-blocking timers. We use millis() instead of delay() so the main loop never stops running.
unsigned long lastOledUpdate = 0;
unsigned long lastGreenFlash = 0;
bool greenFlashState = false; // Tracks if the green LED is currently on or off during flashing

// Global variables to store the current driving commands
int currentThrottle = 0;
int currentSteering = 0;

// Interrupt Service Routines (ISRs) for the wheel encoders.
// These run instantly in the background every time a wheel magnet passes the sensor.
void ISR_FL() { countFL++; }
void ISR_RL() { countRL++; }
void ISR_FR() { countFR++; }
void ISR_RR() { countRR++; }

// Function Declarations (Tells the compiler these functions exist further down in the file)
void handleUdpPackets();
void handleSerialCommands();
void handleOledButtons();
void updateUrcLed();
void executeMotorDrive(int throttle, int steering);
void stopMotors();
void renderOledTelemetry();

// ================================================================================
// SETUP - Runs once when the Arduino turns on
// ================================================================================
void setup() {
  Serial.begin(115200); // Start the USB serial connection for debugging on the computer
  delay(1000); // Brief pause to allow the Serial Monitor to connect

  Serial.println(F("\n=============================================="));
  Serial.println(F("   URC MINI-ROVER FIRMWARE V6 (SH1107 FIX)    "));
  Serial.println(F("=============================================="));

  // Configure all motor control pins as outputs
  pinMode(PIN_MOTOR_L_IN1, OUTPUT);
  pinMode(PIN_MOTOR_L_PWM, OUTPUT);
  pinMode(PIN_MOTOR_L_IN2, OUTPUT);
  pinMode(PIN_MOTOR_R_PWM, OUTPUT);
  pinMode(PIN_MOTOR_R_IN1, OUTPUT);
  pinMode(PIN_MOTOR_R_IN2, OUTPUT);
  pinMode(PIN_MOTOR_STBY, OUTPUT);

  // Keep the standby pin LOW initially so motors don't spin uncontrollably on startup
  digitalWrite(PIN_MOTOR_STBY, LOW); 

  // Configure the ultrasonic sonar pins
  pinMode(PIN_SONAR_TRIG, OUTPUT); // Trigger sends the sound pulse
  pinMode(PIN_SONAR_ECHO, INPUT);  // Echo listens for the sound bouncing back

  // Configure OLED buttons with internal pull-up resistors (HIGH when unpressed, LOW when pressed)
  pinMode(PIN_OLED_BTN_A, INPUT_PULLUP);
  pinMode(PIN_OLED_BTN_B, INPUT_PULLUP);
  pinMode(PIN_OLED_BTN_C, INPUT_PULLUP);

  // Configure encoder pins and attach their hardware interrupts
  // 'RISING' means the interrupt triggers when the signal goes from LOW to HIGH
  pinMode(PIN_ENC_FL, INPUT_PULLUP);
  pinMode(PIN_ENC_RL, INPUT_PULLUP);
  pinMode(PIN_ENC_FR, INPUT_PULLUP);
  pinMode(PIN_ENC_RR, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_FL), ISR_FL, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RL), ISR_RL, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_FR), ISR_FR, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RR), ISR_RR, RISING);

  // Start the NeoPixel and set its initial color based on 'currentLedMode'
  pixel.begin();
  pixel.setBrightness(50); // Set brightness to ~20% so it's not blinding
  updateUrcLed();

  // Initialize the secondary I2C bus used by the Uno R4's Qwiic connector
  Wire1.begin();

  // Attempt to connect to the OLED at I2C address 0x3C
  if (display.begin(0x3C, true)) {
    oledConnected = true;
    display.setRotation(1); // Force landscape orientation (fixes the SH1107 bug)
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE); 
    display.setCursor(0, 0);
    display.println(F("URC MINI-ROVER v6"));
    display.println(F("OLED: SH1107 Wire1 OK"));
    display.println(F("Initializing..."));
    display.display(); // Push the drawing buffer to the actual screen
    Serial.println(F("[OLED] SH1107 connected successfully on Qwiic (&Wire1) @ 0x3C"));
  } else {
    Serial.println(F("[OLED WARNING] SH1107 display not found on Wire1 0x3C!"));
  }

  // Initialize the primary I2C bus used for the IMU
  Wire.begin();
  if (mpu.begin()) {
    mpuConnected = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G); // Set G-force limits
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);      // Set rotation speed limits
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);   // Apply hardware smoothing to sensor data
    Serial.println(F("[IMU] MPU6050 Connected on Wire (A4/A5)"));
  } else {
    Serial.println(F("[IMU WARNING] MPU6050 not detected on A4/A5"));
  }

  // Connect to the Wi-Fi router
  Serial.print(F("[WIFI] Connecting to Home Router: "));
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass); 
  
  // Wait in a loop until the router assigns an IP address
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  
  // Open the UDP port to listen for incoming commands from the Python script
  Udp.begin(localPort); 
  Serial.println(F("\n[WIFI] Connected to Home Network!"));
  Serial.print(F(" -> ROVER IP ADDR:  ")); Serial.println(WiFi.localIP()); // Print the IP needed for Python
  Serial.print(F(" -> UDP PORT:       ")); Serial.println(localPort);

  Serial.println(F("=============================================="));
  Serial.println(F("Ready! Use Python script or Serial ('X','B','A')."));
}

// ================================================================================
// MAIN LOOP - Runs repeatedly, thousands of times per second
// ================================================================================
void loop() {
  handleUdpPackets();     // 1. Check for new network commands
  handleSerialCommands(); // 2. Check for manual keyboard typing in Serial Monitor
  handleOledButtons();    // 3. Check if physical buttons were pressed
  updateUrcLed();         // 4. Update the NeoPixel status color

  // 5. Update the OLED screen, but ONLY every 150 milliseconds.
  // Drawing to the OLED is slow. If we did this every loop, the rover would lag 
  // and miss UDP driving packets.
  if (oledConnected && (millis() - lastOledUpdate > 150)) {
    lastOledUpdate = millis(); // Reset the timer
    renderOledTelemetry();
  }
}

// ================================================================================
// URC LED MODE HANDLER (SWITCH CASE + STATE-CHANGE OPTIMIZED)
// ================================================================================
void updateUrcLed() {
  // 'static' means this variable remembers its value between function calls.
  // It allows us to know what color the LED is currently showing without asking the LED.
  static int lastRenderedMode = -1;

  switch (currentLedMode) {
    case MODE_ARRIVED:
      // In ARRIVED mode, we must evaluate time continuously to create a flashing effect.
      if (millis() - lastGreenFlash > 250) { // Toggle every 250ms
        lastGreenFlash = millis();
        greenFlashState = !greenFlashState; // Flip the state (True->False, False->True)
        // If state is true, set to Green. If false, set to Off (0,0,0)
        pixel.setPixelColor(0, greenFlashState ? pixel.Color(0, 255, 0) : pixel.Color(0, 0, 0));
        pixel.show();
      }
      lastRenderedMode = MODE_ARRIVED;
      break;

    case MODE_TELEOP:
      // Optimization: Only push data to the hardware if the mode JUST changed.
      // Calling pixel.show() repeatedly wastes processing power and delays the main loop.
      if (currentLedMode != lastRenderedMode) {
        pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Solid Blue
        pixel.show();
        lastRenderedMode = MODE_TELEOP;
      }
      break;

    case MODE_AUTO:
      if (currentLedMode != lastRenderedMode) {
        pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Solid Red
        pixel.show();
        lastRenderedMode = MODE_AUTO;
      }
      break;
  }
}

// ================================================================================
// UDP PACKET PARSER
// ================================================================================
void handleUdpPackets() {
  int packetSize = Udp.parsePacket(); // Check if a network packet arrived
  if (packetSize) {
    int len = Udp.read(packetBuffer, 255); // Read up to 255 bytes into the buffer
    if (len > 0) packetBuffer[len] = 0;    // Add a null terminator to make it a valid String

    String pktStr = String(packetBuffer);
    pktStr.trim(); // Remove any invisible whitespace or newline characters

    // Parse specific string commands sent by the controller
    if (pktStr == "MODE:TELEOP" || pktStr == "BTN:X") {
      currentLedMode = MODE_TELEOP;
      digitalWrite(PIN_MOTOR_STBY, HIGH); // Enable the motor drivers
    } 
    else if (pktStr == "MODE:AUTO" || pktStr == "BTN:B") {
      currentLedMode = MODE_AUTO;
      digitalWrite(PIN_MOTOR_STBY, HIGH); // Enable the motor drivers
    } 
    else if (pktStr == "MODE:ARRIVED" || pktStr == "BTN:A") {
      currentLedMode = MODE_ARRIVED;
      digitalWrite(PIN_MOTOR_STBY, LOW);  // HARDWARE SAFETY: Disable motor drivers entirely
      stopMotors(); // Set PWM signals to 0
    } 
    // If the string contains a comma, treat it as a "throttle,steering" drive command
    else if (pktStr.indexOf(',') > 0) {
      int commaIndex = pktStr.indexOf(',');
      // Split the string at the comma and convert both halves to integers
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
  // Allows you to test the modes via the Arduino IDE Serial Monitor without Wi-Fi
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
// MOTOR DRIVE CONTROL (Arcade/Differential Mix)
// ================================================================================
void executeMotorDrive(int throttle, int steering) {
  // Mix throttle and steering, capping the final speeds between -255 and 255
  int leftSpeed = constrain(throttle + steering, -255, 255);
  int rightSpeed = constrain(throttle - steering, -255, 255);

  // Left Motor Direction: If speed is positive, set IN1 HIGH, IN2 LOW. Otherwise, reverse it.
  digitalWrite(PIN_MOTOR_L_IN1, leftSpeed >= 0 ? HIGH : LOW);
  digitalWrite(PIN_MOTOR_L_IN2, leftSpeed >= 0 ? LOW : HIGH);
  // Send PWM speed. abs() removes negative signs because analogWrite only takes 0-255.
  analogWrite(PIN_MOTOR_L_PWM, abs(leftSpeed));

  // Right Motor Direction
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
  // 'static' stores the previous state of the buttons.
  // This lets us detect the moment a button is *pressed*, rather than held down.
  static bool lastBtnA = HIGH, lastBtnB = HIGH, lastBtnC = HIGH;
  
  bool btnA = digitalRead(PIN_OLED_BTN_A);
  bool btnB = digitalRead(PIN_OLED_BTN_B);
  bool btnC = digitalRead(PIN_OLED_BTN_C);

  // If button goes from HIGH (unpressed) to LOW (pressed):
  // The modulo operator (%) loops the page number seamlessly between 0, 1, 2, and 3.
  if (btnA == LOW && lastBtnA == HIGH) oledScreenPage = (oledScreenPage + 3) % 4; // Cycles Backward
  if (btnB == LOW && lastBtnB == HIGH) oledScreenPage = (oledScreenPage + 1) % 4; // Cycles Forward
  
  // Button C cycles through the LED operational modes physically
  if (btnC == LOW && lastBtnC == HIGH) { 
    if (currentLedMode == MODE_TELEOP) currentLedMode = MODE_AUTO;
    else if (currentLedMode == MODE_AUTO) currentLedMode = MODE_ARRIVED;
    else currentLedMode = MODE_TELEOP;
  }

  // Save current state to check against in the next loop
  lastBtnA = btnA; lastBtnB = btnB; lastBtnC = btnC;
}

// ================================================================================
// OLED RENDERING
// ================================================================================
void renderOledTelemetry() {
  display.clearDisplay(); // Wipe the hidden buffer clean
  display.setTextSize(1);

  // Calculates 12V battery voltage via a resistor voltage divider connected to A1
  float rawA1 = analogRead(PIN_BATTERY_SENSE);
  float batteryVoltage = ((rawA1 / 1023.0) * 5.0) * (13.3 / 3.3);

  // Render the selected telemetry page
  switch (oledScreenPage) {
    case 0: // PAGE 1: Core System & Network
      display.setCursor(0, 0);  display.println(F("=== POWER & NETWORK ==="));
      display.setCursor(0, 14); display.print(F("IP:  ")); display.println(WiFi.localIP());
      display.setCursor(0, 26); display.print(F("Bat: ")); display.print(batteryVoltage, 2); display.println(F(" V"));
      display.setCursor(0, 38); display.print(F("LED: "));
      if (currentLedMode == MODE_TELEOP) display.println(F("BLUE (TELEOP)"));
      else if (currentLedMode == MODE_AUTO) display.println(F("RED (AUTO)"));
      else display.println(F("FLASH GREEN (ARRIVED)"));
      break;

    case 1: // PAGE 2: IMU Motion Data
      display.setCursor(0, 0); display.println(F("=== MPU6050 MOTION ==="));
      if (mpuConnected) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp); // Read the latest hardware sensor data
        display.setCursor(0, 16);
        display.print(F("Accel X: ")); display.print(a.acceleration.x, 1); display.println(F(" m/s2"));
        display.print(F("Accel Y: ")); display.print(a.acceleration.y, 1); display.println(F(" m/s2"));
        display.print(F("Gyro  Z: ")); display.print(g.gyro.z, 1); display.println(F(" rad/s"));
      } else {
        display.setCursor(0, 24); display.println(F("IMU Status: N/C"));
      }
      break;

    case 2: // PAGE 3: Wheel Encoder Diagnostics
      display.setCursor(0, 0); display.println(F("=== WHEEL ENCODERS ==="));
      display.setCursor(0, 16);
      display.print(F("FL: ")); display.print(countFL); display.print(F("  FR: ")); display.println(countFR);
      display.print(F("RL: ")); display.print(countRL); display.print(F("  RR: ")); display.println(countRR);
      display.setCursor(0, 40);
      display.print(F("STBY Pin: ")); display.println(digitalRead(PIN_MOTOR_STBY) ? "ENABLED" : "STANDBY");
      break;

    case 3: // PAGE 4: Sonar (Placeholder for now)
      display.setCursor(0, 0); display.println(F("=== HC-SR04 SONAR ==="));
      display.setCursor(0, 24); display.println(F("Sonar: Pins A2/A3 Configured"));
      display.println(F("Ready for sensor plug-in"));
      break;
  }

  // Draw the fixed bottom footer (Shows current page and control hints)
  display.drawFastHLine(0, 54, 128, SH110X_WHITE);
  display.setCursor(0, 56);
  display.print(F("P")); display.print(oledScreenPage + 1); display.print(F("/4 | Btn A/B:Nav C:Mode"));
  
  display.display(); // Push the completed frame to the physical screen
}
