// Adapted from ftpd/source/3ds/platform.cpp
// Changes from original:
//   - Removed ftpd-specific includes (fs.h, ftpServer.h, imgui_*, citro3d, tex3ds)
//   - Removed platform::networkAddress() (unused; main.cpp calls gethostid() directly)
//   - Removed KEY_START exit from platform::loop() — app exits via HOME/APT only
//   - All non-CLASSIC (#ifndef CLASSIC) code removed for clarity

#include "platform.h"
#include "log.h"

#include <arpa/inet.h>
#include <malloc.h>

#include <algorithm>

#include <atomic>
#include <chrono>
#include <cstring>
#include <ctime>
#include <mutex>

PrintConsole g_statusConsole;
PrintConsole g_logConsole;
PrintConsole g_sessionConsole;

namespace
{
constexpr auto STACK_SIZE      = 0x8000;
constexpr auto SOCU_ALIGN      = 0x1000;
constexpr auto SOCU_BUFFERSIZE = 0x100000;

static_assert (SOCU_BUFFERSIZE % SOCU_ALIGN == 0);

bool s_ndmuLocked = false;

std::atomic_bool s_socuActive = false;
u32             *s_socuBuffer = nullptr;
platform::Mutex  s_acuFence;

bool s_backlight = true;

aptHookCookie s_aptHookCookie;

// Only used in CLASSIC status line
in_addr_t s_addr = 0;

void enableBacklight (bool const enable_)
{
	if (R_FAILED (gspLcdInit ()))
		return;

	(enable_ ? GSPLCD_PowerOnBacklight : GSPLCD_PowerOffBacklight) (GSPLCD_SCREEN_BOTH);

	gspLcdExit ();
}

void handleAPTHook (APT_HookType const type_, void *const param_)
{
	(void)param_;

	switch (type_)
	{
	case APTHOOK_ONSUSPEND:
	case APTHOOK_ONSLEEP:
		if (!s_backlight)
			enableBacklight (true);
		break;

	case APTHOOK_ONRESTORE:
	case APTHOOK_ONWAKEUP:
		enableBacklight (s_backlight);
		break;

	default:
		break;
	}
}

bool getNetworkVisibility ()
{
	auto const lock = std::scoped_lock (s_acuFence);

	static std::uint32_t lastWifi = 0;
	static Result        lastResult = 0;

	std::uint32_t wifi   = 0;
	auto const    result = ACU_GetWifiStatus (&wifi);
	if (result != lastResult)
		info ("ACU_GetWifiStatus: result 0x%lx -> 0x%lx\n", lastResult, result);
	lastResult = result;

	if (R_SUCCEEDED (result))
	{
		if (wifi != lastWifi)
			info ("ACU_GetWifiStatus: wifi 0x%lx -> 0x%lx\n", lastWifi, wifi);
		lastWifi = wifi;
	}

	if (R_FAILED (result) || !wifi)
	{
		s_addr = 0;
		return false;
	}

	if (!s_addr)
		s_addr = gethostid ();

	if (s_addr == INADDR_BROADCAST)
		s_addr = 0;

	return true;
}

void startNetwork ()
{
	if (s_socuActive)
		return;

	if (!getNetworkVisibility ())
		return;

	if (!s_socuBuffer)
		s_socuBuffer = static_cast<u32 *> (::memalign (SOCU_ALIGN, SOCU_BUFFERSIZE));

	if (!s_socuBuffer)
		return;

	if (R_FAILED (socInit (s_socuBuffer, SOCU_BUFFERSIZE)))
		return;

	aptSetSleepAllowed (false);

	Result res;
	if (R_FAILED (res = NDMU_EnterExclusiveState (NDM_EXCLUSIVE_STATE_INFRASTRUCTURE)))
		error ("Failed to enter exclusive NDM state: 0x%lx\n", res);
	else if (R_FAILED (res = NDMU_LockState ()))
	{
		error ("Failed to lock NDM: 0x%lx\n", res);
		NDMU_LeaveExclusiveState ();
	}
	else
		s_ndmuLocked = true;

	s_socuActive = true;
	info ("WiFi connected\n");
}
}

