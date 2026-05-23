// Umbrella header for the self-contained Cemuhook / DSU (DualShock UDP)
// server protocol library. Include this to pull in all three layers.
//
// See PROTOCOL.md (the wire contract) and DESIGN.md (the abstraction design)
// in this directory.
#pragma once

#include "Connection.h"  // L2+L3: Connection, Endpoint, SlotConfig
#include "Messages.h"    // L1: ControllerData, ButtonState, parse_client_packet
#include "Protocol.h"    // L0: crc32, ByteWriter/Reader, frame_packet
