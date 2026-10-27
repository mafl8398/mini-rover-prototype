# 4WD Mini-Rover Prototype (URC 2027 Subscale Testbed)

An agile, low-cost 4WD subscale rover prototype designed to de-risk electrical architecture, wireless teleoperation over UDP, sensor fusion, and closed-loop motor control ahead of the full-scale University Rover Challenge (URC) build.

---
##🛠️ System Overview & Architecture
Main Compute: Arduino Uno R4 WiFi (32-bit Renesas RA4M1 @ 48 MHz + ESP32-S3 Wi-Fi/BT module)
Motor Drivers: Dual TB6612FNG H-Bridge Drivers (7-pin bridged differential drive logic)
Actuation: 4x GA12-N20 12V DC Metal Geared Motors in differential skid-steer configuration
Telemetry & Sensing:
GPS Navigation: GP-20U7 66-Channel Module (Hardware UART on D0 @ 9600 Baud)
Inertial Navigation: MPU6050 6-DoF IMU (I2C Wire bus on A4/A5)
Speed/Odometry: Single-Phase Magnetic Quadrature Encoders (A2, D9, D11, D13)
Power Monitoring: 10k/3.3k Resistor Divider on Analog Pin A1 (12V sensing)
Visual Status: Adafruit 128x64 OLED Display (STEMMA QT / Qwiic Wire1 bus)
URC Indicator: WS2812B Smart RGB NeoPixel LED on D12

---

## 🛠️ Prerequisites & Installation

### 1. Host Laptop Setup (Python Teleop Station)
The host controller script requires Python 3.8+ and Pygame to poll Xbox controller inputs and transmit UDP packets.

```bash
# Clone the repository
git clone https://github.com/mafl8398/mini-rover-prototype.git
cd mini-rover-prototype

# Install required dependencies
pip install pygame
```

### 2. Arduino IDE Setup (Firmware Upload)
Install the following libraries via the **Arduino IDE Library Manager** (`Ctrl+Shift+I` / `Cmd+Shift+I`):
* **`WiFiS3`** *(Built-in with Arduino Uno R4 WiFi Board Package)*
* **`Adafruit_SSD1306`** & **`Adafruit_GFX`** *(OLED Display Driver)*
* **`Adafruit_NeoPixel`** *(URC Status RGB LED)*
* **`Adafruit_MPU6050`** & **`Adafruit_Sensor`** *(Inertial Measurement Unit)*

---

## 🌐 Network Configuration & Finding the Rover IP

The rover can operate in two distinct Wi-Fi network modes:

### Network Mode Comparison
1. **Router Mode (Default Benchtop Network - Linksys Blue):**
   * **Network SSID:** `Linksys Blue`
   * **Password:** `MF114363`
   * **Default Subnet IP:** Assigned dynamically via DHCP (e.g., `10.201.251.114`)
2. **Standalone Access Point Mode (`Rover_AP`):**
   * **AP SSID:** `Rover_AP`
   * **Password:** `Rover1234`
   * **Default Subnet IP:** `192.168.4.1`

### How to Read the Rover's Active IP Address
You can obtain the assigned IP address without needing USB serial debugging:

1. **On the Local OLED Display (Page 0):**
   * Upon boot, the Adafruit 128x64 OLED display defaults to **Page 1: `=== POWER & NETWORK ===`**.
   * Read the second line directly from the screen: `IP:  10.201.251.114` (or `192.168.4.1`).
2. **Via Arduino IDE Serial Monitor:**
   * Open the Serial Monitor at **`115200 baud`**.
   * During startup, the firmware prints the network connection state and active IP:
     ```text
     [WIFI] Connected Successfully!
     [WIFI] Rover IP Address: 10.201.251.114
     [UDP] Listening for teleop commands on port: 4210
     ```

---

## 📌 Master Hardware Pin Mapping (`pin_map.h`)

