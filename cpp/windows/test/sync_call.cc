#include "stdafx.h"
#include "echo_server.h"
#include <msgpack/rpc/server.h>
#include <msgpack/rpc/client.h>

void sync_call()
{
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
}

