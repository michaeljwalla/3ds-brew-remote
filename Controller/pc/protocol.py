"""
Shared packet protocol for the 3DS remote controller.

Packet layout (22 bytes, big-endian / network order):
  Offset  Size  Type    Field
  ------  ----  ------  -----
  0       2     bytes   Magic: 0x3D 0x53
  2       1     uint8   Protocol version (currently 1)
  3       4     float   Circle pad X, normalized -1.0 .. +1.0
  7       4     float   Circle pad Y, normalized -1.0 .. +1.0
  11      4     float   CPP X, normalized -1.0 .. +1.0 (0 if not present)
  15      4     float   CPP Y, normalized -1.0 .. +1.0 (0 if not present)
  19      1     uint8   CPP present flag (0 or 1)
  20      2     uint16  Button bitmask (see BTN_* constants below)
  Total: 22 bytes

Handshake (UDP):
  PC  -> 3DS port 6000 : HELLO_MSG
  3DS -> PC  port 6001 : ACK_MSG   (3DS learns PC address from sender field)
  PC retries every 1s, up to 5 times.
  After ACK, 3DS streams input packets to the PC address it captured.
"""

import struct

MAGIC   = b'\x3D\x53'
VERSION = 1

# fmt: !  = network (big-endian)
#      2s = magic, B = version, ffff = 4 floats, B = cpp_present, H = buttons
PACKET_FORMAT = '!2sBffffBH'
PACKET_SIZE   = struct.calcsize(PACKET_FORMAT)  # 22 bytes

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


def pack_input(cx, cy, cpp_x, cpp_y, cpp_present, buttons_mask):
    """Pack controller input into a 22-byte UDP payload."""
    return struct.pack(
        PACKET_FORMAT,
        MAGIC, VERSION,
        cx, cy,
        cpp_x, cpp_y,
        1 if cpp_present else 0,
        buttons_mask & 0xFFFF,
    )


def unpack_input(data):
    """
    Unpack a 22-byte controller input payload.
    Returns (cx, cy, cpp_x, cpp_y, cpp_present: bool, buttons_mask: int).
    Raises ValueError on bad magic, wrong version, or wrong size.
    """
    if len(data) != PACKET_SIZE:
        raise ValueError(f"Expected {PACKET_SIZE} bytes, got {len(data)}")
    magic, version, cx, cy, cpp_x, cpp_y, cpp_present, buttons = struct.unpack(
        PACKET_FORMAT, data
    )
    if magic != MAGIC:
        raise ValueError(f"Invalid magic: {magic!r}")
    if version != VERSION:
        raise ValueError(f"Unsupported protocol version: {version} (expected {VERSION})")
    return cx, cy, cpp_x, cpp_y, bool(cpp_present), buttons
