#include "stdafx.h"
#include "echo_server.h"
#include <msgpack/rpc/server.h>
#include <msgpack/rpc/client.h>

void notify()
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

	// notify
	cli.notify("echo");
	cli.notify("echo", 0);
	cli.notify("echo", 0, 1);

	//cli.get_loop()->flush();

	Sleep(10);
}

