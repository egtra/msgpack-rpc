#include <queue>
#include "cclog/cclog.h"
#include "mp/unordered.h"
#include "mp/pthread.h"
#include "iocploop.h"

using namespace mp::placeholders;

using mp::pthread_mutex;
using mp::pthread_scoped_lock;
using mp::pthread_cond;
using mp::pthread_thread;

namespace msgpack {
namespace rpc {
namespace impl {
namespace windows {

using mp::timespec;

namespace {

	typedef std::pair<mp::function<void (bool)>, HANDLE> wait_callback_object;

	void CALLBACK wait_callback(void* parameter, BOOLEAN timer_or_wait_fired)
	{
		std::auto_ptr<wait_callback_object> p(static_cast<wait_callback_object*>(parameter));
		::PostQueuedCompletionStatus(p->second, 0, 0, new overlapped_callback(mp::bind(p->first, timer_or_wait_fired != FALSE)));
	}

	unique_wait_handle register_wait_for_single_object(HANDLE hiocp, HANDLE hobject, mp::function<void (bool)> callback, ULONG milliseconds, ULONG flags)
	{
		HANDLE hwait;
		std::auto_ptr<wait_callback_object> p(new wait_callback_object(callback, hiocp));
		if(::RegisterWaitForSingleObject(&hwait, hobject, wait_callback, p.get(), milliseconds, flags)) {
			p.release();
			return unique_wait_handle(hwait);
		} else {
			throw mp::system_error(GetLastError(), "RegisterWaitForSingleobject failed");
		}
	}

	unique_socket make_socket(int af, int type, int protocol)
	{
		SOCKET s = ::socket(af, type, protocol);
		if(s == INVALID_SOCKET) {
			throw mp::system_error(WSAGetLastError(), "socket failed");
		}
		return unique_socket(s);
	}

}

// wavy_loop.h

class loop_impl {
public:
	unique_handle hiocp;
	loop_impl(mp::function<void ()> thread_init_func = mp::function<void ()>());
	~loop_impl();

	typedef mp::shared_ptr<basic_handler> shared_handler;
	typedef mp::function<void ()> task_t;

public:
	void start(size_t num);
	void start(size_t num, size_t max);

	bool is_running() const;

	void end();
	bool is_end() const;

	void run_once();
	void run_once(pthread_scoped_lock& lk);

	void join();
	void detach();

	void add_thread(size_t num);

	void submit_impl(task_t& f);

	void flush();

public:
	void thread_main();
	inline void do_task(pthread_scoped_lock& lk);
	inline void do_out(pthread_scoped_lock& lk);

private:
//	pthread_mutex m_mutex;
//	pthread_cond m_cond;

	mp::function<void ()> m_thread_init_func;

//	pthread_cond m_flush_cond;

private:
	friend class loop;

private:
	volatile bool m_end_flag;

	typedef std::vector<pthread_thread> workers_t;
	workers_t m_workers;

	static const ULONG_PTR COMPLATE_KEY_END = 1;

private:
	loop_impl(const loop_impl&);
};


#define ANON_impl static_cast<loop_impl*>(m_impl)

void loop::read(SOCKET fd, void* buf, size_t size, read_callback_t callback)
{
	using namespace mp::placeholders;

	WSABUF wb = { size, static_cast<char*>(buf) };
	DWORD flags = 0;
	std::auto_ptr<overlapped_callback> overlapped(
		new overlapped_callback(mp::bind(callback, _3, _2)));
	int rl = ::WSARecv(fd, &wb, 1, NULL, &flags, overlapped.get(), NULL);
	if(rl != 0) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) { throw mp::system_error(err, "read error"); }
	}