bool platform::init ()
{
	osSetSpeedupEnable (true);

	acInit ();
	ndmuInit ();
	ptmuInit ();

	gfxInit (GSP_BGR8_OES, GSP_BGR8_OES, false);
	gfxSet3D (false);

	consoleInit (GFX_TOP, &g_statusConsole);
	consoleInit (GFX_TOP, &g_logConsole);
	consoleInit (GFX_BOTTOM, &g_sessionConsole);

	// Status bar: top row only
	consoleSetWindow (&g_statusConsole, 0, 0, 50, 1);
	// Log: remaining rows on top screen
	consoleSetWindow (&g_logConsole, 0, 1, 50, 29);
	// Bottom screen: full 40×30 for IP display
	consoleSetWindow (&g_sessionConsole, 0, 0, 40, 30);

	aptHook (&s_aptHookCookie, handleAPTHook, nullptr);

	return true;
}

bool platform::networkVisible ()
{
	if (!s_socuActive)
		return false;

	return getNetworkVisibility ();
}


bool platform::loop ()
{
	if (!aptMainLoop ())
		return false;

	startNetwork ();

	hidScanInput ();

	return true;
}

void platform::render ()
{
	gfxFlushBuffers ();
	gspWaitForVBlank ();
	gfxSwapBuffers ();
}

void platform::exit ()
{
	if (s_ndmuLocked)
	{
		NDMU_UnlockState ();
		NDMU_LeaveExclusiveState ();
		aptSetSleepAllowed (true);
		s_ndmuLocked = false;
	}

	if (s_socuActive)
	{
		socExit ();
		s_socuActive = false;
	}

	std::free (s_socuBuffer);

	aptUnhook (&s_aptHookCookie);

	if (!s_backlight)
		enableBacklight (true);

	gfxExit ();
	ptmuExit ();
	ndmuExit ();
	acExit ();
}

platform::steady_clock::time_point platform::steady_clock::now () noexcept
{
	return time_point (duration (svcGetSystemTick ()));
}

// --- Thread ---

class platform::Thread::privateData_t
{
public:
	privateData_t ()
	{
		if (thread)
			threadFree (thread);
	}

	privateData_t (std::function<void ()> &&func_) : thread (nullptr), func (std::move (func_))
	{
		s32 priority = 0x30;
		svcGetThreadPriority (&priority, CUR_THREAD_HANDLE);
		priority = std::clamp<s32> (priority, 0x18, 0x3F - 1) + 1;
		thread   = threadCreate (&privateData_t::threadFunc, this, STACK_SIZE, priority, 0, false);
	}

	static void threadFunc (void *const arg_)
	{
		auto const t = static_cast<privateData_t *> (arg_);
		t->func ();
	}

	::Thread thread = nullptr;
	std::function<void ()> func;
};

platform::Thread::~Thread () = default;
platform::Thread::Thread () : m_d (new privateData_t ()) {}
platform::Thread::Thread (std::function<void ()> &&func_) : m_d (new privateData_t (std::move (func_))) {}
platform::Thread::Thread (Thread &&that_) : m_d (new privateData_t ()) { std::swap (m_d, that_.m_d); }
platform::Thread &platform::Thread::operator= (Thread &&that_) { std::swap (m_d, that_.m_d); return *this; }
void platform::Thread::join () { threadJoin (m_d->thread, UINT64_MAX); }
void platform::Thread::sleep (std::chrono::milliseconds const timeout_)
{
	svcSleepThread (std::chrono::nanoseconds (timeout_).count ());
}

// --- Mutex ---

class platform::Mutex::privateData_t
{
public:
	LightLock mutex;
};

platform::Mutex::~Mutex () = default;
platform::Mutex::Mutex () : m_d (new privateData_t ()) { LightLock_Init (&m_d->mutex); }
void platform::Mutex::lock ()   { LightLock_Lock   (&m_d->mutex); }
void platform::Mutex::unlock () { LightLock_Unlock (&m_d->mutex); }
