#include "cclog_debugoutput.h"
#include <windows.h>

cclog_debugoutput::cclog_debugoutput(level runtime_level) :
	cclog(runtime_level)
{ }

cclog_debugoutput::~cclog_debugoutput()
{ }

void cclog_debugoutput::log_impl(level lv, std::string& str)
{
	std::string output = str + "\r\n";
	OutputDebugStringA(output.c_str());
}