	overlapped.release();
}

// wavy_out.cc

void xfer2::push_write(const void* buf, size_t size)
{
	assert(size <= std::numeric_limits<DWORD>::max());
	WSABUF b = {static_cast<ULONG>(size), static_cast<char*>(const_cast<void*>(buf))};
	sync_ref ref(m_sync);
	ref->buffer.push_back(b);
}

void xfer2::push_writev(const struct iovec* vec, size_t veclen)
{
	sync_ref ref(m_sync);
	ref->buffer.resize(ref->buffer.size() + veclen);
	for(size_t i = 0; i < veclen; ++i) {
		WSABUF b = {static_cast<ULONG>(vec[i].iov_len), static_cast<char*>(const_cast<void*>(vec[i].iov_base))};
		ref->buffer.push_back(b);
	}
}

void xfer2::push_finalize(finalize_t fin, void* user)
{
	sync_ref ref(m_sync);
	ref->finalizer.push_back(mp::bind(fin, user));
}

namespace {
	void self(const mp::function<void ()>& f)
	{
		try {
			f();
		} catch (...) {
		}
	}
}

void xfer2::clear()
{
	sync_ref ref(m_sync);
	ref->clear();
}

void xfer2::sync_t::clear()
{
	std::for_each(finalizer.begin(), finalizer.end(), self);
	finalizer.clear();
	buffer.clear();
}

void xfer2::sync_t::swap(sync_t& y)
{
	buffer.swap(y.buffer);
	finalizer.swap(y.finalizer);
}


namespace {

void on_write_xfer2(DWORD transferred, DWORD error, mp::shared_ptr<xfer2::sync_t> xf)
{
	if(error != 0) {
		LOG_WARN("write error (on_write_xfer2): ", mp::system_error::errno_string(error));
	}
	xf->clear();
}

}  // noname namespace

void loop::commit(SOCKET fd, xfer2* xf)
{
	mp::shared_ptr<xfer2::sync_t> data = mp::make_shared<xfer2::sync_t>();
	{
		xfer2::sync_ref ref(xf->m_sync);
		ref->swap(*data);
	}
	if(!data->buffer.empty()) {
		std::auto_ptr<impl::windows::overlapped_callback> overlapped(new impl::windows::overlapped_callback(
			mp::bind(&on_write_xfer2, _2, _3, data)));
		int ret = ::WSASend(fd, &data->buffer[0], data->buffer.size(), 0, 0, overlapped.get(), NULL);
		if(ret != 0) {
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING) { throw mp::system_error(err, "write error"); }
		}

		overlapped.release();
	}
}

//void loop::write(int fd, const void* buf, size_t size)
//	{ ANON_out->write(fd, buf, size); }

namespace {

void on_write(DWORD transferred, DWORD error, void* user, loop::finalize_t fin)
{
	if(error != 0) {
		LOG_WARN("write error: ", mp::system_error::errno_string(error));
	}
	fin(user);
}

struct iovec_to_wsabuf : std::unary_function<const iovec&, WSABUF> {
	result_type operator() (argument_type v) const
	{
		const WSABUF b = {v.iov_len, static_cast<char*>(const_cast<void*>(v.iov_base))};
		return b;
	}
};

}  // noname namespace

void loop::write(SOCKET fd,
		const void* buf, size_t size,
		finalize_t fin, void* user)
{
	using namespace mp::placeholders;

	assert(size <= std::numeric_limits<ULONG>::max());
	WSABUF wb = {size, const_cast<char*>(static_cast<const char*>(buf))};
	std::auto_ptr<impl::windows::overlapped_callback> overlapped(new impl::windows::overlapped_callback(
		mp::bind(&on_write, _2, _3, user, fin)));
	BOOL ret = ::WSASend(fd, &wb, 1, 0, 0, overlapped.get(), NULL);
	if(ret != 0) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) { throw mp::system_error(err, "write error"); }
	}

	overlapped.release();
}

