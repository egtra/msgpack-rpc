#include "loop_impl.h"
#include "msgpack/rpc/transport/iocp/base.h"
#include <mp/pthread.h>
#include <mp/sync.h>
#include <mp/cstdint.h>
#include <cassert>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <iterator>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

namespace msgpack {
namespace rpc {
namespace winiocp {

namespace detail {

using msgpack::rpc::transport::detail::handle_deleter;

#define UNIQUE_PTR_WITH_DELETER(handle_type, delete_func) mp::unique_ptr<handle_type, handle_deleter<handle_type, delete_func> >

typedef UNIQUE_PTR_WITH_DELETER(HANDLE, ::CloseHandle) unique_handle;
typedef UNIQUE_PTR_WITH_DELETER(SOCKET, ::closesocket) unique_socket;
typedef UNIQUE_PTR_WITH_DELETER(WSAEVENT, ::WSACloseEvent) unique_wsaevent;
typedef UNIQUE_PTR_WITH_DELETER(HANDLE, ::UnregisterWait) unique_wait_handle;

const ULONG_PTR COMPLATE_KEY_END = 1;

class timer {
public:
	timer(mp::weak_ptr<iocp_loop> loop, mp::int64_t value_100nsec, int interval_msec, mp::function<bool ()> callback);

	HANDLE get_handle() const
	{
		return htimer.get();
	}

private:
	static void CALLBACK on_timer_entry(void* parameter, BOOLEAN);
	void on_timer();

	unique_handle htimer;
	unique_wait_handle hwait;
	mp::weak_ptr<iocp_loop> m_loop;
	mp::function<bool ()> m_callback;

	timer(const timer&);
	timer& operator =(const timer&);
};

class loop_impl
{
public:
	explicit loop_impl(int threads);

	unique_handle hiocp;
#ifdef _MSC_VER
	volatile bool end_flag;
#else
	std::atomic<bool> end_flag;
#endif

	typedef mp::sync<std::vector<unique_handle> > workers_t;
	workers_t worker;

	static DWORD WINAPI thread_entry(void* pthis);
	void thread_main();

	bool dispatch(bool block);

	mp::intptr_t add_timer(mp::weak_ptr<iocp_loop> loop, mp::int64_t value_100nsec, int interval_msec, mp::function<bool ()> callback);
	void remove_timer(mp::intptr_t timer);

