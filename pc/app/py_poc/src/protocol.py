"""
Shared packet protocol for the 3DS remote controller.

Packet layout (55 bytes, big-endian / network order):
  Offset  Size  Type    Field
  ------  ----  ------  -----
  0       2     bytes   Magic: 0x3D 0x53
  2       1     uint8   Protocol version (currently 2)
  3       1     uint8   cpp_present: 1 if Circle Pad Pro / C-stick present, else 0
  4       4     float   Circle pad X, normalized -1.0 .. +1.0
  8       4     float   Circle pad Y, normalized -1.0 .. +1.0
  12      4     float   CPP X, normalized -1.0 .. +1.0 (meaningless if cpp_present=0)
  16      4     float   CPP Y, normalized -1.0 .. +1.0 (meaningless if cpp_present=0)
  20      2     uint16  Button bitmask (see BTN_* constants below)
  22      1     uint8   touch_active: 1 if touchscreen is touched, else 0
  23      4     float   touch_x, normalized 0.0 .. 1.0 (meaningless if touch_active=0)
  27      4     float   touch_y, normalized 0.0 .. 1.0 (meaningless if touch_active=0)
  31      4     float   gyro_x, degrees per second (roll)
  35      4     float   gyro_y, degrees per second (pitch)
  39      4     float   gyro_z, degrees per second (yaw)
  43      4     float   accel_x, g-force
  47      4     float   accel_y, g-force
  51      4     float   accel_z, g-force
  Total: 55 bytes

  Touch screen physical dimensions: TOUCH_SCREEN_W x TOUCH_SCREEN_H pixels.
  Normalized touch: touch_x = px / (TOUCH_SCREEN_W - 1)
                    touch_y = py / (TOUCH_SCREEN_H - 1)
  Use KEY_TOUCH (hidKeysHeld) to detect active touch; touch_active reflects this.

  Gyroscope:     3DS raw s16 / HIDUSER_GetGyroRawToDpsCoefficient() = dps.
  Accelerometer: 3DS raw s16 / 512.0 = g-force (512 LSB per g).

  cpp_present is placed before the CPP axes so the receiver knows whether
  to trust the following values before reading them.

Handshake (UDP):
  PC  -> 3DS port 6000 : HELLO_MSG
  3DS -> PC  port 6001 : ACK_MSG   (3DS learns PC address from sender field)
  PC retries every 1s, up to 5 times.
  After ACK, 3DS streams input packets to the PC address it captured.
"""

import struct

MAGIC   = b'\x3D\x53'
VERSION = 2

TOUCH_SCREEN_W = 320   # bottom-screen pixel width  (px range: 0 .. 319)
TOUCH_SCREEN_H = 240   # bottom-screen pixel height (py range: 0 .. 239)

# fmt: !  = network (big-endian)
#      2s = magic, B = version, B = cpp_present, ffff = 4 floats, H = buttons,
#      B  = touch_active, ffffffff = 8 floats (touch xy, gyro xyz, accel xyz)
PACKET_FORMAT = '!2sBBffffHBffffffff'
PACKET_SIZE   = struct.calcsize(PACKET_FORMAT)  # 55 bytes

HELLO_MSG = b'HELLO_3DS'
ACK_MSG   = b'ACK_3DS'

DEFAULT_3DS_PORT = 6000   # 3DS listens here
DEFAULT_PC_PORT  = 6001   # PC listens here; 3DS streams back to this port

# Button bitmask positions
BTN_A      = 1 << 0
BTN_B      = 1 << 1
BTN_X      = 1 << 2
BTN_Y      = 1 << 3
BTN_UP     = 1 << 4
BTN_DOWN   = 1 << 5
BTN_LEFT   = 1 << 6
BTN_RIGHT  = 1 << 7
BTN_L      = 1 << 8
BTN_R      = 1 << 9
BTN_ZL     = 1 << 10
BTN_ZR     = 1 << 11
BTN_SELECT = 1 << 12
BTN_START  = 1 << 13

# Ordered list used for formatting (display order)
BUTTON_NAMES = [
    ('A',      BTN_A),
    ('B',      BTN_B),
    ('X',      BTN_X),
    ('Y',      BTN_Y),
    ('UP',     BTN_UP),
    ('DOWN',   BTN_DOWN),
    ('LEFT',   BTN_LEFT),
    ('RIGHT',  BTN_RIGHT),
    ('L',      BTN_L),
    ('R',      BTN_R),
    ('ZL',     BTN_ZL),
    ('ZR',     BTN_ZR),
    ('SELECT', BTN_SELECT),
    ('START',  BTN_START),
]


def pack_input(cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask,
               touch_active, touch_x, touch_y,
               gyro_x, gyro_y, gyro_z,
               accel_x, accel_y, accel_z):
    """Pack controller input into a 55-byte UDP payload."""
    return struct.pack(
        PACKET_FORMAT,
        MAGIC, VERSION,
        1 if cpp_present else 0,
        cx, cy, cpp_x, cpp_y,
        buttons_mask & 0xFFFF,
        1 if touch_active else 0,
        touch_x, touch_y,
        gyro_x, gyro_y, gyro_z,
        accel_x, accel_y, accel_z,
    )


def unpack_input(data):
    """
    Unpack a 55-byte controller input payload.
    Returns:
      (cx, cy, cpp_x, cpp_y, cpp_present: bool, buttons_mask: int,
       touch_active: bool, touch_x: float, touch_y: float,
       gyro_x: float, gyro_y: float, gyro_z: float,
       accel_x: float, accel_y: float, accel_z: float)
    Raises ValueError on bad magic, wrong version, or wrong size.
    """
    if len(data) != PACKET_SIZE:
        raise ValueError(f"Expected {PACKET_SIZE} bytes, got {len(data)}")
    (magic, version, cpp_present, cx, cy, cpp_x, cpp_y, buttons,
     touch_active, touch_x, touch_y,
     gyro_x, gyro_y, gyro_z,
     accel_x, accel_y, accel_z) = struct.unpack(PACKET_FORMAT, data)
    if magic != MAGIC:
        raise ValueError(f"Invalid magic: {magic!r}")
    if version != VERSION:
        raise ValueError(f"Unsupported protocol version: {version} (expected {VERSION})")
    return (cx, cy, cpp_x, cpp_y, bool(cpp_present), buttons,
            bool(touch_active), touch_x, touch_y,
            gyro_x, gyro_y, gyro_z,
            accel_x, accel_y, accel_z)