void loop::writev(SOCKET fd,
		const struct iovec* vec, size_t veclen,
		finalize_t fin, void* user)
{
	using namespace mp::placeholders;

	if(veclen > 0) {
		std::vector<WSABUF> wb(veclen);
		std::transform(vec, vec + veclen, wb.begin(), iovec_to_wsabuf());

		assert(veclen <= std::numeric_limits<ULONG>::max());
		std::auto_ptr<impl::windows::overlapped_callback> overlapped(new impl::windows::overlapped_callback(
			mp::bind(&on_write, _2, _3, user, fin)));
		BOOL ret = ::WSASend(fd, &wb[0], 1, 0, 0, overlapped.get(), NULL);
		if(ret != 0) {
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING) { throw mp::system_error(err, "write error"); }
		}

		overlapped.release();
	}
}

//void loop::sendfile(int fd,
//		int infd, uint64_t off, size_t size,
//		finalize_t fin, void* user)
//{
//	char xfbuf[ xfer_impl::sizeof_sendfile() + xfer_impl::sizeof_finalize() ];
//	char* p = xfbuf;
//	p = xfer_impl::fill_sendfile(p, infd, off, size);
//	p = xfer_impl::fill_finalize(p, fin, user);
//	ANON_out->commit_raw(fd, xfbuf, p);
//}
//
//void loop::hsendfile(int fd,
//		const void* header, size_t header_size,
//		int infd, uint64_t off, size_t size,
//		finalize_t fin, void* user)
//{
//	char xfbuf[ xfer_impl::sizeof_mem()
//		+ xfer_impl::sizeof_sendfile() + xfer_impl::sizeof_finalize() ];
//	char* p = xfbuf;
//	p = xfer_impl::fill_mem(p, header, header_size);
//	p = xfer_impl::fill_sendfile(p, infd, off, size);
//	p = xfer_impl::fill_finalize(p, fin, user);
//	ANON_out->commit_raw(fd, xfbuf, p);
//}
//
//void loop::hvsendfile(int fd,
//		const struct iovec* header_vec, size_t header_veclen,
//		int infd, uint64_t off, size_t size,
//		finalize_t fin, void* user)
//{
//	char xfbuf[ xfer_impl::sizeof_iovec(header_veclen)
//		+ xfer_impl::sizeof_sendfile() + xfer_impl::sizeof_finalize() ];
//	char* p = xfbuf;
//	p = xfer_impl::fill_iovec(p, header_vec, header_veclen);
//	p = xfer_impl::fill_sendfile(p, infd, off, size);
//	p = xfer_impl::fill_finalize(p, fin, user);
//	ANON_out->commit_raw(fd, xfbuf, p);
//}

// wavy_loop.cc

loop_impl::loop_impl(mp::function<void ()> thread_init_func) :/*
	m_off(0), m_num(0), m_pollable(true),
	m_thread_init_func(thread_init_func),*/
	m_end_flag(false),
	hiocp(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 8))
{
}

loop_impl::~loop_impl()
{
	end();
	join();  // FIXME detached?
	//{
	//	pthread_scoped_lock lk(m_mutex);
	//	m_cond.broadcast();
	//}
	//delete[] m_state;
}

void loop_impl::end()
{
	m_end_flag = true;
	{
//		pthread_scoped_lock lk(m_mutex);
		for (size_t i = 0; i < m_workers.size(); ++i) {
			::PostQueuedCompletionStatus(hiocp.get(), 0, COMPLATE_KEY_END, 0);
		}
	}
}

bool loop_impl::is_end() const
{
	return m_end_flag;
}


void loop_impl::join()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		it->join();
	}
	m_workers.clear();
}

void loop_impl::detach()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		it->detach();
	}
}


void loop_impl::start(size_t num)
{
//	pthread_scoped_lock lk(m_mutex);
	if(is_running()) {
		// FIXME exception
		throw std::runtime_error("loop is already running");
	}
	add_thread(num);
}

