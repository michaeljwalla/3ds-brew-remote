#!/usr/bin/env python3
"""
3DS Remote Controller - PC Receiver

Usage:
    python receiver.py

Prompts for the 3DS IP and port (shown on the 3DS top screen), then:
  1. Performs a UDP handshake (HELLO -> ACK), retrying up to 5 times with a 1s timeout.
  2. Receives the input stream and renders a live table that overwrites in-place.
  3. Appends all state changes (with timestamps) to timeline.txt in this directory.
  4. Auto-terminates after 30 seconds of no valid input packets.
"""

import os
import socket
import sys
import time
from datetime import datetime

from protocol import (
    ACK_MSG, BUTTON_NAMES, DEFAULT_3DS_PORT, DEFAULT_PC_PORT,
    HELLO_MSG, PACKET_SIZE, unpack_input,
)

HANDSHAKE_TIMEOUT_S  = 0.5
HANDSHAKE_RETRIES    = 30
POLL_TIMEOUT_S       = 1.0   # socket poll interval; short so we can check auto-terminate
AUTO_TERMINATE_S     = 30.0  # exit if no valid packet received for this long
ANALOG_EPSILON       = 0.005 # minimum analog delta treated as a real change
KEEPALIVE_INTERVAL_S = 5.0   # how often to re-send HELLO to the 3DS while streaming

TIMELINE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'timeline.txt')


# ---------------------------------------------------------------------------
# Timeline logging  (terminal output stops once the table is live)
# ---------------------------------------------------------------------------

def _ts():
    return datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]

_timeline = None
def set_timeline(fxn: "function"):
    global _timeline
    _timeline = fxn
    return

def _write_timeline(line):
    if not _timeline: return
    _timeline(line)
    return
    # with open(TIMELINE_FILE, 'a') as f:
    #     f.write(f"[{_ts()}] {line}\n")


# ---------------------------------------------------------------------------
# Table rendering
#
# Layout (W=46, 22 lines):
#
#   +--------------------------------------------+   line  1
#   |         3DS REMOTE CONTROLLER              |   line  2
#   +----------------------+---------------------+   line  3
#   |   CIRCLE PAD         |   CPP [NOT PRESENT] |   line  4
#   |  X :  +0.000         |  X :  +0.000        |   line  5
#   |  Y :  +0.000         |  Y :  +0.000        |   line  6
#   +--------------------------------------------+   line  7
#   |  FACE       | D-PAD        | SHOULDER      |   line  8
#   |  A   [X]    | UP    [X]    | L    [ ]      |   line  9
#   |  B   [ ]    | DOWN  [ ]    | R    [X]      |   line 10
#   |  X   [ ]    | LEFT  [ ]    | ZL   [ ]      |   line 11
#   |  Y   [ ]    | RIGHT [ ]    | ZR   [ ]      |   line 12
#   +--------------------------------------------+   line 13
#   |  SELECT [X]                   START [ ]    |   line 14
#   +--------------------------------------------+   line 15
#   |  TOUCH: [YES]   X: 0.512   Y: 0.341        |   line 16
#   +----------------------+---------------------+   line 17
#   |  GYRO (dps)          |  ACCEL (g)          |   line 18
#   |  X :    +48.000      |  X :   +0.150       |   line 19
#   |  Y :    -32.000      |  Y :   -0.120       |   line 20
#   |  Z :    +20.000      |  Z :   +0.980       |   line 21
#   +--------------------------------------------+   line 22
#
# Column widths:
#   Full inner:  44  (W - 2 border chars)
#   Analog:      left=22, right=21   (1+22+1+21+1 = 46)
#   Buttons:     c1=12, c2=13, c3=17 (1+12+1+13+1+17+1 = 46)
# ---------------------------------------------------------------------------

TABLE_WIDTH  = 46
TABLE_LINES  = 22
_INNER       = TABLE_WIDTH - 2   # 44
_A_LEFT      = 22
_A_RIGHT     = 21
_B_C1        = 12
_B_C2        = 13
_B_C3        = 17

