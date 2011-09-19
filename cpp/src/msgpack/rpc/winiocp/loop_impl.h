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
#ifndef MSGPACK_RPC_WINIOCP_LOOP_IMPL_H__
#define MSGPACK_RPC_WINIOCP_LOOP_IMPL_H__

#include "mp/functional.h"

namespace msgpack {
namespace rpc {
namespace winiocp {

class loop_impl {
public:
	loop_impl() {}
	~loop_impl() {}

	bool is_running() const;

	void run_once();

	intptr_t add_timer(double value_sec, double interval_sec, mp::function<bool ()> callback);

	template <typename F>
	void submit(F f);
	template <typename F, typename A1>
	void submit(F f, A1 a1);
	template <typename F, typename A1, typename A2>
	void submit(F f, A1 a1, A2 a2);
	template <typename F, typename A1, typename A2, typename A3>
	void submit(F f, A1 a1, A2 a2, A3 a3);
	template <typename F, typename A1, typename A2, typename A3, typename A4>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15);
	template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
	void submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15, A16 a16);

private:
	typedef mp::function<void ()> task_t;
	void submit_impl(task_t f);

	loop_impl(const loop_impl&); // = delete;
	loop_impl& operator =(const loop_impl&); // = delete;
};

template <typename F>
inline void loop_impl::submit(F f)
	{ submit_impl(task_t(f)); }
template <typename F, typename A1>
inline void loop_impl::submit(F f, A1 a1)
	{ submit_impl(mp::bind(f, a1)); }
template <typename F, typename A1, typename A2>
inline void loop_impl::submit(F f, A1 a1, A2 a2)
	{ submit_impl(mp::bind(f, a1, a2)); }
template <typename F, typename A1, typename A2, typename A3>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3)
	{ submit_impl(mp::bind(f, a1, a2, a3)); }
template <typename F, typename A1, typename A2, typename A3, typename A4>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
inline void loop_impl::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15, A16 a16)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16)); }

}  // namespace winiocp
}  // namespace rpc
}  // namespace msgpack

#endif /* msgpack/rpc/winiocp/loop_impl.h */

