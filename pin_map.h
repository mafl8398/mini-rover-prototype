/*
 * ================================================================================
 * Master Pin Mapping Header File (pin_map.h - v6.1 Unified with GPS)
 * 4WD Mini Rover Prototype - Arduino Uno R4 WiFi Architecture
 * ================================================================================
 */

#ifndef PIN_MAP_H
#define PIN_MAP_H

// --- TB6612FNG Dual Motor Drivers ---
#define PIN_LEFT_DIR1     2   
#define PIN_LEFT_PWM      3   
#define PIN_LEFT_DIR2     4   
#define PIN_MOTOR_L_IN1   PIN_LEFT_DIR1
#define PIN_MOTOR_L_PWM   PIN_LEFT_PWM
#define PIN_MOTOR_L_IN2   PIN_LEFT_DIR2

#define PIN_RIGHT_PWM     5   
#define PIN_RIGHT_DIR1    6   
#define PIN_RIGHT_DIR2    7   
#define PIN_MOTOR_R_PWM   PIN_RIGHT_PWM
#define PIN_MOTOR_R_IN1   PIN_RIGHT_DIR1
#define PIN_MOTOR_R_IN2   PIN_RIGHT_DIR2

#define PIN_MOTOR_STBY    8   // Hardware safety cutoff

// --- Single-Phase Hall Quadrature Encoders ---
#define PIN_ENC_FL        A2  
#define PIN_ENC_RL        9   
#define PIN_ENC_FR        11  
#define PIN_ENC_RR        13  

// --- GP-20U7 GPS Module (Hardware Serial1 UART) ---
#define PIN_GPS_RX        0   // Hardware RX (Receives from GPS TX)
#define PIN_GPS_TX        1   // Hardware TX 
#define GPS_BAUD_RATE     9600 

// --- URC Status Indicator & Health Telemetry ---
#define PIN_NEOPIXEL      12  
#define PIN_BATTERY_SENSE A1  // 12V scaled down to 3.3V max via resistor divider

// --- Ranging Sensor (HC-SR04 / RCWL-1601 Ultrasonic) ---
#define PIN_SONAR_TRIG    A3  
#define PIN_SONAR_ECHO    A0  

// --- OLED Display Tactile Buttons ---
// Consolidating to a single button because D10 is the only remaining free pin on the board.
#define PIN_OLED_BTN_A    10  // Press to cycle through the 5 display pages

#endif // PIN_MAP_H
