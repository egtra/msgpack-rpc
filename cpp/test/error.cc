#ifdef _WIN32
#	include "wsainitializer.h"
#endif

#include "echo_server.h"
#include <msgpack/rpc/server.h>
#include <msgpack/rpc/client.h>
#include <cclog/cclog.h>
#include <cclog/cclog_tty.h>
#ifdef _WIN32
#	include <cclog/cclog_console.h>
#	include <cclog/cclog_debugoutput.h>
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


	try {
		rpc::client cli("127.0.0.1", 16396);
		cli.call("add", 1, 2).get<int>();
	} catch(msgpack::rpc::connect_error& e) {
		std::cout << "ok: "<< e.what() << std::endl;
	}

	rpc::client cli("127.0.0.1", 18811);

	try {
		cli.call("sub", 2, 1).get<int>();
	} catch(msgpack::rpc::no_method_error& e) {
		std::cout << "ok: " << e.what() << std::endl;
	}

	try {
		cli.call("add", 1).get<int>();
	} catch(msgpack::rpc::argument_error& e) {
		std::cout << "ok: " << e.what() << std::endl;
	}

	try {
		cli.call("err").get<int>();
	} catch(msgpack::rpc::remote_error& e) {
		std::cout << "ok: " << e.what() << std::endl;
		std::cout << "error object: " << e.error() << std::endl;
		std::cout << "result object: " << e.result() << std::endl;
	}
}