_SEP  = '+' + '-' * _INNER + '+'
_SEP2 = '+' + '-' * _A_LEFT + '+' + '-' * _A_RIGHT + '+'

_table_active = False   # True after first render; controls ANSI jump-back


def _p(s, n):
    """Truncate or space-pad string s to exactly n chars."""
    s = str(s)
    return s[:n] if len(s) > n else s.ljust(n)


def _btn(state, name):
    return '[X]' if state.get(name) else '[ ]'


def _render(state):
    """Return a list of exactly TABLE_LINES strings, each TABLE_WIDTH chars wide."""
    cpp_label = 'CONNECTED ' if state['cpp_present'] else 'NOT PRESENT'

    lines = [
        _SEP,
        '|' + _p('  3DS REMOTE CONTROLLER', _INNER) + '|',
        _SEP2,
        '|' + _p('  CIRCLE PAD', _A_LEFT) + '|' + _p(f'  CPP [{cpp_label}]', _A_RIGHT) + '|',
        '|' + _p(f'  X :  {state["circle_x"]:+.3f}', _A_LEFT) + '|' + _p(f'  X :  {state["cpp_x"]:+.3f}', _A_RIGHT) + '|',
        '|' + _p(f'  Y :  {state["circle_y"]:+.3f}', _A_LEFT) + '|' + _p(f'  Y :  {state["cpp_y"]:+.3f}', _A_RIGHT) + '|',
        _SEP,
        '|' + _p('  FACE',     _B_C1) + '|' + _p('  D-PAD',  _B_C2) + '|' + _p('  SHOULDER', _B_C3) + '|',
        '|' + _p(f'  A   {_btn(state,"A")}',   _B_C1) + '|' + _p(f'  UP    {_btn(state,"UP")}',    _B_C2) + '|' + _p(f'  L   {_btn(state,"L")}',  _B_C3) + '|',
        '|' + _p(f'  B   {_btn(state,"B")}',   _B_C1) + '|' + _p(f'  DOWN  {_btn(state,"DOWN")}',  _B_C2) + '|' + _p(f'  R   {_btn(state,"R")}',  _B_C3) + '|',
        '|' + _p(f'  X   {_btn(state,"X")}',   _B_C1) + '|' + _p(f'  LEFT  {_btn(state,"LEFT")}',  _B_C2) + '|' + _p(f'  ZL  {_btn(state,"ZL")}', _B_C3) + '|',
        '|' + _p(f'  Y   {_btn(state,"Y")}',   _B_C1) + '|' + _p(f'  RIGHT {_btn(state,"RIGHT")}', _B_C2) + '|' + _p(f'  ZR  {_btn(state,"ZR")}', _B_C3) + '|',
        _SEP,
    ]

    # SELECT / START row — fixed positions within the 44-char inner
    sel = f'  SELECT {_btn(state, "SELECT")}'   # 12 chars
    sta = f'START {_btn(state, "START")}  '     # 11 chars
    mid = _INNER - len(sel) - len(sta)
    lines.append('|' + sel + ' ' * mid + sta + '|')
    lines.append(_SEP)

    # Touch row
    touch_state = 'YES' if state['touch_active'] else ' NO'
    touch_line  = f'  TOUCH: [{touch_state}]   X: {state["touch_x"]:.3f}   Y: {state["touch_y"]:.3f}'
    lines.append('|' + _p(touch_line, _INNER) + '|')

    # Gyro / Accel section
    lines.append(_SEP2)
    lines.append('|' + _p('  GYRO (dps)', _A_LEFT) + '|' + _p('  ACCEL (g)', _A_RIGHT) + '|')
    for axis, g_key, a_key in (('X', 'gyro_x', 'accel_x'),
                                ('Y', 'gyro_y', 'accel_y'),
                                ('Z', 'gyro_z', 'accel_z')):
        g_str = f'  {axis} :  {state[g_key]:+9.3f}'
        a_str = f'  {axis} :  {state[a_key]:+7.4f}'
        lines.append('|' + _p(g_str, _A_LEFT) + '|' + _p(a_str, _A_RIGHT) + '|')
    lines.append(_SEP)

    assert len(lines) == TABLE_LINES, f"Table line count mismatch: {len(lines)}"
    return lines


