// 3DS Remote Controller
// Streams all 3DS inputs as UDP packets to a PC running receiver.py.
//
// Protocol: see Controller/pc/protocol.py
// Packet layout (55 bytes, big-endian):
//   [0-1]   Magic        0x3D 0x53
//   [2]     Version      2
//   [3]     cpp_present  (0 or 1)
//   [4-7]   Circle pad X (float, -1..+1)
//   [8-11]  Circle pad Y (float, -1..+1)
//   [12-15] CPP/C-stick X (float, -1..+1)
//   [16-19] CPP/C-stick Y (float, -1..+1)
//   [20-21] Button bitmask (uint16)
//   [22]    touch_active  (0 or 1)
//   [23-26] touch_x       (float, 0..1, px / 319)
//   [27-30] touch_y       (float, 0..1, py / 239)
//   [31-34] gyro_x        (float, dps, roll)
//   [35-38] gyro_y        (float, dps, pitch)
//   [39-42] gyro_z        (float, dps, yaw)
//   [43-46] accel_x       (float, g-force, raw / 512)
//   [47-50] accel_y       (float, g-force, raw / 512)
//   [51-54] accel_z       (float, g-force, raw / 512)
//
// Button bits: A=0 B=1 X=2 Y=3 UP=4 DOWN=5 LEFT=6 RIGHT=7
//              L=8 R=9 ZL=10 ZR=11 SELECT=12 START=13

#include "platform.h"
#include "log.h"

#include <3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Protocol constants
// ---------------------------------------------------------------------------

static constexpr uint16_t CTRL_PORT   = 6000;
static constexpr uint8_t  MAGIC_0     = 0x3D;
static constexpr uint8_t  MAGIC_1     = 0x53;
static constexpr uint8_t  PROTO_VER   = 2;
static constexpr int      PACKET_SIZE = 55;

static constexpr float TOUCH_NORM_X = 1.0f / 319.0f;  // px / 319 -> 0..1
static constexpr float TOUCH_NORM_Y = 1.0f / 239.0f;  // py / 239 -> 0..1
static constexpr float ACCEL_NORM   = 1.0f / 512.0f;  // 512 LSB per g

static const char HELLO_MSG[] = "HELLO_3DS";
static const char ACK_MSG[]   = "ACK_3DS";
static constexpr int HELLO_LEN = 9;
static constexpr int ACK_LEN   = 7;

// Button bitmask positions — must match protocol.py
static constexpr uint16_t BTN_A      = 1 << 0;
static constexpr uint16_t BTN_B      = 1 << 1;
static constexpr uint16_t BTN_X      = 1 << 2;
static constexpr uint16_t BTN_Y      = 1 << 3;
static constexpr uint16_t BTN_UP     = 1 << 4;
static constexpr uint16_t BTN_DOWN   = 1 << 5;
static constexpr uint16_t BTN_LEFT   = 1 << 6;
static constexpr uint16_t BTN_RIGHT  = 1 << 7;
static constexpr uint16_t BTN_L      = 1 << 8;
static constexpr uint16_t BTN_R      = 1 << 9;
static constexpr uint16_t BTN_ZL     = 1 << 10;
static constexpr uint16_t BTN_ZR     = 1 << 11;
static constexpr uint16_t BTN_SELECT = 1 << 12;
static constexpr uint16_t BTN_START  = 1 << 13;

// ---------------------------------------------------------------------------
// App state
// ---------------------------------------------------------------------------

enum AppState
{
	STATE_WAITING_WIFI,
	STATE_LISTENING,
	STATE_STREAMING,
};