void loop_impl::add_thread(size_t num)
{
	for(size_t i=0; i < num; ++i) {
		m_workers.push_back( pthread_thread() );
		try {
			m_workers.back().run(
					mp::bind(&loop_impl::thread_main, this));
		} catch (...) {
			m_workers.pop_back();
			throw;
		}
	}
}

bool loop_impl::is_running() const
{
	return !m_workers.empty();
}

void loop_impl::submit_impl(task_t& f)
{
	//pthread_scoped_lock lk(m_mutex);
	::PostQueuedCompletionStatus(hiocp.get(), 0, 0, new overlapped_callback(mp::bind(f)));
}


void loop_impl::thread_main()
{
	while(true) {
		DWORD transferred;
		ULONG_PTR key;
		OVERLAPPED* poverlapped;
		BOOL ret = GetQueuedCompletionStatus(hiocp.get(), &transferred, &key, &poverlapped, INFINITE);
		DWORD lastError = GetLastError();

		if(ret && key == COMPLATE_KEY_END) {
			break;
		}

		if(poverlapped != NULL) {
			std::auto_ptr<overlapped_callback> oc(static_cast<overlapped_callback*>(poverlapped));
			if (oc->callback) {
				oc->callback(*poverlapped, transferred,
					ret
					? 0
					: lastError);
			}
		}
		else if(!ret) {
			if (lastError == WAIT_TIMEOUT) {
				break;
			} else {
				throw mp::system_error(lastError, "GetQueuedCompletionStatus");
			}
		}	}
}

//inline void loop_impl::run_once()
//{
//	pthread_scoped_lock lk(m_mutex);
//	run_once(lk);
//}
inline void loop_impl::run_once()
{
	while(true) {
		DWORD transferred;
		ULONG_PTR key;
		OVERLAPPED* poverlapped;
		BOOL ret = GetQueuedCompletionStatus(hiocp.get(), &transferred, &key, &poverlapped, 0);
		DWORD lastError = GetLastError();

		if(ret && key == COMPLATE_KEY_END) {
			PostQueuedCompletionStatus(hiocp.get(), 0, COMPLATE_KEY_END, 0);
			break;
		}

		if(poverlapped != NULL) {
			std::auto_ptr<overlapped_callback> oc(static_cast<overlapped_callback*>(poverlapped));
			if (oc->callback) {
				oc->callback(*poverlapped, transferred,
					ret
					? 0
					: lastError);
			}
		}
		else if(!ret) {
			if (lastError == WAIT_TIMEOUT) {
				break;
			} else {
				throw mp::system_error(lastError, "GetQueuedCompletionStatus");
			}
		}
	}
}

//void loop_impl::run_once(pthread_scoped_lock& lk)
//{
//	if(m_end_flag) { return; }
//
//	kernel::event ke;
//
//	if(!m_more_queue.empty()) {
//		ke = m_more_queue.front();
//		m_more_queue.pop();
//		goto process_handler;
//	}
//
//	if(!m_pollable) {
//		if(m_out->has_queue()) {
//			do_out(lk);
//		} else if(!m_task_queue.empty()) {
//			do_task(lk);
//		} else {
//			m_cond.wait(m_mutex);
//		}
//		return;
//	} else if(!m_task_queue.empty()) {
//		do_task(lk);
//		return;
//	} else if(m_out->has_queue()) {
//		do_out(lk);  // FIXME
//		return;
//	}
//
//	if(m_num == m_off) {
//		m_pollable = false;
//		lk.unlock();
//
//		int num = m_kernel.wait(&m_backlog, 1000);
//
//		if(num <= 0) {
//			if(num == 0 || errno == EINTR || errno == EAGAIN) {
//				m_pollable = true;
//				return;
//			} else {
//				throw system_error(errno, "wavy kernel event failed");
//			}
//		}
//
//		lk.relock(m_mutex);
//		m_off = 0;
//		m_num = num;
//
//		m_pollable = true;
//		m_cond.signal();
//	}
//
//	ke = m_backlog[m_off++];
//
//	process_handler:
//	int ident = ke.ident();
//
//	if(ident == m_out->ident()) {
//		m_out->poll_event();
//		lk.unlock();
//
//		m_kernel.reactivate(ke);
//
//	} else {
//		lk.unlock();
//
//		event_impl e(this, ke);
//		shared_handler h = m_state[ident];
//
//		bool cont = false;
//		if(h) {
//			try {
//				cont = (*h)(e);
//			} catch (...) { }
//		}
//
//		if(!e.is_reactivated()) {
//			if(e.is_removed()) {
//				return;
//			}
//			if(!cont) {
//				m_kernel.remove(ke);
//				reset_handler(ident);
//				return;
//			}
//			m_kernel.reactivate(ke);
//		}
//	}
//}
//
//
//void loop_impl::flush()
//{
//	pthread_scoped_lock lk(m_mutex);
//	while(!m_out->empty() || !m_task_queue.empty()) {
//		if(is_running()) {
//			m_flush_cond.wait(m_mutex);
//		} else {
//			run_once(lk);
//			if(!lk.owns()) {
//				lk.relock(m_mutex);
//			}
//		}
//	}
//}
//
//
//}  // noname namespace