| Arduino Pin | Peripheral / Signal | Function / Notes |
| :--- | :--- | :--- |
| **`D0` (RX)** | GP-20U7 GPS TX | Hardware `Serial1` RX (9600 Baud) |
| **`D2`** | Driver 1 `AIN1`/`BIN1` | Left Direction 1 |
| **`D3` (PWM)**| Driver 1 `PWMA`/`PWMB` | Left Speed PWM |
| **`D4`** | Driver 1 `AIN2`/`BIN2` | Left Direction 2 |
| **`D5` (PWM)**| Driver 2 `PWMA`/`PWMB` | Right Speed PWM |
| **`D6`** | Driver 2 `AIN1`/`BIN1` | Right Direction 1 |
| **`D7`** | Driver 2 `AIN2`/`BIN2` | Right Direction 2 |
| **`D8`** | Drivers 1 & 2 `STBY` | Shared Driver Standby / E-Stop Enable |
| **`D9`** | Rear Left Encoder | Phase A Speed Interrupt (`RL`) |
| **`D11`** | Front Right Encoder | Phase A Speed Interrupt (`FR`) |
| **`D12`** | NeoPixel State LED | URC Mode Status Signal |
| **`D13`** | Rear Right Encoder | Phase A Speed Interrupt (`RR`) |
| **`A1`** | Battery Voltage Sense | 10k/3.3k Divider (0–15V sensing range) |
| **`A2`** | Front Left Encoder | Phase A Speed Interrupt (`FL`) |
| **`A4` / `A5`**| MPU6050 IMU | Standard I2C `Wire` Bus (`SDA`/`SCL`) |
| **Qwiic Port**| OLED Display (128x64) | Secondary I2C `Wire1` Bus |

---

## 📺 Onboard OLED Navigation & Telemetry Pages

The Adafruit 128x64 OLED FeatherWing includes 3 tactile buttons used for local diagnostic cycling:
* **Button A (`A0`):** Cycle to **Previous** Telemetry Page
* **Button B (`D10`):** Cycle to **Next** Telemetry Page
* **Button C (`D1`):** Manual URC State LED Toggle (`TELEOP` \\(\rightarrow\\) `AUTO` \\(\rightarrow\\) `ARRIVED`)

### Telemetry Display Screens:
* **Page 1 (`POWER & NETWORK`):** IP Address, Battery Voltage (A1 divider), LED Status, Uptime.
* **Page 2 (`MPU6050 MOTION`):** Live Acceleration (\\(m/s^2\\)) and Gyro Rotation (\\(rad/s\\)).
* **Page 3 (`WHEEL ENCODERS`):** Individual Pulse Ticks (`FL`, `RL`, `FR`, `RR`) & Driver Standby State.
* **Page 4 (`HC-SR04 SONAR`):** Frontal Obstacle Distance (\\(cm\\)).
* **Page 5 (`GP-20U7 GPS NAV`):** Latitude, Longitude, and Satellite Fix Count.

---

## 🎮 How to Launch Teleoperation

1. **Connect Wi-Fi:** Ensure your laptop is joined to `Linksys Blue` (Password: `MF114363`).
2. **Verify IP Address:** Check the rover's OLED display to confirm its active IP, then open `teleop_controller.py` and set:
   ```python
   ROVER_IP = "10.201.251.114"  # Match IP shown on OLED Screen Page 1
   ROVER_PORT = 4210
   ```
3. **Plug in Xbox Controller** via USB or Bluetooth.
4. **Execute Script:**
   ```bash
   python teleop_controller.py
   ```

### Control Mappings:
Laptop Keyboard Controls:
* **W / Up Arrow:** Drive Forward (Throttle: +200)
* **S / Down Arrow:** Reverse (Throttle: -200)
* **A / Left Arrow:** Spin Left (Steering: -180)
* **D / Right Arrow:** Spin Right (Steering: +180)
* **Spacebar:** Emergency Stop / Instant Zero Velocity (0, 0)
* **1 / X:** Set URC LED Mode to TELEOP (Solid Blue)
* **2 / B:** Set URC LED Mode to AUTO (Solid Red)
* **3 / A:** Set URC LED Mode to ARRIVED (Flashing Green)
* **Right Joystick Vertical:** Throttle (Forward / Reverse)
* **Right Joystick Horizontal:** Steering (Left / Right Differential Turn)
* **Button X (Blue):** Set Mode to **`TELEOP`** (Solid Blue NeoPixel)
* **Button B (Red):** Set Mode to **`AUTO`** (Solid Red NeoPixel)
* **Button A (Green):** Set Mode to **`ARRIVED`** (Flashing Green NeoPixel)

---

## ⚡ Safety & Watchdog System

* **Hardware E-Stop:** A physical SPDT toggle switch isolates the 12V battery positive lead from the motor driver `VM` pins.
* **500 ms Communication Watchdog:** If the UDP packet stream stops for over **500 ms**, the Arduino firmware automatically pulls `STBY` (Pin `D8`) **LOW**, disabling the motor outputs until a valid packet arrives.
```

---

📡 **Does this cover all the missing details you wanted for your repo?**