	typedef std::vector<mp::shared_ptr<timer> > vec_timer_t;
	typedef mp::sync<vec_timer_t> sync_timer_t;
	sync_timer_t m_timer;

private:
	loop_impl(const loop_impl&); // = delete;
	loop_impl& operator=(const loop_impl&); // = delete;
};

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

loop_impl::loop_impl(int threads) :
	hiocp(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, threads)),
	end_flag()
{
}

DWORD WINAPI loop_impl::thread_entry(void* pthis)
{
	static_cast<loop_impl*>(pthis)->thread_main();
	return 0;
}

void loop_impl::thread_main()
{
	while(dispatch(true)) {
		;
	}
}

bool loop_impl::dispatch(bool block)
{
	DWORD transferred;
	ULONG_PTR key;
	OVERLAPPED* poverlapped;
	BOOL ret = ::GetQueuedCompletionStatus(hiocp.get(), &transferred, &key, &poverlapped, block ? INFINITE : 0);
	DWORD lastError = ::GetLastError();

	if(ret && key == COMPLATE_KEY_END) {
		::PostQueuedCompletionStatus(hiocp.get(), 0, COMPLATE_KEY_END, 0);
		return false;
	}

	if(poverlapped != NULL) {
		std::auto_ptr<overlapped_callback> oc(static_cast<overlapped_callback*>(poverlapped));
		oc->on_completed(transferred,
			ret
			? 0
			: lastError);
	} else if(!ret) {
		if (lastError == WAIT_TIMEOUT) {
			return false;
		} else {
			throw mp::system_error(lastError, "GetQueuedCompletionStatus failed");
		}
	}
	return true;
}

timer::timer(mp::weak_ptr<iocp_loop> loop, int64_t value_100nsec, int interval_msec, mp::function<bool ()> callback)
	: htimer(), m_loop(loop), m_callback(callback)
{
	htimer.reset(::CreateWaitableTimer(NULL, FALSE, NULL));
	if(!htimer) {
		throw mp::system_error(::GetLastError(), "CreateWaitableTimer failed");
	}

	assert(value_100nsec != 0);  // FIX ME: shoud call CancelWaitableTimer?

	LARGE_INTEGER value;
	value.QuadPart = value_100nsec;
	if(!::SetWaitableTimer(htimer.get(), &value, interval_msec, NULL, NULL, FALSE)) {
		throw mp::system_error(::GetLastError(), "SetWaitableTimer failed");
	}

	HANDLE hw;
	if(!::RegisterWaitForSingleObject(&hw, htimer.get(), on_timer_entry, this, INFINITE, WT_EXECUTEDEFAULT)) {
		throw mp::system_error(::GetLastError(), "RegisterWaitForSingleObject failed");
	}
	hwait.reset(hw);
}

void CALLBACK timer::on_timer_entry(void* parameter, BOOLEAN)
{
	timer* pthis = static_cast<timer*>(parameter);
	mp::shared_ptr<iocp_loop> loop = pthis->m_loop.lock();
	if(loop) {
		loop->submit(&timer::on_timer, pthis);
	}
}

void timer::on_timer()
{
	if(!m_callback()) {
		::CancelWaitableTimer(htimer.get());
	}
}

mp::intptr_t loop_impl::add_timer(mp::weak_ptr<iocp_loop> loop, mp::int64_t value_100nsec, int interval_msec, mp::function<bool ()> callback)
{
	sync_timer_t::ref ref(m_timer);
	ref->push_back(mp::make_shared<timer>(loop, value_100nsec, interval_msec, callback));
	return reinterpret_cast<mp::intptr_t>(ref->back()->get_handle());
}

struct timer_finder
{
	typedef bool result_type;
	bool operator()(const mp::shared_ptr<timer>& timer, mp::intptr_t htimer) const
	{
		return timer->get_handle() == reinterpret_cast<HANDLE>(htimer);
	}
};

void loop_impl::remove_timer(mp::intptr_t timer)
{
	using namespace mp::placeholders;

	sync_timer_t::ref ref(m_timer);
	vec_timer_t::const_iterator it = std::find_if(ref->begin(), ref->end(), mp::bind(timer_finder(), _1, timer));
	if(it == ref->end()) {
		throw std::invalid_argument("remove_timer");
	}
	ref->erase(it);
}

}  // namespace detail

typedef detail::loop_impl::workers_t::ref worker_ref;

iocp_loop::iocp_loop(int threads)
	: m_impl(new detail::loop_impl(threads))
{
}

iocp_loop::~iocp_loop()
{
	end();
	join();  // FIXME detached?
}

void iocp_loop::start(size_t num)
{
	if(is_running()) {
		// FIXME exception
		throw std::runtime_error("loop is already running");
	}
	add_thread(num);
}

void iocp_loop::run(size_t num)
{
	start(num);
	join();
}

bool iocp_loop::is_running() const
{
	return !worker_ref(m_impl->worker)->empty();
}

void iocp_loop::run_once()
{
	m_impl->dispatch(true);
}

void iocp_loop::run_nonblock()
{
	m_impl->dispatch(false);
}

void iocp_loop::flush()
{
	while (m_impl->dispatch(false)){
		// Nothing to do here
	}
}

void iocp_loop::end()
{
	m_impl->end_flag = true;
	::PostQueuedCompletionStatus(m_impl->hiocp.get(), 0, detail::COMPLATE_KEY_END, 0);
}

bool iocp_loop::is_end() const
{
	return m_impl->end_flag;
}

void iocp_loop::join()
{
	worker_ref ref(m_impl->worker);
	if (ref->empty()) {
		return;
	}
	std::vector<HANDLE> h;
	h.reserve(ref->size());
	std::transform(ref->begin(), ref->end(), std::back_inserter(h), mp::mem_fn(&detail::unique_handle::get));
	size_t i = 0;
	for(; i < h.size(); i += MAXIMUM_WAIT_OBJECTS) {
		::WaitForMultipleObjects(std::min<DWORD>(MAXIMUM_WAIT_OBJECTS, h.size() - i), &h[i], TRUE, INFINITE);
	}
}

//void iocp_loop::detach()

void iocp_loop::add_thread(size_t num)
{
	if(m_impl->end_flag) {
		return;
	}
	worker_ref ref(m_impl->worker);
	size_t prev_size = ref->size();
	ref->reserve(prev_size + num);
	for(size_t i = 0; i < num; ++i) {
		ref->push_back(detail::unique_handle());
		ref->back().reset(::CreateThread(NULL, 0, detail::loop_impl::thread_entry, m_impl.get(), 0, NULL));
		if(ref->back().get() == NULL) {
			ref->pop_back();
			throw mp::system_error(::GetLastError(), "CreateThread failed");
		}
	}
}

SOCKET iocp_loop::craete_socket(int af, int type, int protocol)
{
	detail::unique_socket s(::WSASocket(af, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED));
	if(s.get() == INVALID_SOCKET) {
		throw mp::system_error(::WSAGetLastError(), "WSASocket failed");
	}
	if(::CreateIoCompletionPort(reinterpret_cast<HANDLE>(s.get()), m_impl->hiocp.get(), 0, 0) == NULL) {
		throw mp::system_error(::GetLastError(), "CreateIoCompletionPort failed");
	}
	return s.release();
}

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

void iocp_loop::send(SOCKET socket, const WSABUF* buffers, size_t count, send_receive_callback_t callback)
{
	typedef send_receive_overlapped<send_receive_callback_t> receive_overlapped;
	std::auto_ptr<receive_overlapped> ov(new(std::nothrow) receive_overlapped(std::move(callback)));
	if(ov.get() == NULL && callback) {
		callback(0, ERROR_OUTOFMEMORY);
	}
	try {
		LOG_TRACE("send: ", ov.get());
		int ret = ::WSASend(socket, const_cast<WSABUF*>(buffers), count, NULL, 0, ov.get(), NULL);
		if(ret != 0) {
			DWORD error = ::WSAGetLastError();
			if(error != WSA_IO_PENDING) {
				ov->on_completed(0, error);
				return;
			}
		}
		ov.release();
	} catch(const std::bad_alloc&) {
		ov->on_completed(0, ERROR_OUTOFMEMORY);
	} catch(...) {
		ov->on_completed(0, E_UNEXPECTED);
	}
}

void iocp_loop::receive(SOCKET socket, const WSABUF* buffers, size_t count, send_receive_callback_t callback)
{
	typedef send_receive_overlapped<send_receive_callback_t> receive_overlapped;
	try {
		std::auto_ptr<receive_overlapped> ov(new receive_overlapped(std::move(callback)));
		LOG_TRACE("receive: ", ov.get());
		DWORD flags = 0;
		int ret = ::WSARecv(socket, const_cast<WSABUF*>(buffers), count, NULL, &flags, ov.get(), NULL);
		if(ret != 0) {
			DWORD error = ::WSAGetLastError();
			if(error != WSA_IO_PENDING) {
				ov->on_completed(0, error);
				return;
			}
		}
		ov.release();
	} catch(const std::bad_alloc&) {
		callback(0, ERROR_OUTOFMEMORY);
	} catch(...) {
		callback(0, E_UNEXPECTED);
	}
}

struct connect_info
{
	iocp_loop::connect_callback_t callback;
	detail::unique_socket socket;
	detail::unique_wsaevent connect_event;
	DWORD timeout_ms;
};

void connect_wait(mp::shared_ptr<connect_info> info)
{
	WSAEVENT e = info->connect_event.get();
	DWORD ret = ::WSAWaitForMultipleEvents(1, &e, FALSE, info->timeout_ms, FALSE);
	switch(ret) {
	case WSA_WAIT_EVENT_0: {
			WSANETWORKEVENTS ne;
			if(WSAEnumNetworkEvents(info->socket.get(), e, &ne) == 0) {
				if(ne.iErrorCode[FD_CONNECT_BIT] == 0) {
					info->callback(info->socket.release(), 0);
				} else {
					info->callback(INVALID_SOCKET, ne.iErrorCode[FD_CONNECT_BIT]);
				}
			} else {
				info->callback(INVALID_SOCKET, ::WSAGetLastError());
			}
			return;
		}
	case WSA_WAIT_TIMEOUT:
		info->callback(INVALID_SOCKET, WSAETIMEDOUT);
		return;
	case WSA_WAIT_FAILED:
		{
			DWORD error = ::WSAGetLastError();
			info->callback(INVALID_SOCKET, error);
			return;
		}
	default:
		info->callback(INVALID_SOCKET, E_UNEXPECTED);
		return;
	}
}

void iocp_loop::connect(const sockaddr* addr, socklen_t addrlen, double timeout_sec, connect_callback_t callback)
{
	mp::shared_ptr<connect_info> info;
	try {
		info = mp::make_shared<connect_info>();
	} catch(const std::bad_alloc&) {
		callback(INVALID_SOCKET, ERROR_OUTOFMEMORY);
		return;
	}

	detail::unique_socket s;
	try {
		s.reset(craete_socket(addr->sa_family, SOCK_STREAM, 0));
	} catch (const mp::system_error& e) {
		callback(INVALID_SOCKET, e.code);
	}
	if(s.get() == INVALID_SOCKET) {
		callback(INVALID_SOCKET, ::WSAGetLastError());
		return;
	}

	info->connect_event.reset(::WSACreateEvent());
	if(info->connect_event.get() == WSA_INVALID_EVENT) {
		callback(INVALID_SOCKET, ::WSAGetLastError());
		return;
	}
	if(WSAEventSelect(s.get(), info->connect_event.get(), FD_CONNECT) != 0) {
		callback(INVALID_SOCKET, ::WSAGetLastError());
		return;
	}
	if(::connect(s.get(), addr, addrlen) == 0) {
		callback(s.release(), 0);
		return;
	} else {
		DWORD err = ::WSAGetLastError();
		if(err != WSAEWOULDBLOCK) {
			callback(INVALID_SOCKET, err);
			return;
		}
	}
	info->callback.swap(callback); // info->callback = std::move(callback);
	info->socket.swap(s); // info->socket = std::move(s);
	info->timeout_ms = static_cast<DWORD>(timeout_sec * 1000);
	this->submit(connect_wait, info);
}

void listen_loop(SOCKET lsock, iocp_loop::listen_callback_t callback, HANDLE hiocp)
{
	detail::unique_wsaevent e(::WSACreateEvent());
	if(e.get() == WSA_INVALID_EVENT) {
		callback(INVALID_SOCKET, ::WSAGetLastError());
		return;
	}
	if(::WSAEventSelect(lsock, e.get(), FD_ACCEPT | FD_CLOSE) != 0) {
		callback(INVALID_SOCKET, ::WSAGetLastError());
		return;
	}

	WSAEVENT e2 = e.get();
	for (;;) {
		DWORD wait = ::WSAWaitForMultipleEvents(1, &e2, FALSE, WSA_INFINITE, FALSE);
		if(wait == WSA_WAIT_EVENT_0) {
			WSANETWORKEVENTS ne;
			if(::WSAEnumNetworkEvents(lsock, e.get(), &ne) != 0) {
				DWORD error = ::WSAGetLastError();
				break;
			}
			if((ne.lNetworkEvents & FD_ACCEPT) == FD_ACCEPT) {
				if(ne.iErrorCode[FD_ACCEPT_BIT] != 0) {
					callback(INVALID_SOCKET, ne.iErrorCode[FD_ACCEPT_BIT]);
					break;
				}
				detail::unique_socket as(::accept(lsock, NULL, NULL));
				if(as.get() == INVALID_SOCKET) {
					callback(INVALID_SOCKET, ::WSAGetLastError());
					break;
				}
				if(::CreateIoCompletionPort(reinterpret_cast<HANDLE>(as.get()), hiocp, 0, 0) == NULL) {
					callback(INVALID_SOCKET, ::GetLastError());
					break;
				}
				callback(as.release(), 0);
			}
		} else if (wait == WSA_WAIT_FAILED) {
			callback(INVALID_SOCKET, ::WSAGetLastError());
			break;
		} else {
			callback(INVALID_SOCKET, E_UNEXPECTED);
			break;
		}
	}
}

SOCKET iocp_loop::listen(int socket_family, int socket_type, int protocol,
	const sockaddr* addr, socklen_t addrlen, listen_callback_t callback, int backlog)
{
	detail::unique_socket lsock(craete_socket(addr->sa_family, SOCK_STREAM, 0));

	BOOL on = TRUE;
	if(::setsockopt(lsock.get(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on)) != 0) {
		throw mp::system_error(::WSAGetLastError(), "setsockopt failed");
	}

	if(::bind(lsock.get(), addr, addrlen) != 0) {
		throw mp::system_error(::WSAGetLastError(), "bind failed");
	}

	if(::listen(lsock.get(), backlog) != 0) {
		throw mp::system_error(::WSAGetLastError(), "listen failed");
	}

	SOCKET ls = lsock.get();
	HANDLE hiocp = m_impl->hiocp.get();
	submit(listen_loop, ls, callback, hiocp);

	return lsock.release();
}

class task_overlapped : public detail::overlapped_callback
{
public:
	mp::function<void ()> m_callback;