loop::loop() : m_impl(new loop_impl()) { }

loop::~loop() { delete ANON_impl; }

void loop::run(size_t num)
{
	start(num);
	join();
}

void loop::start(size_t num)
	{ ANON_impl->start(num); }

bool loop::is_running() const
	{ return ANON_impl->is_running(); }

void loop::run_once()
	{ ANON_impl->run_once(); }

void loop::end()
	{ ANON_impl->end(); }

bool loop::is_end() const
	{ return ANON_impl->is_end(); }

void loop::join()
	{ ANON_impl->join(); }

//void loop::detach()
//	{ ANON_impl->detach(); }
//
//void loop::add_thread(size_t num)
//	{ ANON_impl->add_thread(num); }

void loop::submit_impl(task_t f)
	{ ANON_impl->submit_impl(f); }

//void loop::flush()
//	{ ANON_impl->flush(); }

// wavy_timer.h

class timer {
public:
	timer(HANDLE hiocp, double value_sec, double interval_sec,
			mp::function<bool ()> callback)
			: htimer(::CreateWaitableTimer(nullptr, FALSE, nullptr)), hiocp(hiocp), callback(callback)
	{
		assert(htimer != NULL);

		LARGE_INTEGER value;
		value.QuadPart = static_cast<long long>(value_sec * -10000000);
		BOOL res = ::SetWaitableTimer(htimer.get(), &value, static_cast<long>(interval_sec * 1000), nullptr, nullptr, FALSE);
		assert(res != 0);

		HANDLE hw;
		res = ::RegisterWaitForSingleObject(&hw, htimer.get(), on_timer_entry, this, INFINITE, WT_EXECUTEDEFAULT);
		assert(res);
		hwait.reset(hw);
	}

private:
	static void CALLBACK on_timer_entry(void* parameter, BOOLEAN)
	{
		auto pthis = static_cast<timer*>(parameter);
		::PostQueuedCompletionStatus(pthis->hiocp, 0, 0, new overlapped_callback(mp::bind(&timer::on_timer, pthis)));
	}

	void on_timer()
	{
		if (!callback())
		{
			::CancelWaitableTimer(htimer.get());
		}
	}

	unique_handle htimer;
	unique_wait_handle hwait;
	HANDLE hiocp;
	mp::function<bool ()> callback;

	timer(const timer&);
	timer& operator =(const timer&);
};

// wavy_timer.cc

//int loop::add_timer(const timespec* value, const timespec* interval,
//		function<bool ()> callback)
//{
//	kernel& kern(ANON_impl->get_kernel());
//
//	shared_handler sh(new timer_handler(kern, value, interval, callback));
//	ANON_impl->set_handler(sh);
//
//	return sh->ident();
//}


