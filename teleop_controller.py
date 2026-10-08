"""
Xbox Controller Teleoperation Host Script
Reads Xbox controller joystick inputs via Pygame and transmits UDP drive commands to the Mini Rover.
"""

import socket
import time
import sys

# Try importing pygame; print friendly error if not installed
try:
    import pygame
except ImportError:
    print("[ERROR] Pygame is not installed. Run 'pip install pygame' on your laptop.")
    sys.exit(1)

# ==========================================
# Network Configuration
# ==========================================
ROVER_IP = "192.168.1.100"  # Replace with Arduino's IP address once connected
ROVER_PORT = 4210            # UDP port opened on Arduino

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# ==========================================
# Gamepad Initialization
# ==========================================
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("[WARNING] No joystick detected! Plug in your Xbox controller and restart.")
else:
    controller = pygame.joystick.Joystick(0)
    controller.init()
    print(f"[SYSTEM] Connected to Controller: {controller.get_name()}")

print(f"[SYSTEM] Streaming teleop packets to {ROVER_IP}:{ROVER_PORT} (Press Ctrl+C to stop)...")

# ==========================================
# Main Control Loop (~20 Hz)
# ==========================================
try:
    while True:
        pygame.event.pump()

        if pygame.joystick.get_count() > 0:
            # Axis 1: Left Stick Vertical (Inverted so forward is positive)
            throttle_raw = -controller.get_axis(1)
            # Axis 2 or 0: Right/Left Stick Horizontal (Steering)
            steering_raw = controller.get_axis(0)

            # Map float values (-1.0 to 1.0) to Motor PWM range (-255 to 255)
            throttle = int(throttle_raw * 255)
            steering = int(steering_raw * 255)

            # Deadzone filter to ignore small stick drift
            if abs(throttle) < 20: throttle = 0
            if abs(steering) < 20: steering = 0

            # Format packet string: "throttle,steering\n" (e.g., "180,-45\n")
            packet = f"{throttle},{steering}\n"

            # Transmit via UDP
            sock.sendto(packet.encode('utf-8'), (ROVER_IP, ROVER_PORT))
            print(f"[TELEOP SENT] Throttle: {throttle:4d} | Steering: {steering:4d}", end='\r')

        time.sleep(0.05) # 50ms loop delay (20 Packets/sec)

except KeyboardInterrupt:
    print("\n[SYSTEM] Teleop script stopped by user.")
    sock.close()
    pygame.quit()
