#ifdef _WIN32
#	include "wsainitializer.h"
#endif

#include "echo_server.h"
#include <msgpack/rpc/server.h>
#include <msgpack/rpc/client.h>
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


	// create client
	rpc::client cli("127.0.0.1", 18811);

	// call
	std::string msg("MessagePack-RPC");
	std::string ret = cli.call("echo", msg).get<std::string>();

	std::cout << "call: echo(\"MessagePack-RPC\") = " << ret << std::endl;

	return 0;
}