static inline struct timespec sec2spec(double sec)
{
	struct timespec spec = {
		(time_t)sec, (long)((sec - (double)(time_t)sec) * 1e9) };
	return spec;
}

int loop::add_timer(double value_sec, double interval_sec,
		mp::function<bool ()> callback)
{
	//if(value_sec >= 0.0) {
	//	if(interval_sec > 0.0) {
	//		struct timespec value = sec2spec(value_sec);
	//		struct timespec interval = sec2spec(interval_sec);
	//		return add_timer(&value, &interval, callback);
	//	} else {
	//		struct timespec value = sec2spec(value_sec);
	//		return add_timer(&value, NULL, callback);
	//	}
	//} else {
	//	if(interval_sec > 0.0) {
	//		struct timespec interval = sec2spec(interval_sec);
	//		return add_timer(NULL, &interval, callback);
	//	} else {
	//		// FIXME ambiguous overload
	//		return add_timer(NULL, (const timespec*)NULL, callback);
	//	}
	//}
	timers.push_back(std::make_shared<timer>(ANON_impl->hiocp.get(), value_sec, interval_sec, callback));
	return 0; // TODO: –ß‚è’l‚ð’¼‚·
}


//void loop::remove_timer(int ident)
//{
//	ANON_impl->reset_handler(ident);
//	kernel& kern(ANON_impl->get_kernel());
//	kern.remove_timer(ident);  // FIXME?
//}

// wavy_connect.cc

#include <MSWSock.h>

namespace {

template<typename T>
int GetExtensionFunctionPointer(SOCKET s, GUID guid, T& pfn)
{
	DWORD dwBytes;
	return ::WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof guid, &pfn, sizeof pfn, &dwBytes, NULL, NULL);
}

}  // noname namespace


//void loop::connect(
//		int socket_family, int socket_type, int protocol,
//		const sockaddr* addr, socklen_t addrlen,
//		double timeout_sec, connect_callback_t callback)
//{
//	struct timespec timeout = {
//		timeout_sec,
//		((timeout_sec - (double)(time_t)timeout_sec) * 1e9) };
//	return connect(socket_family, socket_type, protocol,
//			addr, addrlen, &timeout, callback);
//}

namespace {

struct connect_info {
	loop::connect_callback_t callback;
	SOCKET sock;
	HANDLE hwait;
	unique_handle hevent;
};

void CALLBACK on_connect(void* parameter, BOOLEAN timeout)
{
	assert(parameter);
	std::auto_ptr<connect_info> const info(static_cast<connect_info*>(parameter));
	if(info.get()) {
		if(timeout) {
			::closesocket(info->sock);
			info->callback(INVALID_SOCKET, WSAETIMEDOUT);
		} else {
			info->callback(info->sock, 0);
		}

		volatile HANDLE* p = &info->hwait;
		while(*p == NULL) {
			YieldProcessor();
		}
		UnregisterWait(*p);
	}
}

}  // noname namespace

