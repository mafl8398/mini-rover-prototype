"""
================================================================================
URC 2027 Mini-Rover Teleoperation Control Station (v2 - WASD & Gamepad Support)
================================================================================
Dual-input base station script supporting both Xbox Gamepad joysticks and 
laptop WASD / Arrow keys for wireless rover teleoperation over UDP.

URC 2027 LED Mode Controls:
  - [X] Key / [1] Key / Gamepad X Button -> Teleoperation Mode (Solid Blue LED)
  - [B] Key / [2] Key / Gamepad B Button -> Autonomous Mode (Solid Red LED)
  - [A] Key / [3] Key / Gamepad A Button -> Target Arrival Mode (Flashing Green LED)

Keyboard Drive Controls:
  - [W] or [Up Arrow]    -> Forward Throttle (+200)
  - [S] or [Down Arrow]  -> Reverse Throttle (-200)
  - [A] or [Left Arrow]  -> Steer Left (-180)
  - [D] or [Right Arrow] -> Steer Right (+180)
  - [Spacebar]           -> Emergency Stop (Zero Throttle/Steering)

Usage:
  python teleop_controller.py
================================================================================
"""

import socket
import time
import sys

# Attempt Pygame import
try:
    import pygame
except ImportError:
    print("[ERROR] Pygame is not installed. Run 'pip install pygame' on your laptop.")
    sys.exit(1)

# ===============================================================================
# NETWORK CONFIGURATION
# ===============================================================================
# Linksys Blue Benchtop Network IP (Default dynamic IP from OLED Page 1)
# For Standalone AP mode (Rover_AP), change to: "192.168.4.1"
ROVER_IP = "10.201.251.114"
ROVER_PORT = 4210

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# ===============================================================================
# PYGAME & GUI INITIALIZATION
# ===============================================================================
pygame.init()
pygame.joystick.init()

# Create GUI HUD Window (Required for Pygame keyboard event capture)
WINDOW_WIDTH, WINDOW_HEIGHT = 520, 360
screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
pygame.display.set_caption("URC 2027 Mini-Rover Teleop Station")
clock = pygame.time.Clock()

# Color Definitions
COLOR_BG = (25, 30, 38)
COLOR_PANEL = (38, 45, 56)
COLOR_WHITE = (240, 240, 240)
COLOR_GRAY = (140, 150, 165)
COLOR_BLUE = (0, 140, 255)
COLOR_RED = (255, 60, 60)
COLOR_GREEN = (40, 220, 100)
COLOR_YELLOW = (255, 200, 50)

# Fonts
font_title = pygame.font.SysFont("Helvetica", 20, bold=True)
font_body = pygame.font.SysFont("Helvetica", 14)
font_bold = pygame.font.SysFont("Helvetica", 14, bold=True)
font_status = pygame.font.SysFont("Helvetica", 16, bold=True)

# Detect connected gamepads
controller = None
if pygame.joystick.get_count() > 0:
    controller = pygame.joystick.Joystick(0)
    controller.init()
    print(f"[SYSTEM] Gamepad Detected: {controller.get_name()}")
else:
    print("[SYSTEM] No Gamepad detected. WASD Keyboard Controls are active.")

print(f"[SYSTEM] Teleop Station active! Streaming packets to {ROVER_IP}:{ROVER_PORT}")
print("   -> Driving: WASD / Arrow Keys OR Gamepad Joysticks")
print("   -> Modes:   [X/1] Teleop (Blue) | [B/2] Auto (Red) | [A/3] Arrived (Green)\n")

# Local State Tracking
current_mode_str = "TELEOP"
mode_color = COLOR_BLUE
last_btn_state = {'a': False, 'b': False, 'x': False}

