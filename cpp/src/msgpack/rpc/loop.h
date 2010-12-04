//
// msgpack::rpc::loop - MessagePack-RPC for C++
//
// Copyright (C) 2009-2010 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#ifndef MSGPACK_RPC_LOOP_H__
#define MSGPACK_RPC_LOOP_H__

#ifndef _WIN32
#include <mp/wavy.h>
#else
#include "iocploop.h"
#endif
#include <mp/memory.h>


namespace msgpack {
namespace rpc {

#ifdef _WIN32
class loop : public mp::shared_ptr<impl::windows::loop> {
public:
	loop() : mp::shared_ptr<impl::windows::loop>(new impl::windows::loop()) { }
	~loop() { }
};
#else
class loop : public mp::shared_ptr<mp::wavy::loop> {
public:
	loop() : mp::shared_ptr<mp::wavy::loop>(new mp::wavy::loop()) { }
	~loop() { }
};
#endif


}  // namespace rpc
}  // namespace msgpack

#endif /* msgpack/rpc/loop.h */

