#include "stdafx.h"
#include "echo_server.h"
#include <msgpack/rpc/server.h>
#include <msgpack/rpc/client.h>

void zone()
{
	// run server {
	rpc::server svr;

	std::auto_ptr<rpc::dispatcher> dp(new myecho);
	svr.serve(dp.get());

	svr.listen("0.0.0.0", 18811);

	svr.start(4);
	// }

	mp::shared_ptr<msgpack::zone> zone = std::make_shared<msgpack::zone>();

	// create client
	rpc::client cli("127.0.0.1", 18811);

	// call
	std::string msg("MessagePack-RPC");
	std::string ret = cli.call("echo", zone, msg).get<std::string>();
	std::string ret2 = cli.call("echo", zone, msg).get<std::string>();

	std::cout << "call with zone: echo(\"MessagePack-RPC\") = " << ret << std::endl;
	std::cout << "call with zone: echo(\"MessagePack-RPC\") = " << ret2 << std::endl;
}