# ===============================================================================
# MAIN TELEOP LOOP (~20 Hz)
# ===============================================================================
try:
    running = True
    while running:
        # 1. Process Pygame Window Events
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                # Mode Key Shortcuts
                if event.key == pygame.K_x or event.key == pygame.K_1:
                    current_mode_str = "TELEOP"
                    mode_color = COLOR_BLUE
                    sock.sendto(b"MODE:TELEOP\n", (ROVER_IP, ROVER_PORT))
                    print("\n[KEYBOARD MODE] -> Teleoperation Mode (Solid Blue LED)")
                elif event.key == pygame.K_b or event.key == pygame.K_2:
                    current_mode_str = "AUTO"
                    mode_color = COLOR_RED
                    sock.sendto(b"MODE:AUTO\n", (ROVER_IP, ROVER_PORT))
                    print("\n[KEYBOARD MODE] -> Autonomous Operation Mode (Solid Red LED)")
                elif event.key == pygame.K_a or event.key == pygame.K_3:
                    current_mode_str = "ARRIVED"
                    mode_color = COLOR_GREEN
                    sock.sendto(b"MODE:ARRIVED\n", (ROVER_IP, ROVER_PORT))
                    print("\n[KEYBOARD MODE] -> Target Arrival Triggered (Flashing Green LED)")

        # 2. Poll WASD / Arrow Keyboard States
        keys = pygame.key.get_pressed()
        
        kb_throttle = 0
        kb_steering = 0
        
        if keys[pygame.K_w] or keys[pygame.K_UP]:
            kb_throttle += 200
        if keys[pygame.K_s] or keys[pygame.K_DOWN]:
            kb_throttle -= 200
        if keys[pygame.K_a] or keys[pygame.K_LEFT]:
            kb_steering -= 180
        if keys[pygame.K_d] or keys[pygame.K_RIGHT]:
            kb_steering += 180
        if keys[pygame.K_SPACE]:  # Emergency Stop Spacebar
            kb_throttle = 0
            kb_steering = 0

        # 3. Poll Gamepad Joysticks & Buttons (if plugged in)
        js_throttle = 0
        js_steering = 0
        
        if controller:
            # Check Gamepad Buttons (0=A, 1=B, 2=X)
            btn_a = controller.get_button(0)
            btn_b = controller.get_button(1)
            btn_x = controller.get_button(2)

            if btn_x and not last_btn_state['x']:
                current_mode_str = "TELEOP"
                mode_color = COLOR_BLUE
                sock.sendto(b"MODE:TELEOP\n", (ROVER_IP, ROVER_PORT))
                print("\n[GAMEPAD MODE] -> Teleoperation Mode (Solid Blue LED)")
            elif btn_b and not last_btn_state['b']:
                current_mode_str = "AUTO"
                mode_color = COLOR_RED
                sock.sendto(b"MODE:AUTO\n", (ROVER_IP, ROVER_PORT))
                print("\n[GAMEPAD MODE] -> Autonomous Operation Mode (Solid Red LED)")
            elif btn_a and not last_btn_state['a']:
                current_mode_str = "ARRIVED"
                mode_color = COLOR_GREEN
                sock.sendto(b"MODE:ARRIVED\n", (ROVER_IP, ROVER_PORT))
                print("\n[GAMEPAD MODE] -> Target Arrival Triggered (Flashing Green LED)")

            last_btn_state['a'] = btn_a
            last_btn_state['b'] = btn_b
            last_btn_state['x'] = btn_x

            # Read Joysticks (Axis 1 = Y Throttle, Axis 0 = X Steering)
            # Check for alternative axis index if primary is inactive
            raw_t = -controller.get_axis(1) if abs(controller.get_axis(1)) > 0.05 else (-controller.get_axis(3) if controller.get_numaxes() > 3 else 0)
            raw_s = controller.get_axis(0) if abs(controller.get_axis(0)) > 0.05 else (controller.get_axis(2) if controller.get_numaxes() > 2 else 0)
            
            js_throttle = int(raw_t * 255)
            js_steering = int(raw_s * 255)

            # Deadzone Filter
            if abs(js_throttle) < 20: js_throttle = 0
            if abs(js_steering) < 20: js_steering = 0

        # 4. Merge Keyboard and Gamepad Control Channels
        final_throttle = kb_throttle if kb_throttle != 0 else js_throttle
        final_steering = kb_steering if kb_steering != 0 else js_steering

        # Constrain PWM values between -255 and 255
        final_throttle = max(-255, min(255, final_throttle))
        final_steering = max(-255, min(255, final_steering))

        # 5. Transmit UDP Packet to Rover
        packet_str = f"{final_throttle},{final_steering}\n"
        sock.sendto(packet_str.encode('utf-8'), (ROVER_IP, ROVER_PORT))

        # Terminal Telemetry Output
        active_source = "WASD/KEYBOARD" if (kb_throttle or kb_steering) else ("GAMEPAD" if (js_throttle or js_steering) else "IDLE")
        print(f"[TELEOP] Source: {active_source:13s} | Mode: {current_mode_str:8s} | Throttle: {final_throttle:4d} | Steering: {final_steering:4d}", end='\r')

        # 6. Render Graphical Teleop HUD Window
        screen.fill(COLOR_BG)

        # Header Banner
        pygame.draw.rect(screen, COLOR_PANEL, (15, 15, 490, 50), border_radius=6)
        lbl_title = font_title.render("URC MINI-ROVER TELEOP HUD", True, COLOR_WHITE)
        lbl_ip = font_body.render(f"Target: {ROVER_IP}:{ROVER_PORT}", True, COLOR_YELLOW)
        screen.blit(lbl_title, (30, 22))
        screen.blit(lbl_ip, (330, 30))

        # Mode Indicator Box
        pygame.draw.rect(screen, COLOR_PANEL, (15, 75, 235, 120), border_radius=6)
        lbl_mhdr = font_body.render("CURRENT URC LED MODE", True, COLOR_GRAY)
        lbl_mval = font_status.render(f"MODE: {current_mode_str}", True, mode_color)
        pygame.draw.circle(screen, mode_color, (35, 155), 10)
        lbl_mdesc = font_body.render("Active Status Payload", True, COLOR_WHITE)
        screen.blit(lbl_mhdr, (30, 88))
        screen.blit(lbl_mval, (30, 115))
        screen.blit(lbl_mdesc, (52, 147))

        # Output Power Gauges Box
        pygame.draw.rect(screen, COLOR_PANEL, (260, 75, 245, 120), border_radius=6)
        lbl_ghdr = font_body.render("DRIVE COMMAND OUTPUTS", True, COLOR_GRAY)
        lbl_th = font_bold.render(f"Throttle: {final_throttle:4d}", True, COLOR_WHITE)
        lbl_st = font_bold.render(f"Steering: {final_steering:4d}", True, COLOR_WHITE)
        lbl_src = font_body.render(f"Input Active: {active_source}", True, COLOR_YELLOW)
        screen.blit(lbl_ghdr, (275, 88))
        screen.blit(lbl_th, (275, 115))
        screen.blit(lbl_st, (275, 138))
        screen.blit(lbl_src, (275, 162))

        # Controls Reference Cheat Sheet Panel
        pygame.draw.rect(screen, COLOR_PANEL, (15, 205, 490, 140), border_radius=6)
        lbl_chdr = font_bold.render("CONTROLS CHEAT SHEET", True, COLOR_WHITE)
        lbl_c1 = font_body.render("Drive: WASD / Arrow Keys OR Xbox Right Stick", True, COLOR_GRAY)
        lbl_c2 = font_body.render("Modes: [X / 1] Teleop (Blue)  |  [B / 2] Auto (Red)  |  [A / 3] Arrived (Green)", True, COLOR_GRAY)
        lbl_c3 = font_body.render("E-Stop: [Spacebar] Instant Zero  |  Stop Script: [Ctrl+C] or Close Window", True, COLOR_GRAY)
        screen.blit(lbl_chdr, (30, 218))
        screen.blit(lbl_c1, (30, 245))
        screen.blit(lbl_c2, (30, 270))
        screen.blit(lbl_c3, (30, 295))

        pygame.display.flip()
        clock.tick(20)  # Maintain 20 Hz loop rate

except KeyboardInterrupt:
    print("\n[SYSTEM] Teleop script stopped by user (KeyboardInterrupt).")

finally:
    # Send zero velocity packet on exit
    sock.sendto(b"0,0\n", (ROVER_IP, ROVER_PORT))
    sock.close()
    pygame.quit()
    print("[SYSTEM] Closed UDP socket and GUI window successfully.")
