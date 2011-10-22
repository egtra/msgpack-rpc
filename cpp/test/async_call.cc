#ifdef _WIN32
#	include "wsainitializer.h"
#endif

#include "echo_server.h"
#include <msgpack/rpc/server.h>
#include <msgpack/rpc/session_pool.h>
#include <cclog/cclog.h>
#ifdef _WIN32
#	include <cclog/cclog_console.h>
#else
#	include <cclog/cclog_tty.h>
#endif

int main(void)
{
#ifdef _WIN32
	cclog::reset(new cclog_console(cclog::TRACE, ::GetStdHandle(STD_OUTPUT_HANDLE)));
#else
	cclog::reset(new cclog_tty(cclog::TRACE, std::cout));
	signal(SIGPIPE, SIG_IGN);
#endif

	// run server {
	rpc::server svr;

	std::auto_ptr<rpc::dispatcher> dp(new myecho);
	svr.serve(dp.get());

	svr.listen("0.0.0.0", 18811);

	svr.start(4);
	// }


	// create session pool
	rpc::session_pool sp;

	// get session
	rpc::session s = sp.get_session("127.0.0.1", 18811);

	// async call
	rpc::future fs[10];

	for(int i=0; i < 10; ++i) {
		fs[i] = s.call("add", 1, 2);
	}

	for(int i=0; i < 10; ++i) {
		int ret = fs[i].get<int>();
		std::cout << "async call: add(1, 2) = " << ret << std::endl;
	}

	return 0;
}