	task_overlapped(mp::function<void ()> callback) : m_callback(std::move(callback)) {}

	void on_completed(DWORD /*transferred*/, DWORD /*error*/) override
	{
		m_callback();
	}
};

void iocp_loop::submit_impl(task_t f)
{
	std::auto_ptr<task_overlapped> ov(new task_overlapped(std::move(f)));
	LOG_TRACE("submit_impl: ", ov.get());
	if(::PostQueuedCompletionStatus(m_impl->hiocp.get(), 0, 0, ov.get())) {
		ov.release();
	} else {
		throw mp::system_error(::GetLastError(), "PostQueuedCompletionStatus failed");
	}
}

mp::intptr_t iocp_loop::add_timer(double value_sec, double interval_sec, mp::function<bool ()> callback)
{
	if(value_sec >= 0.0) {
		if(interval_sec > 0.0) {
			return m_impl->add_timer(shared_from_this(), static_cast<mp::int64_t>(value_sec * -10000000), static_cast<long>(interval_sec * 1000), callback);
		} else {
			return m_impl->add_timer(shared_from_this(), static_cast<mp::int64_t>(value_sec * -10000000), 0, callback);
		}
	} else {
		if(interval_sec > 0.0) {
			return m_impl->add_timer(shared_from_this(), 0, static_cast<long>(interval_sec * 1000), callback);
		} else {
			return m_impl->add_timer(shared_from_this(), 0, 0, callback);
		}
	}
}

void iocp_loop::remove_timer(mp::intptr_t ident)
{
	m_impl->remove_timer(ident);
}


}  // namespace winiocp
}  // namespace rpc
}  // namespace msgpack