void loop::connect(
	int socket_family, int socket_type, int protocol,
	const sockaddr* addr, socklen_t addrlen,
	double timeout_sec, connect_callback_t callback)
{
	int err = 0;
	SOCKET fd = ::WSASocket(socket_family, socket_type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (fd == INVALID_SOCKET) {
		err = WSAGetLastError();
		goto out_;
	}

	if (::CreateIoCompletionPort(reinterpret_cast<HANDLE>(fd), ANON_impl->hiocp.get(), 0, 0) == NULL) {
		err = GetLastError();
		goto error;
	}

#if 0
	{
		std::auto_ptr<connect_info> info(new connect_info);
		info->hevent.reset(::CreateEvent(NULL, TRUE, FALSE, NULL));
		if (WSAEventSelect(fd, info->hevent.get(), FD_CONNECT) != 0) {
			err = GetLastError();
			goto error;
		}
		if (WSAConnect(fd, addr, addrlen, NULL, NULL, NULL, NULL) != 0) {
			err = GetLastError();
			if (err != WSAEWOULDBLOCK) {
				goto error;
			}
		}
		info->callback = callback;
		info->sock = fd;
		if(RegisterWaitForSingleObject(&info->hwait, info->hevent.get(), on_connect, info.get(), static_cast<DWORD>(timeout_sec * 1000), WT_EXECUTEONLYONCE))
		{
			info.release();
			return;
		}
	}
#else
	using namespace mp::placeholders;

	// TODO: IPv4ˆÈŠO‚Ö‚Ì‘Î‰ž
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr.s_addr = INADDR_ANY;
	if (::bind(fd, reinterpret_cast<sockaddr const*>(&sin), sizeof sin) != 0) {
		err = GetLastError();
		goto error;
	}

	LPFN_CONNECTEX lpfnConnectEx = NULL;
	{
		GUID guid = WSAID_CONNECTEX;
		if (GetExtensionFunctionPointer(fd, guid, lpfnConnectEx) != 0)
		{
			err = WSAGetLastError();
			goto out_;
		}

		{
			std::auto_ptr<overlapped_callback> overlapped(
				new(std::nothrow) overlapped_callback(
					mp::bind(callback, fd, _3)));
			if (overlapped.get() == 0)
			{
				err = ERROR_OUTOFMEMORY;
				goto out_;
			}
			BOOL ret = lpfnConnectEx(fd, addr, addrlen, NULL, 0, NULL, overlapped.get());
			err = WSAGetLastError();
			if (ret || err == ERROR_IO_PENDING)
			{
				overlapped.release();
				return;
			}
		}
	}
#endif
error:
	::closesocket(fd);
	fd = INVALID_SOCKET;

out_:
	callback(fd, err);
}

// wavy_listen.cc

namespace {

void on_accept(SOCKET s, loop::listen_callback_t callback, HANDLE hiocp, loop* pthis)
{
	while(true) {
		unique_socket ps = pthis->accept(s, NULL, NULL);
		if(!ps) {
			return;
		}
		callback(ps, 0);
	}
}

struct listen_socket {
	unique_handle m_hevent;
	unique_wait_handle m_hwait;
	SOCKET m_lsock;

	listen_socket(unique_handle&& hevent, unique_wait_handle&& hwait, SOCKET s) :
		m_hevent(std::move(hevent)), m_hwait(std::move(hwait)), m_lsock(s) {
	}

	~listen_socket()
	{
		::closesocket(m_lsock);
	}
};

}  // noname namespace

mp::shared_ptr<SOCKET> loop::listen_accept(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		listen_callback_t callback,
		int backlog)
{
	SOCKET lsock = ::socket(socket_family, socket_type, protocol);
	if(lsock == INVALID_SOCKET) {
		throw mp::system_error(errno, "socket() failed");
	}

	BOOL on = TRUE;
	if(::setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on)) != 0) {
		::closesocket(lsock);
		throw mp::system_error(WSAGetLastError(), "setsockopt failed");
	}

	if(::bind(lsock, addr, addrlen) != 0) {
		::closesocket(lsock);
		throw mp::system_error(WSAGetLastError(), "bind failed");
	}

	if(::listen(lsock, backlog) != 0) {
		::closesocket(lsock);
		throw mp::system_error(WSAGetLastError(), "listen failed");
	}

	try {
		unique_handle hevent(::CreateEvent(NULL, FALSE, FALSE, NULL));
		unique_wait_handle hwait = register_wait_for_single_object(ANON_impl->hiocp.get(), hevent.get(), mp::bind(on_accept, lsock, callback, ANON_impl->hiocp.get(), this), INFINITE, WT_EXECUTEDEFAULT);
		if (::WSAEventSelect(lsock, hevent.get(), FD_ACCEPT) != 0) {
			::closesocket(lsock);
			throw mp::system_error(WSAGetLastError(), "bind failed");
		}
		mp::shared_ptr<listen_socket> ls(std::make_shared<listen_socket>(std::move(hevent), std::move(hwait), lsock));
		return mp::shared_ptr<SOCKET>(std::move(ls), &ls->m_lsock);

	} catch (...) {
		::closesocket(lsock);
		throw;
	}
}

