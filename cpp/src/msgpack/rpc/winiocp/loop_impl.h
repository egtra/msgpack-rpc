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
#ifndef MSGPACK_RPC_WINIOCP_IOCP_LOOP_H__
#define MSGPACK_RPC_WINIOCP_IOCP_LOOP_H__

#include "mp/functional.h"
#include "mp/memory.h"
#include "mp/cstdint.h"

namespace msgpack {
namespace rpc {
namespace winiocp {

namespace detail
{

class loop_impl;

class overlapped_callback : public ::OVERLAPPED
{
public:
	virtual void on_completed(DWORD transferred, DWORD error) = 0;
	virtual ~overlapped_callback() {}

protected:
	overlapped_callback() : ::OVERLAPPED() {}

private:
	overlapped_callback(const overlapped_callback&); // = delete;
	overlapped_callback& operator=(const overlapped_callback&); // = delete;
};

template<typename T>
class send_receive_overlapped : public detail::overlapped_callback
{
	T m_callback;

public:
	send_receive_overlapped(T callback) : m_callback(std::move(callback)) {}

	void on_completed(DWORD transferred, DWORD error) override
	{
		m_callback(transferred, error);
	}
};

}  // namespace detail

class iocp_loop : public mp::enable_shared_from_this<iocp_loop> {
public:
	explicit iocp_loop(int threads = 0);
	~iocp_loop();

	void start(size_t num);

	void run(size_t num);

	bool is_running() const;

	void run_once();
	void run_nonblock();
	void flush();

	void end();
	bool is_end() const;

	void join();
	void detach();

	void add_thread(size_t num);

	typedef mp::function<void (SOCKET s, DWORD err)> connect_callback_t;

	void connect(const sockaddr* addr, socklen_t addrlen, double timeout_sec, connect_callback_t callback);

	typedef mp::function<void (SOCKET s, DWORD err)> listen_callback_t;

	SOCKET listen(int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen, listen_callback_t callback, int backlog = 1024);

private:
	void send_impl(SOCKET socket, const WSABUF* buffers, size_t count, std::auto_ptr<detail::overlapped_callback> callback);
	void receive_impl(SOCKET socket, const WSABUF* buffers, size_t count, std::auto_ptr<detail::overlapped_callback> callback);

public:
	template<typename F>
	void send(SOCKET socket, const WSABUF* buffers, size_t count, F callback)
	{
		std::auto_ptr<detail::overlapped_callback> overlapped(
			new(std::nothrow) detail::send_receive_overlapped<F>(callback));
		if(overlapped.get() == NULL) {
			callback(0, ERROR_NOT_ENOUGH_MEMORY);
		} else {
			send_impl(socket, buffers, count, overlapped);
		}
	}

	template<typename F>
	void receive(SOCKET socket, const WSABUF* buffers, size_t count, F callback)
	{
		std::auto_ptr<detail::overlapped_callback> overlapped(
			new(std::nothrow) detail::send_receive_overlapped<F>(callback));
		if(overlapped.get() == NULL) {
			callback(0, ERROR_NOT_ENOUGH_MEMORY);
		} else {
			receive_impl(socket, buffers, count, overlapped);
		}
	}

	mp::intptr_t add_timer(double value_sec, double interval_sec, mp::function<bool ()> callback);

	void remove_timer(mp::intptr_t ident);

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

	mp::unique_ptr<detail::loop_impl, mp::default_delete<detail::loop_impl> > m_impl;

	iocp_loop(const iocp_loop&); // = delete;
	iocp_loop& operator =(const iocp_loop&); // = delete;
};

template <typename F>
inline void iocp_loop::submit(F f)
	{ submit_impl(task_t(f)); }
template <typename F, typename A1>
inline void iocp_loop::submit(F f, A1 a1)
	{ submit_impl(mp::bind(f, a1)); }
template <typename F, typename A1, typename A2>
inline void iocp_loop::submit(F f, A1 a1, A2 a2)
	{ submit_impl(mp::bind(f, a1, a2)); }
template <typename F, typename A1, typename A2, typename A3>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3)
	{ submit_impl(mp::bind(f, a1, a2, a3)); }
template <typename F, typename A1, typename A2, typename A3, typename A4>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
inline void iocp_loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15, A16 a16)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16)); }

}  // namespace winiocp
}  // namespace rpc
}  // namespace msgpack

#endif /* msgpack/rpc/winiocp/iocp_loop.h */

