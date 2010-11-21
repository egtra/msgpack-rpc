#ifndef CCLOG_DEBUGOUTPUT_H__
#define CCLOG_DEBUGOUTPUT_H__

#include "cclog.h"

class cclog_debugoutput : public cclog {
public:
	cclog_debugoutput(level runtime_level);
	~cclog_debugoutput();

	void log_impl(level lv, std::string& str);
};

#endif /* cclog_debugoutput.h */
