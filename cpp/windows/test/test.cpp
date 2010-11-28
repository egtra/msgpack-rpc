#include "stdafx.h"

#include <boost/scope_exit.hpp>
#include <cclog/cclog_debugoutput.h>
#include "test.h"

int _tmain(int argc, _TCHAR* argv[])
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	BOOST_SCOPE_EXIT((&wsaData))
	{
		WSACleanup();
	}
	BOOST_SCOPE_EXIT_END

	cclog::reset(new cclog_debugoutput(cclog::TRACE));

	//async_call_0();
	async_server();

	return 0;
}
