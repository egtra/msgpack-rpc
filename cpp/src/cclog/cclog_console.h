#ifndef CCLOG_CONSOLE_H__
#define CCLOG_CONSOLE_H__

#include "cclog.h"
#include <mp/pthread.h>
#include <windows.h>

class cclog_console : public cclog {
public:
	cclog_console(level runtime_level, HANDLE hconsole);
	~cclog_console();

	void log_impl(level lv, std::string& str);

private:
	mp::pthread_mutex m_mutex;
	HANDLE m_hconsole;
	WORD m_normal_color;
};

#endif /* cclog_console.h */