def _display(state):
    """Render and write the table. On subsequent calls, jump cursor back up first."""
    global _table_active
    lines = _render(state)
    buf = []
    if _table_active:
        buf.append(f'\033[{TABLE_LINES}A')
    for line in lines:
        buf.append(f'\r\033[2K{line}\n')
    sys.stdout.write(''.join(buf))
    sys.stdout.flush()
    _table_active = True


# ---------------------------------------------------------------------------
# State parsing and diffing
# ---------------------------------------------------------------------------

def _parse(cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask,
           touch_active, touch_x, touch_y,
           gyro_x, gyro_y, gyro_z,
           accel_x, accel_y, accel_z):
    state = {
        'circle_x':    cx,
        'circle_y':    cy,
        'cpp_x':       cpp_x,
        'cpp_y':       cpp_y,
        'cpp_present': cpp_present,
        'touch_active': touch_active,
        'touch_x':     touch_x,
        'touch_y':     touch_y,
        'gyro_x':      gyro_x,
        'gyro_y':      gyro_y,
        'gyro_z':      gyro_z,
        'accel_x':     accel_x,
        'accel_y':     accel_y,
        'accel_z':     accel_z,
    }
    for name, mask in BUTTON_NAMES:
        state[name] = bool(buttons_mask & mask)
    return state


def _diff(prev, curr):
    analog = {
        'circle_x', 'circle_y', 'cpp_x', 'cpp_y',
        'touch_x', 'touch_y',
        'gyro_x', 'gyro_y', 'gyro_z',
        'accel_x', 'accel_y', 'accel_z',
    }
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
    if key in ('circle_x', 'circle_y', 'cpp_x', 'cpp_y', 'touch_x', 'touch_y'):
        return f'{val:+.3f}'
    if key in ('gyro_x', 'gyro_y', 'gyro_z'):
        return f'{val:+.1f} dps'
    if key in ('accel_x', 'accel_y', 'accel_z'):
        return f'{val:+.4f} g'
    if isinstance(val, bool):
        return 'PRESSED ' if val else 'released'
    return str(val)


# ---------------------------------------------------------------------------
# Handshake
# ---------------------------------------------------------------------------

def do_handshake(sock, ds_addr):
    sock.settimeout(HANDSHAKE_TIMEOUT_S)
    for attempt in range(1, HANDSHAKE_RETRIES + 1):
        _write_timeline(f"  [{attempt}/{HANDSHAKE_RETRIES}] Sending HELLO to {ds_addr[0]}:{ds_addr[1]} ...")
        _write_timeline(f"Handshake attempt {attempt}/{HANDSHAKE_RETRIES} -> {ds_addr[0]}:{ds_addr[1]}")
        sock.sendto(HELLO_MSG, ds_addr)
        try:
            data, addr = sock.recvfrom(64)
            if data == ACK_MSG:
                _write_timeline(f"  ACK received from {addr[0]}:{addr[1]}")
                _write_timeline(f"Handshake ACK from {addr[0]}:{addr[1]}")
                return True
            else:
                _write_timeline(f"  Unexpected response: {data!r} — ignoring")
        except socket.timeout:
            _write_timeline("  Timed out.")
            _write_timeline(f"Handshake attempt {attempt} timed out")
    return False


# ---------------------------------------------------------------------------
# Receive loop
# ---------------------------------------------------------------------------

