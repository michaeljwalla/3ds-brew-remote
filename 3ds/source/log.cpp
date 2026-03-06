// Copied from ftpd/source/log.cpp (CLASSIC build only — imgui sections compiled out)
#include "log.h"
#include "platform.h"

#include <mutex>
#include <vector>

namespace
{
#ifdef __3DS__
constexpr auto MAX_LOGS = 250;
#else
constexpr auto MAX_LOGS = 10000;
#endif

bool s_logUpdated = true;

static char const *const s_prefix[] = {
    [DEBUG]    = "[DEBUG]",
    [INFO]     = "[INFO]",
    [ERROR]    = "[ERROR]",
    [COMMAND]  = "[COMMAND]",
    [RESPONSE] = "[RESPONSE]",
};

struct Message
{
	Message (LogLevel const level_, std::string message_)
	    : level (level_), message (std::move (message_))
	{
	}

	LogLevel level;
	std::string message;
};

std::vector<Message> s_messages;
platform::Mutex s_lock;
}

void drawLog ()
{
	auto const lock = std::scoped_lock (s_lock);

	if (!s_logUpdated)
		return;

	s_logUpdated = false;

	auto const maxLogs = g_logConsole.windowHeight;

	if (s_messages.size () > static_cast<unsigned> (maxLogs))
	{
		auto const begin = std::begin (s_messages);
		auto const end   = std::next (begin, s_messages.size () - maxLogs);
		s_messages.erase (begin, end);
	}

	char const *const s_colors[] = {
	    [DEBUG]    = "\x1b[33;1m",
	    [INFO]     = "\x1b[37;1m",
	    [ERROR]    = "\x1b[31;1m",
	    [COMMAND]  = "\x1b[32;1m",
	    [RESPONSE] = "\x1b[36;1m",
	};

	auto it = std::begin (s_messages);
	if (s_messages.size () > static_cast<unsigned> (g_logConsole.windowHeight))
		it = std::next (it, s_messages.size () - g_logConsole.windowHeight);

	consoleSelect (&g_logConsole);
	std::fputs ("\x1b[2J", stdout);
	while (it != std::end (s_messages))
	{
		std::fputs (s_colors[it->level], stdout);
		std::fputs (it->message.c_str (), stdout);
		++it;
	}
	std::fflush (stdout);
	s_messages.clear ();
}

void debug (char const *const fmt_, ...)
{
#ifndef NDEBUG
	va_list ap;
	va_start (ap, fmt_);
	addLog (DEBUG, fmt_, ap);
	va_end (ap);
#else
	(void)fmt_;
#endif
}

void info (char const *const fmt_, ...)
{
	va_list ap;
	va_start (ap, fmt_);
	addLog (INFO, fmt_, ap);
	va_end (ap);
}

void error (char const *const fmt_, ...)
{
	va_list ap;
	va_start (ap, fmt_);
	addLog (ERROR, fmt_, ap);
	va_end (ap);
}

void command (char const *const fmt_, ...)
{
	va_list ap;
	va_start (ap, fmt_);
	addLog (COMMAND, fmt_, ap);
	va_end (ap);
}

void response (char const *const fmt_, ...)
{
	va_list ap;
	va_start (ap, fmt_);
	addLog (RESPONSE, fmt_, ap);
	va_end (ap);
}

void addLog (LogLevel const level_, char const *const fmt_, va_list ap_)
{
#ifdef NDEBUG
	if (level_ == DEBUG)
		return;
#endif

	thread_local static char buffer[1024];
	std::vsnprintf (buffer, sizeof (buffer), fmt_, ap_);

	auto const lock = std::scoped_lock (s_lock);
	s_messages.emplace_back (level_, buffer);
	s_logUpdated = true;
}

void addLog (LogLevel const level_, std::string_view const message_)
{
#ifdef NDEBUG
	if (level_ == DEBUG)
		return;
#endif

	auto msg = std::string (message_);
	for (auto &c : msg)
	{
		if (c == '\0')
			c = '?';
	}

	auto const lock = std::scoped_lock (s_lock);
	s_messages.emplace_back (level_, msg);
	s_logUpdated = true;
}
