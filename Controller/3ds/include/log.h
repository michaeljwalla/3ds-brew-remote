// Copied from ftpd/include/log.h
#pragma once

#include <cstdarg>
#include <string>
#include <string_view>

#ifdef DEBUG
#undef DEBUG
#endif

enum LogLevel
{
	DEBUG,
	INFO,
	ERROR,
	COMMAND,
	RESPONSE,
};

void drawLog ();

__attribute__ ((format (printf, 1, 2))) void debug (char const *fmt_, ...);
__attribute__ ((format (printf, 1, 2))) void info (char const *fmt_, ...);
__attribute__ ((format (printf, 1, 2))) void error (char const *fmt_, ...);
__attribute__ ((format (printf, 1, 2))) void command (char const *fmt_, ...);
__attribute__ ((format (printf, 1, 2))) void response (char const *fmt_, ...);

void addLog (LogLevel level_, char const *fmt_, va_list ap_);
void addLog (LogLevel level_, std::string_view message_);