def receive_loop(sock, ds_addr, displaylive=True):
    sock.settimeout(POLL_TIMEOUT_S)
    prev_state     = None
    last_rx        = time.monotonic()
    last_keepalive = time.monotonic()

    _write_timeline("Stream started")
    _write_timeline("\nStreaming. Ctrl+C to stop.\n")

    try:
        while True:
            # --- Keepalive: re-send HELLO every KEEPALIVE_INTERVAL_S ---
            now = time.monotonic()
            if now - last_keepalive >= KEEPALIVE_INTERVAL_S:
                sock.sendto(HELLO_MSG, ds_addr)
                last_keepalive = now

            # --- Receive ---
            try:
                data, _ = sock.recvfrom(PACKET_SIZE + 32)
                last_rx = time.monotonic()
            except socket.timeout:
                idle = time.monotonic() - last_rx
                if idle >= AUTO_TERMINATE_S:
                    if _table_active:
                        sys.stdout.write('\n')
                    msg = f"No input for {AUTO_TERMINATE_S:.0f}s — auto-terminating."
                    _write_timeline(msg)
                    return
                continue

            # --- Silently discard ACK replies to our keepalive HELLOs ---
            if data == ACK_MSG:
                continue

            # --- Parse ---
            try:
                (cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask,
                 touch_active, touch_x, touch_y,
                 gyro_x, gyro_y, gyro_z,
                 accel_x, accel_y, accel_z) = unpack_input(data)
            except ValueError:
                continue

            curr = _parse(cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask,
                          touch_active, touch_x, touch_y,
                          gyro_x, gyro_y, gyro_z,
                          accel_x, accel_y, accel_z)

            # # --- Timeline: log changes only ---
            # if prev_state is None:
            #     _write_timeline("Initial state received")
            # else:
            #     for key, old, new in _diff(prev_state, curr):
            #         _write_timeline(f"  {key:<12}  {_fmt(key, old):>14}  ->  {_fmt(key, new)}")

            # --- Display: update every packet ---
            if displaylive: _display(curr)
            prev_state = curr

    except KeyboardInterrupt:
        pass


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(ip:str=None, port: str=DEFAULT_3DS_PORT, listen: str=DEFAULT_PC_PORT, displaylive=False)->int:
    _write_timeline("=== 3DS Remote Controller - Receiver ===\n")
    _write_timeline("=== Session started ===")

    ds_ip = ip or input("3DS IP address        : ").strip()
    if not ds_ip:
        _write_timeline("No IP provided. Exiting.")
        return 1

    raw = port or input(f"3DS port       [{DEFAULT_3DS_PORT}] : ").strip()
    ds_port = int(raw) if raw else DEFAULT_3DS_PORT

    raw = listen or input(f"PC listen port [{DEFAULT_PC_PORT}] : ").strip()
    pc_port = int(raw) if raw else DEFAULT_PC_PORT

    _write_timeline(f"Targeting 3DS at {ds_ip}:{ds_port}, listening on :{pc_port}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(('', pc_port))
    except OSError as e:
        _write_timeline(f"\nFailed to bind port {pc_port}: {e}")
        sys.exit(1)

    _write_timeline(f"\nListening on :{pc_port}")
    _write_timeline(f"Attempting handshake with {ds_ip}:{ds_port} ...\n")

    try:
        if not do_handshake(sock, (ds_ip, ds_port)):
            msg = f"Handshake failed after {HANDSHAKE_RETRIES} attempts."
            _write_timeline(msg)
            _write_timeline(msg)
            sys.exit(1)

        _write_timeline(f"Connected to 3DS at {ds_ip}:{ds_port}")
        _write_timeline(f"Connected to 3DS at {ds_ip}:{ds_port}")
        receive_loop(sock, (ds_ip, ds_port), displaylive=displaylive)

    except KeyboardInterrupt:
        pass
    finally:
        if _table_active:
            sys.stdout.write('\n')
        _write_timeline("=== Session ended ===")
        sock.close()


if __name__ == '__main__':
    set_timeline(print)
    main(displaylive=True)