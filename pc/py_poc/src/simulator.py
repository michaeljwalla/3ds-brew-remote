#!/usr/bin/env python3
"""
3DS Input Simulator - loopback test for the PC receiver.

Pretends to be the 3DS homebrew app on 127.0.0.1:6000.

Run this in one terminal, then run receiver.py in another and enter:
  3DS IP address : 127.0.0.1
  3DS port       : 6000
  PC listen port : 6001  (or press Enter for default)

The simulator will:
  1. Wait for the HELLO handshake from the receiver.
  2. Reply with ACK.
  3. Stream fake controller packets at 60 Hz, cycling through:
       - Circle pad sweeping a slow ellipse
       - CPP doing a smaller counter-rotation
       - Each button pressed then released in sequence, one every ~0.75s
       - Touch cycling on/off every ~2s, tracing a slow path
       - Gyroscope simulating slow rotation (~±50 dps)
       - Accelerometer simulating a resting 3DS (gravity mostly on Z)
"""

import math
import socket
import time

from .protocol import (
    ACK_MSG, BUTTON_NAMES, DEFAULT_3DS_PORT,
    HELLO_MSG, pack_input,
)

LOOPBACK   = '127.0.0.1'
STREAM_HZ  = 60
DT         = 1.0 / STREAM_HZ

# How long (seconds) each button stays pressed before being released and
# moving to the next one.
BTN_HOLD_S = 0.4
BTN_GAP_S  = 0.35   # gap between releases and next press

# Touch cycle: active for TOUCH_ON_S, then inactive for TOUCH_OFF_S
TOUCH_ON_S  = 2.0
TOUCH_OFF_S = 1.0


def wait_for_hello(sock):
    print(f"[sim] Bound to {LOOPBACK}:{DEFAULT_3DS_PORT}")
    print("[sim] Waiting for HELLO from receiver ...")
    sock.settimeout(60.0)
    while True:
        try:
            data, addr = sock.recvfrom(64)
        except socket.timeout:
            print("[sim] No HELLO received in 60s. Exiting.")
            raise SystemExit(1)
        if data == HELLO_MSG:
            print(f"[sim] HELLO received from {addr[0]}:{addr[1]}")
            return addr
        else:
            print(f"[sim] Ignoring unexpected packet: {data!r}")


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOOPBACK, DEFAULT_3DS_PORT))

    target = wait_for_hello(sock)
    sock.sendto(ACK_MSG, target)
    print(f"[sim] ACK sent. Streaming at {STREAM_HZ} Hz to {target[0]}:{target[1]}")
    print("[sim] Press Ctrl+C to stop.\n")

    buttons_list = [mask for _, mask in BUTTON_NAMES]
    btn_index    = 0
    btn_mask     = 0
    btn_held_for = 0.0   # seconds the current button has been in its state
    btn_pressed  = False # True = currently pressed, False = in gap before next

    touch_timer  = 0.0   # time in current touch phase
    touch_active = False

    t = 0.0

    try:
        while True:
            frame_start = time.monotonic()

            # --- Analog stick inputs ---
            cx    =  math.sin(t * 0.4)
            cy    =  math.cos(t * 0.4) * 0.7
            cpp_x =  math.sin(t * 0.25 + 0.9) * 0.6
            cpp_y = -math.cos(t * 0.25 + 0.9) * 0.6
            cpp_present = True

            # --- Button cycling ---
            btn_held_for += DT
            if btn_pressed:
                if btn_held_for >= BTN_HOLD_S:
                    btn_mask     &= ~buttons_list[btn_index]
                    btn_pressed   = False
                    btn_held_for  = 0.0
            else:
                if btn_held_for >= BTN_GAP_S:
                    btn_index     = (btn_index + 1) % len(buttons_list)
                    btn_mask     |= buttons_list[btn_index]
                    btn_pressed   = True
                    btn_held_for  = 0.0

            # --- Touch cycling ---
            touch_timer += DT
            if touch_active:
                if touch_timer >= TOUCH_ON_S:
                    touch_active = False
                    touch_timer  = 0.0
            else:
                if touch_timer >= TOUCH_OFF_S:
                    touch_active = True
                    touch_timer  = 0.0

            # Slow path across the touch screen while active
            touch_x = 0.5 + math.sin(t * 0.6) * 0.4
            touch_y = 0.5 + math.cos(t * 0.8) * 0.35

            # --- Gyroscope (degrees/second) - simulated slow rotation ---
            gyro_x = math.sin(t * 0.31) * 48.0
            gyro_y = math.cos(t * 0.47) * 32.0
            gyro_z = math.sin(t * 0.19 + 1.2) * 20.0

            # --- Accelerometer (g-force) - resting 3DS with slight tilt sway ---
            accel_x = math.sin(t * 0.13) * 0.15
            accel_y = math.cos(t * 0.17) * 0.12
            accel_z = 1.0 - abs(math.sin(t * 0.09)) * 0.08   # mostly 1 g

            pkt = pack_input(
                cx, cy, cpp_x, cpp_y, cpp_present, btn_mask,
                touch_active, touch_x, touch_y,
                gyro_x, gyro_y, gyro_z,
                accel_x, accel_y, accel_z,
            )
            sock.sendto(pkt, target)

            t += DT
            elapsed = time.monotonic() - frame_start
            wait = DT - elapsed
            if wait > 0:
                time.sleep(wait)

    except KeyboardInterrupt:
        print("\n[sim] Stopped.")
    finally:
        sock.close()


if __name__ == '__main__':
    main()