static int              g_sock           = -1;
static sockaddr_in      g_pcAddr         = {};
static AppState         g_state          = STATE_WAITING_WIFI;
static bool             g_cppPresent     = false;
static bool             g_gyroReady      = false;
static float            g_gyroCoeff      = 0.0f;  // raw / coeff = dps
static bool             g_accelReady     = false;
static u64              g_lastHelloMs    = 0;      // osGetTime() of last PC keepalive
static constexpr u64    HELLO_TIMEOUT_MS = 30000;  // drop back to listening after 30s

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static float clampf (float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

// Convert float to big-endian 32-bit word (network order)
static uint32_t f32_to_be (float f)
{
	uint32_t i;
	std::memcpy (&i, &f, 4);
	return htonl (i);
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

static void updateStatusBar ()
{
	consoleSelect (&g_statusConsole);
	switch (g_state)
	{
	case STATE_WAITING_WIFI:
		std::printf ("\x1b[0;0H\x1b[33;1mWaiting for WiFi...                               \x1b[K");
		break;

	case STATE_LISTENING:
	{
		struct in_addr ia;
		ia.s_addr = gethostid ();
		std::printf ("\x1b[0;0H\x1b[36;1mListening  \x1b[37;1m%s:%-5u\x1b[K",
		    inet_ntoa (ia), (unsigned)CTRL_PORT);
		break;
	}

	case STATE_STREAMING:
		std::printf ("\x1b[0;0H\x1b[32;1mStreaming  \x1b[37;1m-> %s\x1b[K",
		    inet_ntoa (g_pcAddr.sin_addr));
		break;
	}
	std::fflush (stdout);
}

static void updateBottomScreen ()
{
	consoleSelect (&g_sessionConsole);
	std::printf ("\x1b[2J"); // clear screen

	switch (g_state)
	{
	case STATE_WAITING_WIFI:
		std::printf ("\x1b[33;1mWaiting for WiFi...\n\n"
		             "\x1b[37;1mEnsure the 3DS is\n"
		             "connected to a WiFi\n"
		             "network.\n");
		break;

	case STATE_LISTENING:
	{
		struct in_addr ia;
		ia.s_addr = gethostid ();
		std::printf ("\x1b[36;1mWaiting for PC...\n\n"
		             "\x1b[37;1mEnter in receiver.py:\n\n"
		             "  IP  : \x1b[32;1m%s\x1b[37;1m\n"
		             "  Port: \x1b[32;1m%u\x1b[37;1m\n\n"
		             "Press HOME to exit.",
		    inet_ntoa (ia), (unsigned)CTRL_PORT);
		break;
	}

	case STATE_STREAMING:
		std::printf ("\x1b[32;1mStreaming to:\n\n"
		             "  \x1b[37;1m%s\n\n"
		             "\x1b[37;1mPress HOME to exit.\n\n"
		             "\x1b[36;1mTip: you can drag on\n"
		             "the touchpad here :)",
		    inet_ntoa (g_pcAddr.sin_addr));
		break;
	}
	std::fflush (stdout);
}

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

static bool createSocket ()
{
	g_sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (g_sock < 0)
	{
		error ("socket: %d %s\n", errno, strerror (errno));
		return false;
	}

	int reuse = 1;
	setsockopt (g_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));

	sockaddr_in addr = {};
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons (CTRL_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind (g_sock, reinterpret_cast<sockaddr *> (&addr), sizeof (addr)) < 0)
	{
		error ("bind: %d %s\n", errno, strerror (errno));
		close (g_sock);
		g_sock = -1;
		return false;
	}

	// Non-blocking so the main loop never stalls waiting for a packet
	fcntl (g_sock, F_SETFL, fcntl (g_sock, F_GETFL, 0) | O_NONBLOCK);

	struct in_addr ia;
	ia.s_addr = gethostid ();
	info ("Bound UDP socket on %s:%u\n", inet_ntoa (ia), (unsigned)CTRL_PORT);

	return true;
}

// ---------------------------------------------------------------------------
// Input → packet
// ---------------------------------------------------------------------------

static void sendInputPacket ()
{
	// platform::loop() already called hidScanInput() this frame
	u32 kHeld = hidKeysHeld ();

	// --- Circle pad ---
	circlePosition circle = {};
	hidCircleRead (&circle);

	// --- CPP / C-stick ---
	circlePosition cstick = {};
	if (g_cppPresent)
	{
		irrstScanInput ();
		irrstCstickRead (&cstick);
	}

	// Normalize circle pad: libctru range is roughly ±155
	float cx    = clampf ((float)circle.dx / 155.0f, -1.0f, 1.0f);
	float cy    = clampf ((float)circle.dy / 155.0f, -1.0f, 1.0f);
	float cpp_x = clampf ((float)cstick.dx / 155.0f, -1.0f, 1.0f);
	float cpp_y = clampf ((float)cstick.dy / 155.0f, -1.0f, 1.0f);

	// --- Buttons ---
	uint16_t buttons = 0;
	if (kHeld & KEY_A)      buttons |= BTN_A;
	if (kHeld & KEY_B)      buttons |= BTN_B;
	if (kHeld & KEY_X)      buttons |= BTN_X;
	if (kHeld & KEY_Y)      buttons |= BTN_Y;
	if (kHeld & KEY_DUP)    buttons |= BTN_UP;
	if (kHeld & KEY_DDOWN)  buttons |= BTN_DOWN;
	if (kHeld & KEY_DLEFT)  buttons |= BTN_LEFT;
	if (kHeld & KEY_DRIGHT) buttons |= BTN_RIGHT;
	if (kHeld & KEY_L)      buttons |= BTN_L;
	if (kHeld & KEY_R)      buttons |= BTN_R;
	if (kHeld & KEY_ZL)     buttons |= BTN_ZL;
	if (kHeld & KEY_ZR)     buttons |= BTN_ZR;
	if (kHeld & KEY_SELECT) buttons |= BTN_SELECT;
	if (kHeld & KEY_START)  buttons |= BTN_START;

	// --- Touchscreen ---
	bool         touch_active = (kHeld & KEY_TOUCH) != 0;
	touchPosition touch       = {};
	hidTouchRead (&touch);
	float touch_x = touch_active ? clampf ((float)touch.px * TOUCH_NORM_X, 0.0f, 1.0f) : 0.0f;
	float touch_y = touch_active ? clampf ((float)touch.py * TOUCH_NORM_Y, 0.0f, 1.0f) : 0.0f;

	// --- Gyroscope (degrees per second) ---
	float gyro_x = 0.0f, gyro_y = 0.0f, gyro_z = 0.0f;
	if (g_gyroReady)
	{
		angularRate gyro = {};
		hidGyroRead (&gyro);
		gyro_x = (float)gyro.x / g_gyroCoeff;
		gyro_y = (float)gyro.y / g_gyroCoeff;
		gyro_z = (float)gyro.z / g_gyroCoeff;
	}

	// --- Accelerometer (g-force) ---
	float accel_x = 0.0f, accel_y = 0.0f, accel_z = 0.0f;
	if (g_accelReady)
	{
		accelVector accel = {};
		hidAccelRead (&accel);
		accel_x = (float)accel.x * ACCEL_NORM;
		accel_y = (float)accel.y * ACCEL_NORM;
		accel_z = (float)accel.z * ACCEL_NORM;
	}

	// --- Pack into 55-byte big-endian payload matching Python struct '!2sBBffffHBffffffff' ---
	uint8_t  pkt[PACKET_SIZE];
	uint32_t cx_be       = f32_to_be (cx);
	uint32_t cy_be       = f32_to_be (cy);
	uint32_t cppx_be     = f32_to_be (cpp_x);
	uint32_t cppy_be     = f32_to_be (cpp_y);
	uint16_t btn_be      = htons (buttons);
	uint32_t touch_x_be  = f32_to_be (touch_x);
	uint32_t touch_y_be  = f32_to_be (touch_y);
	uint32_t gyro_x_be   = f32_to_be (gyro_x);
	uint32_t gyro_y_be   = f32_to_be (gyro_y);
	uint32_t gyro_z_be   = f32_to_be (gyro_z);
	uint32_t accel_x_be  = f32_to_be (accel_x);
	uint32_t accel_y_be  = f32_to_be (accel_y);
	uint32_t accel_z_be  = f32_to_be (accel_z);

	pkt[0] = MAGIC_0;
	pkt[1] = MAGIC_1;
	pkt[2] = PROTO_VER;
	pkt[3] = g_cppPresent ? 1 : 0;
	std::memcpy (pkt + 4,  &cx_be,      4);
	std::memcpy (pkt + 8,  &cy_be,      4);
	std::memcpy (pkt + 12, &cppx_be,    4);
	std::memcpy (pkt + 16, &cppy_be,    4);
	std::memcpy (pkt + 20, &btn_be,     2);
	pkt[22] = touch_active ? 1 : 0;
	std::memcpy (pkt + 23, &touch_x_be, 4);
	std::memcpy (pkt + 27, &touch_y_be, 4);
	std::memcpy (pkt + 31, &gyro_x_be,  4);
	std::memcpy (pkt + 35, &gyro_y_be,  4);
	std::memcpy (pkt + 39, &gyro_z_be,  4);
	std::memcpy (pkt + 43, &accel_x_be, 4);
	std::memcpy (pkt + 47, &accel_y_be, 4);
	std::memcpy (pkt + 51, &accel_z_be, 4);

	sendto (g_sock, pkt, PACKET_SIZE, 0,
	    reinterpret_cast<const sockaddr *> (&g_pcAddr), sizeof (g_pcAddr));
}

// ---------------------------------------------------------------------------
// State machine tick (called once per frame)
// ---------------------------------------------------------------------------

static void controllerTick ()
{
	switch (g_state)
	{
	case STATE_WAITING_WIFI:
		// Wait for socInit to complete (done by platform::loop → startNetwork)
		if (platform::networkVisible ())
		{
			if (createSocket ())
			{
				g_state = STATE_LISTENING;
				updateBottomScreen ();
			}
			// If createSocket fails, stay in WAITING_WIFI and retry next frame
		}
		break;

	case STATE_LISTENING:
	{
		char       buf[32];
		sockaddr_in from    = {};
		socklen_t   fromLen = sizeof (from);

		int n = recvfrom (g_sock, buf, (int)sizeof (buf) - 1, 0,
		    reinterpret_cast<sockaddr *> (&from), &fromLen);

		if (n == HELLO_LEN && std::memcmp (buf, HELLO_MSG, HELLO_LEN) == 0)
		{
			g_pcAddr      = from;
			g_lastHelloMs = osGetTime ();
			sendto (g_sock, ACK_MSG, ACK_LEN, 0,
			    reinterpret_cast<const sockaddr *> (&from), fromLen);

			info ("Connected: %s:%u\n",
			    inet_ntoa (from.sin_addr), (unsigned)ntohs (from.sin_port));
			info ("Streaming started\n");

			g_state = STATE_STREAMING;
			updateBottomScreen ();
		}
		break;
	}

	case STATE_STREAMING:
	{
		// Check for keepalive HELLO from PC (non-blocking, socket is O_NONBLOCK)
		char        buf[32]  = {};
		sockaddr_in from     = {};
		socklen_t   fromLen  = sizeof (from);
		int n = recvfrom (g_sock, buf, (int)sizeof (buf) - 1, 0,
		    reinterpret_cast<sockaddr *> (&from), &fromLen);
		if (n == HELLO_LEN && std::memcmp (buf, HELLO_MSG, HELLO_LEN) == 0)
		{
			g_lastHelloMs = osGetTime ();
			g_pcAddr      = from;  // update in case receiver restarted on a new port
			sendto (g_sock, ACK_MSG, ACK_LEN, 0,
			    reinterpret_cast<const sockaddr *> (&from), fromLen);
			info ("Connection established (%s)\n", inet_ntoa (from.sin_addr));
		}

		// Timeout: if no keepalive for 30s, stop streaming and wait for reconnect
		if (osGetTime () - g_lastHelloMs > HELLO_TIMEOUT_MS)
		{
			info ("PC timed out - waiting for reconnect\n");
			g_state = STATE_LISTENING;
			updateBottomScreen ();
			break;
		}

		sendInputPacket ();
		break;
	}
	}
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main ()
{
	if (!platform::init ())
		return EXIT_FAILURE;

	// --- C-stick / Circle Pad Pro ---
	if (R_SUCCEEDED (irrstInit ()))
	{
		g_cppPresent = true;
		info ("C-stick active (N3DS / CPP)\n");
	}
	else
	{
		info ("No C-stick detected (O3DS)\n");
	}

	// --- Gyroscope ---
	if (R_SUCCEEDED (HIDUSER_EnableGyroscope ()))
	{
		float coeff = 0.0f;
		if (R_SUCCEEDED (HIDUSER_GetGyroscopeRawToDpsCoefficient (&coeff)) && coeff != 0.0f)
		{
			g_gyroCoeff = coeff;
			g_gyroReady = true;
			info ("Gyroscope enabled (coeff=%.5f)\n", g_gyroCoeff);
		}
		else
		{
			info ("Gyroscope: failed to get DPS coefficient\n");
			HIDUSER_DisableGyroscope ();
		}
	}
	else
	{
		info ("Gyroscope not available\n");
	}

	// --- Accelerometer ---
	if (R_SUCCEEDED (HIDUSER_EnableAccelerometer ()))
	{
		g_accelReady = true;
		info ("Accelerometer enabled\n");
	}
	else
	{
		info ("Accelerometer not available\n");
	}

	info ("3DS Remote Controller ready (protocol v%u, %d-byte packets)\n",
	    (unsigned)PROTO_VER, PACKET_SIZE);
	info ("Waiting for WiFi...\n");

	updateStatusBar ();
	updateBottomScreen ();

	while (platform::loop ())
	{
		updateStatusBar ();
		controllerTick ();
		drawLog ();
		platform::render ();
	}

	if (g_accelReady)
		HIDUSER_DisableAccelerometer ();
	if (g_gyroReady)
		HIDUSER_DisableGyroscope ();
	if (g_cppPresent)
		irrstExit ();
	if (g_sock >= 0)
		close (g_sock);

	platform::exit ();
	return EXIT_SUCCESS;
}
