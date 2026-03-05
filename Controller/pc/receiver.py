#!/usr/bin/env python3
"""
3DS Remote Controller - PC Receiver

Usage:
    python receiver.py

Prompts for the 3DS IP and port (shown on the 3DS top screen), then:
  1. Performs a UDP handshake (HELLO -> ACK), retrying up to 5 times with a 1s timeout.
  2. Receives the input stream and prints state changes to the terminal.
  3. Appends all state changes (with timestamps) to timeline.txt in this directory.
"""

import os
import socket
import sys
from datetime import datetime

from protocol import (
    ACK_MSG, BUTTON_NAMES, DEFAULT_3DS_PORT, DEFAULT_PC_PORT,
    HELLO_MSG, PACKET_SIZE, unpack_input,
)

# How long to wait for a handshake ACK before retrying
HANDSHAKE_TIMEOUT_S = 1.0
HANDSHAKE_RETRIES   = 5

# How long to wait for the next input packet before warning about disconnect
STREAM_TIMEOUT_S = 5.0

# Minimum analog delta treated as a real change (suppresses hardware noise)
ANALOG_EPSILON = 0.005

TIMELINE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'timeline.txt')


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def _ts():
    return datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]


def _write_timeline(line):
    with open(TIMELINE_FILE, 'a') as f:
        f.write(f"[{_ts()}] {line}\n")


def log(line, *, also_timeline=False):
    print(line)
    if also_timeline:
        _write_timeline(line)


def log_change(line):
    """Print to terminal and append to timeline."""
    print(line)
    _write_timeline(line)


# ---------------------------------------------------------------------------
# Packet parsing
# ---------------------------------------------------------------------------

def _parse_state(cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask):
    state = {
        'circle_x':    cx,
        'circle_y':    cy,
        'cpp_x':       cpp_x,
        'cpp_y':       cpp_y,
        'cpp_present': cpp_present,
    }
    for name, mask in BUTTON_NAMES:
        state[name] = bool(buttons_mask & mask)
    return state


def _diff(prev, curr):
    """
    Return list of (field, old_val, new_val) for every field that changed.
    Analog axes use ANALOG_EPSILON; everything else is compared exactly.
    """
    analog = {'circle_x', 'circle_y', 'cpp_x', 'cpp_y'}
    changes = []
    for key, new in curr.items():
        old = prev.get(key)
        if key in analog:
            if old is None or abs(new - old) > ANALOG_EPSILON:
                changes.append((key, old, new))
        else:
            if old != new:
                changes.append((key, old, new))
    return changes


def _fmt(key, val):
    if val is None:
        return '?'
    if key in ('circle_x', 'circle_y', 'cpp_x', 'cpp_y'):
        return f'{val:+.3f}'
    if isinstance(val, bool):
        return 'PRESSED ' if val else 'released'
    return str(val)


# ---------------------------------------------------------------------------
# Handshake
# ---------------------------------------------------------------------------

def do_handshake(sock, ds_addr):
    """
    Send HELLO_MSG to ds_addr, wait up to HANDSHAKE_TIMEOUT_S for ACK_MSG.
    Retries HANDSHAKE_RETRIES times. Returns True on success, False on failure.
    """
    sock.settimeout(HANDSHAKE_TIMEOUT_S)
    for attempt in range(1, HANDSHAKE_RETRIES + 1):
        log(f"  [{attempt}/{HANDSHAKE_RETRIES}] Sending HELLO to {ds_addr[0]}:{ds_addr[1]} ...")
        _write_timeline(f"Handshake attempt {attempt}/{HANDSHAKE_RETRIES} -> {ds_addr[0]}:{ds_addr[1]}")
        sock.sendto(HELLO_MSG, ds_addr)
        try:
            data, addr = sock.recvfrom(64)
            if data == ACK_MSG:
                log(f"  ACK received from {addr[0]}:{addr[1]}")
                _write_timeline(f"Handshake ACK from {addr[0]}:{addr[1]}")
                return True
            else:
                log(f"  Unexpected response: {data!r} — ignoring")
        except socket.timeout:
            log(f"  Timed out.")
            _write_timeline(f"Handshake attempt {attempt} timed out")
    return False


# ---------------------------------------------------------------------------
# Receive loop
# ---------------------------------------------------------------------------

def receive_loop(sock):
    sock.settimeout(STREAM_TIMEOUT_S)
    prev_state = None

    log("\nStreaming. Press Ctrl+C to stop.\n")
    _write_timeline("Stream started")

    while True:
        try:
            data, _ = sock.recvfrom(PACKET_SIZE + 32)
        except socket.timeout:
            log_change(f"[warn] No data for {STREAM_TIMEOUT_S:.0f}s — 3DS may have disconnected.")
            continue
        except KeyboardInterrupt:
            break

        try:
            cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask = unpack_input(data)
        except ValueError as e:
            log(f"[warn] Bad packet: {e}")
            continue

        curr = _parse_state(cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask)

        if prev_state is None:
            # Log the full initial snapshot once
            pressed = [n for n, _ in BUTTON_NAMES if curr[n]]
            log_change("--- Initial state ---")
            log_change(f"  Circle pad  : X={curr['circle_x']:+.3f}  Y={curr['circle_y']:+.3f}")
            if curr['cpp_present']:
                log_change(f"  CPP         : X={curr['cpp_x']:+.3f}  Y={curr['cpp_y']:+.3f}")
            else:
                log_change("  CPP         : not present")
            log_change(f"  Buttons     : {', '.join(pressed) if pressed else 'none'}")
            prev_state = curr
            continue

        changes = _diff(prev_state, curr)
        for key, old, new in changes:
            line = f"  {key:<12}  {_fmt(key, old):>10}  ->  {_fmt(key, new)}"
            log_change(line)

        if changes:
            prev_state = curr


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    print("=== 3DS Remote Controller - Receiver ===\n")
    _write_timeline("=== Session started ===")

    ds_ip = input("3DS IP address        : ").strip()
    if not ds_ip:
        print("No IP provided. Exiting.")
        sys.exit(1)

    raw = input(f"3DS port       [{DEFAULT_3DS_PORT}] : ").strip()
    ds_port = int(raw) if raw else DEFAULT_3DS_PORT

    raw = input(f"PC listen port [{DEFAULT_PC_PORT}] : ").strip()
    pc_port = int(raw) if raw else DEFAULT_PC_PORT

    _write_timeline(f"Targeting 3DS at {ds_ip}:{ds_port}, listening on :{pc_port}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(('', pc_port))
    except OSError as e:
        print(f"\nFailed to bind port {pc_port}: {e}")
        sys.exit(1)

    print(f"\nListening on :{pc_port}")
    print(f"Attempting handshake with {ds_ip}:{ds_port} ...\n")

    try:
        if not do_handshake(sock, (ds_ip, ds_port)):
            msg = f"Handshake failed after {HANDSHAKE_RETRIES} attempts."
            log_change(msg)
            sys.exit(1)

        log_change(f"Connected to 3DS at {ds_ip}:{ds_port}")
        receive_loop(sock)

    except KeyboardInterrupt:
        pass
    finally:
        log_change("=== Session ended ===")
        sock.close()


if __name__ == '__main__':
    main()
