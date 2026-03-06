// Adapted from ftpd/include/platform.h
// Removed SockAddr dependency and unused Switch/NDS sections.
#pragma once

#include <3ds.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// CLASSIC console handles — defined in platform.cpp, used by log.cpp and main.cpp
extern PrintConsole g_statusConsole;
extern PrintConsole g_logConsole;
extern PrintConsole g_sessionConsole;

namespace platform
{
bool init ();

bool networkVisible ();

bool loop ();

void render ();

void exit ();

// 3DS steady clock backed by svcGetSystemTick
struct steady_clock
{
	using rep        = std::uint64_t;
	using period     = std::ratio<1, SYSCLOCK_ARM11>;
	using duration   = std::chrono::duration<rep, period>;
	using time_point = std::chrono::time_point<steady_clock>;

	constexpr static bool is_steady = true;

	static time_point now () noexcept;
};

class Thread
{
public:
	~Thread ();
	Thread ();
	Thread (std::function<void ()> &&func_);
	Thread (Thread const &) = delete;
	Thread (Thread &&that_);
	Thread &operator= (Thread const &) = delete;
	Thread &operator= (Thread &&that_);
	void join ();
	static void sleep (std::chrono::milliseconds timeout_);

private:
	class privateData_t;
	std::unique_ptr<privateData_t> m_d;
};

class Mutex
{
public:
	~Mutex ();
	Mutex ();
	void lock ();
	void unlock ();

private:
	class privateData_t;
	std::unique_ptr<privateData_t> m_d;
};
}
