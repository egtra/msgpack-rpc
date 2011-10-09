#include "cclog_console.h"
#include <windows.h>

#define CONSOLE_COLOR_NORMAL   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CONSOLE_COLOR_RED      (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_GREEN    (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_YELLOW   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_BLUE     (FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_MAGENTA  (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_CYAN     (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define CONSOLE_COLOR_WHITE    (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)

static WORD const color_table[] = {
	CONSOLE_COLOR_NORMAL,
	CONSOLE_COLOR_WHITE,
	CONSOLE_COLOR_GREEN,
	CONSOLE_COLOR_YELLOW,
	CONSOLE_COLOR_MAGENTA,
	CONSOLE_COLOR_RED,
};

namespace {

WORD get_current_attributes(HANDLE hconsole)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi = {};
	if(GetConsoleScreenBufferInfo(hconsole, &csbi)) {
		return csbi.wAttributes;
	} else {
		return CONSOLE_COLOR_NORMAL;
	}
}

}  // noname namespace


cclog_console::cclog_console(level runtime_level, HANDLE hconsole) :
	cclog(runtime_level),
	m_mutex(),
	m_hconsole(hconsole),
	m_normal_color(get_current_attributes(hconsole))
{ }

cclog_console::~cclog_console()
{ }

void cclog_console::log_impl(level lv, std::string& str)
{
	const std::string output = str + "\r\n";

	mp::pthread_scoped_lock lock(m_mutex);

	DWORD written;
	SetConsoleTextAttribute(m_hconsole, color_table[lv]);
	WriteConsoleA(m_hconsole, output.c_str(), output.size(), &written, NULL);
	SetConsoleTextAttribute(m_hconsole, m_normal_color);
}

