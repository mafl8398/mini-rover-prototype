# mini-rover-prototype
4WD Skid-Steer Mini Rover powered by Arduino Uno R4 WiFi.

## Software Dependencies & Libraries

### Core Architecture
* **Target Board:** Arduino Uno R4 WiFi (RA4M1 Architecture)
* **Library Compatibility Reference:** https://github.com/arduino/uno-r4-library-compatibility

### Required Libraries (Arduino IDE)
1. **WiFiS3** (Built-in) — Handles Wi-Fi networking and UDP/TCP communication.
2. **Wire** (Built-in) — Drives the main I2C bus (MPU6050 IMU on pins A4/A5).
3. **Adafruit_SSD1306** & **Adafruit_GFX** — Controls the 128x64 OLED display over I2C.
4. **Adafruit_MPU6050** & **Adafruit_Sensor** — Reads 6-DoF acceleration and rotation data.

### Required Libraries (Python Host Script on Laptop)
1. **pygame** — Reads inputs from Xbox controller connected via USB.
2. **socket** (Built-in) — Sends wireless telemetry/drive packets to the rover via UDP.
