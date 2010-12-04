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

	try {
		//async_call_0();
		//async_server();
		//sync_call();
		callback();
	} catch (std::exception const& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}

