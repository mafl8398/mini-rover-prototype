#ifndef PIN_MAP_H
#define PIN_MAP_H

// ==========================================
// Arduino Uno R4 WiFi - Mini Rover Pinout
// ==========================================

// Driver 1 (Left Side - Front & Rear Bridged)
#define PIN_LEFT_DIR1     2  // AIN1 & BIN1
#define PIN_LEFT_PWM      3  // PWMA & PWMB (PWM)
#define PIN_LEFT_DIR2     4  // AIN2 & BIN2

// Driver 2 (Right Side - Front & Rear Bridged)
#define PIN_RIGHT_PWM     5  // PWMA & PWMB (PWM)
#define PIN_RIGHT_DIR1    7  // AIN1 & BIN1
#define PIN_RIGHT_DIR2    8  // AIN2 & BIN2

// Standby Pin (Shared by both TB6612FNG drivers)
#define PIN_MOTOR_STBY    6  // HIGH = Active, LOW = Standby

// Encoders (Phase A & B Quadrature)
#define PIN_ENC_FL_A      0  // Front Left Interrupt
#define PIN_ENC_FL_B      1  // Front Left Direction
#define PIN_ENC_RL_A      9  // Rear Left Interrupt
#define PIN_ENC_RL_B     10  // Rear Left Direction
#define PIN_ENC_FR_A     11  // Front Right Interrupt
#define PIN_ENC_FR_B     12  // Front Right Direction
#define PIN_ENC_RR_A     13  // Rear Right Interrupt
#define PIN_ENC_RR_B     A0  // Rear Right Direction

// Distance Sensor (RCWL-1601 / HC-SR04)
#define PIN_SONAR_TRIG   A2  // Digital Output
#define PIN_SONAR_ECHO   A3  // Digital Input

// Battery Voltage Sensing
#define PIN_BATTERY_SENSE A1 // Analog Input via Resistor Divider

// Shared I2C Bus (Pins A4 = SDA, A5 = SCL)
// - MPU6050 IMU: Address 0x68
// - Adafruit OLED Display: Address 0x3C

#endif