mp::shared_ptr<SOCKET> loop::listen(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int backlog)
{
	SOCKET lsock = ::socket(socket_family, socket_type, protocol);
	if(lsock == INVALID_SOCKET) {
		throw mp::system_error(errno, "socket() failed");
	}

	mp::shared_ptr<SOCKET> ret = std::make_shared<SOCKET>(lsock);

	if(::CreateIoCompletionPort(reinterpret_cast<HANDLE>(lsock), ANON_impl->hiocp.get(), 0, 0) == NULL) {
		throw mp::system_error(GetLastError(), "CreateIoCompletionPort failed");
	}

	BOOL on = TRUE;
	if(::setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on)) != 0) {
		throw mp::system_error(WSAGetLastError(), "setsockopt failed");
	}
	
	if(::bind(lsock, addr, addrlen) != 0) {
		throw mp::system_error(WSAGetLastError(), "bind failed");
	}
	
	if(::listen(lsock, backlog) != 0) {
		throw mp::system_error(WSAGetLastError(), "listen failed");
	}

	return std::make_shared<SOCKET>(lsock);
}

namespace {

unique_socket accept_impl(SOCKET fd, sockaddr* addr, int addr_len)
{
	SOCKET ret = ::accept(fd, NULL, NULL);
	if(ret == INVALID_SOCKET) {
		int err = ::WSAGetLastError();
		if(err != WSAEWOULDBLOCK) {
			throw mp::system_error(err, "accept failed");
		}
		return unique_socket();
	}
	return unique_socket(ret);
}

}  // noname namespace

unique_socket loop::accept(SOCKET fd, sockaddr* addr, int addr_len)
{
	unique_socket sock = accept_impl(fd, addr, addr_len);
	if(sock) {
		if(::CreateIoCompletionPort(reinterpret_cast<HANDLE>(sock.get()), ANON_impl->hiocp.get(), 0, 0) == NULL) {
			int err = static_cast<int>(GetLastError());
			throw mp::system_error(err, "CreateIoCompletionPort failed");
		}

	}
	return sock;
}

void loop::accept_ex(SOCKET listen_socket, SOCKET accept_socket, void* buffer, size_t buffer_size, read_callback_t callback)
{
	LPFN_ACCEPTEX accept_ex = NULL;
	GUID guid_accept_ex = WSAID_ACCEPTEX;
	if(GetExtensionFunctionPointer(listen_socket, guid_accept_ex, accept_ex) != 0) {
		throw mp::system_error(WSAGetLastError(), "WSAIoctl [SIO_GET_EXTENSION_FUNCTION_POINTER] failed");
	}

	std::auto_ptr<overlapped_callback> overlapped(
		new overlapped_callback(mp::bind(callback, _3, _2)));

	const UINT addr_size = sizeof (sockaddr_in) + 16;
	if(!accept_ex(listen_socket, accept_socket, buffer, buffer_size - addr_size * 2, addr_size, addr_size, NULL, overlapped.get())) {
		int err = WSAGetLastError();
		if(err != WSA_IO_PENDING) {
			throw mp::system_error(err, "AcceptEx");
		}
	}

	if(::CreateIoCompletionPort(reinterpret_cast<HANDLE>(accept_socket), ANON_impl->hiocp.get(), 0, 0) == NULL) {
		int err = static_cast<int>(GetLastError());
		throw mp::system_error(err, "CreateIoCompletionPort failed");
	}

	overlapped.release();
}


}  // namespace windows
}  // namespace impl
}  // namespace rpc
}  // namespace msgpack
