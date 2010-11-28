#include "stdafx.h"

#include <msgpack/rpc/client.h>
#include "test.h"

void async_call_0()
{
	msgpack::rpc::client c(SERVER_NAME, SERVER_PORT);

	msgpack::rpc::future f1 = c.call("add", 1, 3);
	msgpack::rpc::future f2 = c.call("add", 2, 3);

	int result1 = f1.get<int>();
	int result2 = f2.get<int>();

	std::cout << result1 << std::endl;
	std::cout << result2 << std::endl;
}
