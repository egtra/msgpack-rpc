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

}

// wavy_out.h


class out : public basic_handler {
public:
	out();
	~out();

	typedef loop::finalize_t finalize_t;

	inline void commit_raw(SOCKET fd, char* xfbuf, char* xfendp);

	// optimize
	inline void commit(SOCKET fd, xfer* xf);
//	inline void write(int fd, const void* buf, size_t size);
//
//public:
//	bool has_queue() const
//	{
//		return !m_queue.empty();
//	}
//
//	bool empty() const
//	{
//		return m_watching == 0;
//	}
//
//private:
//	volatile int m_watching;
//
//	void watch(int fd);
	void* m_fdctx;

private:
	out(const out&);
};

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
	//volatile size_t m_off;
	//volatile size_t m_num;
	//volatile bool m_pollable;
//	volatile pthread_t m_poll_thread;  // FIXME signal_stop

//	kernel::backlog m_backlog;

//	shared_handler* m_state;

//	kernel m_kernel;

//	pthread_mutex m_mutex;
//	pthread_cond m_cond;

	typedef std::queue<task_t> task_queue_t;
//	task_queue_t m_task_queue;

	mp::function<void ()> m_thread_init_func;

//	pthread_cond m_flush_cond;

private:
	mp::shared_ptr<out> m_out;
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

// wavy_out.cc

namespace {

// Compatible layout for WSABUF
struct iovec {
	unsigned long iov_len;
	void *iov_base;
};


class xfer_impl : public xfer {
public:
	xfer_impl() { }
	~xfer_impl() { }

	bool try_write(SOCKET fd);

	void push_xfraw(char* buf, size_t size);

	static size_t sizeof_mem();
	static size_t sizeof_iovec(size_t veclen);
	static size_t sizeof_sendfile();
	static size_t sizeof_finalize();

	static char* fill_mem(char* from, const void* buf, size_t size);
	static char* fill_iovec(char* from, const struct iovec* vec, size_t veclen);
	static char* fill_sendfile(char* from, int infd, uint64_t off, size_t len);
	static char* fill_finalize(char* from, finalize_t fin, void* user);

	static bool execute(SOCKET fd, char* head, char** tail);

public:
	pthread_mutex& mutex() { return m_mutex; }

private:
	pthread_mutex m_mutex;

private:
	xfer_impl(const xfer_impl&);
};


typedef unsigned int xfer_type;

static const xfer_type XF_IOVEC    = 0;
static const xfer_type XF_SENDFILE = 1;
static const xfer_type XF_FINALIZE = 3;

struct xfer_sendfile {
	int infd;
	uint64_t off;
	size_t len;
};

struct xfer_finalize {
	void (*finalize)(void*);
	void* user;
};


inline size_t xfer_impl::sizeof_mem()
{
	return sizeof(xfer_type) + sizeof(struct iovec)*1;
}

inline size_t xfer_impl::sizeof_iovec(size_t veclen)
{
	return sizeof(xfer_type) + sizeof(iovec) * veclen;
}

inline size_t xfer_impl::sizeof_sendfile()
{
	return sizeof(xfer_type) + sizeof(xfer_sendfile);
}

inline size_t xfer_impl::sizeof_finalize()
{
	return sizeof(xfer_type) + sizeof(xfer_finalize);
}

inline char* xfer_impl::fill_mem(char* from, const void* buf, size_t size)
{
	*(xfer_type*)from = 1 << 1;
	from += sizeof(xfer_type);

	((struct iovec*)from)->iov_base = const_cast<void*>(buf);
	((struct iovec*)from)->iov_len  = size;
	from += sizeof(struct iovec);

	return from;
}

inline char* xfer_impl::fill_iovec(char* from, const struct iovec* vec, size_t veclen)
{
	*(xfer_type*)from = veclen << 1;
	from += sizeof(xfer_type);

	const size_t iovbufsz = sizeof(struct iovec) * veclen;
	memcpy(from, vec, iovbufsz);
	from += iovbufsz;

	return from;
}

//inline char* xfer_impl::fill_sendfile(char* from, int infd, uint64_t off, size_t len)
//{
//	*(xfer_type*)from = XF_SENDFILE;
//	from += sizeof(xfer_type);
//
//	((xfer_sendfile*)from)->infd = infd;
//	((xfer_sendfile*)from)->off = off;
//	((xfer_sendfile*)from)->len = len;
//	from += sizeof(xfer_sendfile);
//
//	return from;
//}

inline char* xfer_impl::fill_finalize(char* from, finalize_t fin, void* user)
{
	*(xfer_type*)from = XF_FINALIZE;
	from += sizeof(xfer_type);

	((xfer_finalize*)from)->finalize = fin;
	((xfer_finalize*)from)->user = user;
	from += sizeof(xfer_finalize);

	return from;
}

void xfer_impl::push_xfraw(char* buf, size_t size)
{
	if(m_free < size) { reserve(size); }
	memcpy(m_tail, buf, size);
	m_tail += size;
	m_free -= size;
}


#define MP_WAVY_XFER_CONSUMED \
	do { \
		size_t left = endp - p; \
		::memmove(head, p, left); \
		*tail = head + left; \
	} while(0)

bool xfer_impl::execute(SOCKET fd, char* head, char** tail)
{
	char* p = head;
	char* const endp = *tail;
	while(p < endp) {
		switch(*(xfer_type*)p) {
		case XF_SENDFILE: {
			xfer_sendfile* x = (xfer_sendfile*)(p + sizeof(xfer_type));
#if defined(__linux__) || defined(__sun__)
			off_t off = x->off;
			ssize_t wl = ::sendfile(fd, x->infd, &off, x->len);
			if(wl <= 0) {
				MP_WAVY_XFER_CONSUMED;
				if(wl < 0 && (errno == EAGAIN || errno == EINTR)) {
					return true;
				} else {
					return false;
				}
			}
#elif defined(__APPLE__) && defined(__MACH__)
			off_t wl = x->len;
			if(::sendfile(x->infd, fd, x->off, &wl, NULL, 0) < 0) {
				MP_WAVY_XFER_CONSUMED;
				if(errno == EAGAIN || errno == EINTR) {
					return true;
				} else {
					return false;
				}
			}
#elif defined(_WIN32)
			int wl = 0;
			assert(false);
			return false;
#else
			off_t sbytes = 0;
			if(::sendfile(x->infd, fd, x->off, x->len, NULL, &sbytes, 0) < 0) {
				MP_WAVY_XFER_CONSUMED;
				if(errno == EAGAIN || errno == EINTR) {
					return true;
				} else {
					return false;
				}
			}
			off_t wl = x->len + sbytes;
#endif

			if(static_cast<size_t>(wl) < x->len) {
				x->off += wl;
				x->len -= wl;
				MP_WAVY_XFER_CONSUMED;
				return true;
			}

			p += sizeof_sendfile();
			break; }

		case XF_FINALIZE: {
			xfer_finalize* x = (xfer_finalize*)(p + sizeof(xfer_type));
			if(x->finalize) try {
				x->finalize(x->user);
			} catch (...) { }

			p += xfer_impl::sizeof_finalize();
			break; }

		default: {  // XF_IOVEC
			size_t veclen = (*(xfer_type*)p) >> 1;
			struct iovec* vec = (struct iovec*)(p + sizeof(xfer_type));
#ifndef _WIN32
			ssize_t wl = ::writev(fd, vec, veclen);
			if(wl <= 0) {
				MP_WAVY_XFER_CONSUMED;
				if(wl < 0 && (errno == EAGAIN || errno == EINTR)) {
					return true;
				} else {
					return false;
				}
			}
#else
			DWORD wl;
			int result = ::WSASend(fd, reinterpret_cast<WSABUF*>(vec), veclen, &wl, 0, NULL, NULL);
			if(result != 0) {
				MP_WAVY_XFER_CONSUMED;
				if(wl < 0 && (errno == WSAEWOULDBLOCK)) {
					return true;
				} else {
					return false;
				}
			}
#endif
			for(size_t i=0; i < veclen; ++i) {
				if(static_cast<size_t>(wl) >= vec[i].iov_len) {
					wl -= vec[i].iov_len;
				} else {
					vec[i].iov_base = (void*)(((char*)vec[i].iov_base) + wl);
					vec[i].iov_len -= wl;

					if(i == 0) {
						MP_WAVY_XFER_CONSUMED;
					} else {
						p += sizeof_iovec(veclen);
						size_t left = endp - p;
						char* filltail = fill_iovec(head, vec+i, veclen-i);
						::memmove(filltail, p, left);
						*tail = filltail + left;
					}

					return true;
				}
			}

			p += sizeof_iovec(veclen);

			break; }
		}
	}

	*tail = head;
	return false;
}


bool xfer_impl::try_write(SOCKET fd)
{
	char* const alloc_end = m_tail + m_free;
	bool cont = execute(fd, m_head, &m_tail);
	m_free = alloc_end - m_tail;

	if(!cont && !empty()) {
		// error occured
		::shutdown(fd, SD_SEND);
	}
	return cont;
}


}  // noname namespace


void xfer::reserve(size_t reqsz)
{
	size_t used = m_tail - m_head;
	reqsz += used;
	size_t nsize = (used + m_free) * 2 + 72;  // used + m_free may be 0

	while(nsize < reqsz) { nsize *= 2; }

	char* tmp = (char*)::realloc(m_head, nsize);
	if(!tmp) { throw std::bad_alloc(); }

	m_head = tmp;
	m_tail = tmp + used;
	m_free = nsize - used;
}


void xfer::push_write(const void* buf, size_t size)
{
	size_t sz = xfer_impl::sizeof_mem();
	if(m_free < sz) { reserve(sz); }
	m_tail = xfer_impl::fill_mem(m_tail, buf, size);
	m_free -= sz;
}

//void xfer::push_writev(const struct iovec* vec, size_t veclen)
//{
//	size_t sz = xfer_impl::sizeof_iovec(veclen);
//	if(m_free < sz) { reserve(sz); }
//	m_tail = xfer_impl::fill_iovec(m_tail, vec, veclen);
//	m_free -= sz;
//}
//
//void xfer::push_sendfile(int infd, uint64_t off, size_t len)
//{
//	size_t sz = xfer_impl::sizeof_sendfile();
//	if(m_free < sz) { reserve(sz); }
//	m_tail = xfer_impl::fill_sendfile(m_tail, infd, off, len);
//	m_free -= sz;
//}

void xfer::push_finalize(finalize_t fin, void* user)
{
	size_t sz = xfer_impl::sizeof_finalize();
	if(m_free < sz) { reserve(sz); }
	m_tail = xfer_impl::fill_finalize(m_tail, fin, user);
	m_free -= sz;
}

void xfer::migrate(xfer* to)
{
	if(to->m_head == NULL) {
		// swap
		to->m_head = m_head;
		to->m_tail = m_tail;
		to->m_free = m_free;
		m_tail = m_head = NULL;
		m_free = 0;
		return;
	}

	size_t reqsz = m_tail - m_head;
	if(to->m_free < reqsz) { to->reserve(reqsz); }
	
	memcpy(to->m_tail, m_head, reqsz);
	to->m_tail += reqsz;
	to->m_free -= reqsz;
	
	m_free += reqsz;
	m_tail = m_head;
}

void xfer::clear()
{
//	for(char* p = m_head; p < m_tail; ) {
//		switch(*(xfer_type*)p) {
//		case XF_SENDFILE:
//			p += xfer_impl::sizeof_sendfile();
//			break;
//
//		case XF_FINALIZE: {
//			xfer_finalize* x = (xfer_finalize*)(p + sizeof(xfer_type));
//			if(x->finalize) try {
//				x->finalize(x->user);
//			} catch (...) { }
//
//			p += xfer_impl::sizeof_finalize();
//			break; }
//
//		default:  // XF_IOVEC
//			p += xfer_impl::sizeof_iovec( (*(xfer_type*)p) >> 1 );
//			break;
//		}
	}

//	//::free(m_head);
//	//m_tail = m_head = NULL;
//	//m_free = 0;
//	m_free += m_tail - m_head;
//	m_tail = m_head;
//}


//#define ANON_fdctx (*reinterpret_cast<mp::unordered_map<SOCKET, mp::shared_ptr<xfer_impl>*>(m_fdctx))

inline xfer_impl& get_xfer_impl_from_fdctx(void* fdctx, SOCKET s)
{
	mp::shared_ptr<xfer_impl>& p = (*reinterpret_cast<mp::unordered_map<SOCKET, mp::shared_ptr<xfer_impl>>*>(fdctx))[s];
	if (!p)
	{
		p.reset(new xfer_impl);
	}
	return *p;
}

#define ANON_fdctx_at(fd) (get_xfer_impl_from_fdctx(m_fdctx, fd))

out::out() : basic_handler(0/*m_kernel.ident()*/)/*, m_watching(0)*/
{
	//struct rlimit rbuf;
	//if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
	//	throw system_error(errno, "getrlimit() failed");
	//}
	m_fdctx = new mp::unordered_map<SOCKET, mp::shared_ptr<xfer_impl>>;
}

out::~out()
{
	delete reinterpret_cast<mp::unordered_map<SOCKET, mp::shared_ptr<xfer_impl>>*>(m_fdctx);
}


//inline void out::watch(int fd)
//{
//	m_kernel.add_fd(fd, EVKERNEL_WRITE);
//	__sync_add_and_fetch(&m_watching, 1);
//}


void out::commit_raw(SOCKET fd, char* xfbuf, char* xfendp)
{
	xfer_impl& ctx(ANON_fdctx_at(fd));
	pthread_scoped_lock lk(ctx.mutex());

	if(!ctx.empty()) {
		ctx.push_xfraw(xfbuf, xfendp - xfbuf);
		return;
	}

	if(xfer_impl::execute(fd, xfbuf, &xfendp)) {
		ctx.push_xfraw(xfbuf, xfendp - xfbuf);  // FIXME exception
//		watch(fd);  // FIXME exception
	}
}

void out::commit(SOCKET fd, xfer* xf)
{
	xfer_impl& ctx(ANON_fdctx_at(fd));
	pthread_scoped_lock lk(ctx.mutex());

	if(!ctx.empty()) {
		xf->migrate(&ctx);
		return;
	}

	if(static_cast<xfer_impl*>(xf)->try_write(fd)) {
		xf->migrate(&ctx);  // FIXME exception
//		watch(fd);  // FIXME exception
	}
}

//void out::write(int fd, const void* buf, size_t size)
//{
//	xfer_impl& ctx(ANON_fdctx[fd]);
//	pthread_scoped_lock lk(ctx.mutex());
//
//	if(ctx.empty()) {
//		ssize_t wl = ::write(fd, buf, size);
//		if(wl <= 0) {
//			if(wl == 0 || (errno != EINTR && errno != EAGAIN)) {
//				::shutdown(fd, SHUT_RD);
//				return;
//			}
//		} else if(static_cast<size_t>(wl) >= size) {
//			return;
//		} else {
//			buf  = ((const char*)buf) + wl;
//			size -= wl;
//		}
//
//		ctx.push_write(buf, size);
//		watch(fd);
//
//	} else {
//		ctx.push_write(buf, size);
//	}
//}


#define ANON_out static_cast<loop_impl*>(m_impl)->m_out

void loop::commit(SOCKET fd, xfer* xf)
	{ ANON_out->commit(fd, xf); }

//void loop::write(int fd, const void* buf, size_t size)
//	{ ANON_out->write(fd, buf, size); }

namespace {

void on_write(DWORD transferred, DWORD error, const void* buf, loop::finalize_t fin)
{
	if(error != 0) {
		LOG_WARN("write error: ", mp::system_error::errno_string(error));
	}
	fin(const_cast<void*>(buf));
}

}  // noname space

void loop::write(SOCKET fd,
		const void* buf, size_t size,
		finalize_t fin, void* user)
{
////	char xfbuf[ xfer_impl::sizeof_mem() + xfer_impl::sizeof_finalize() ];
//	char* xfbuf = static_cast<char*>(_alloca(xfer_impl::sizeof_mem() + xfer_impl::sizeof_finalize()));
//	char* p = xfbuf;
//	p = xfer_impl::fill_mem(p, buf, size);
//	p = xfer_impl::fill_finalize(p, fin, user);
//	ANON_out->commit_raw(fd, xfbuf, p);

	using namespace mp::placeholders;

	assert(size <= std::numeric_limits<DWORD>::max());
	WSABUF wb = {size, const_cast<char*>(static_cast<const char*>(buf))};
	std::auto_ptr<impl::windows::overlapped_callback> overlapped(new impl::windows::overlapped_callback(
		mp::bind(&on_write, _2, _3, buf, fin)));
	BOOL ret = ::WSASend(fd, &wb, 1, 0, 0, overlapped.get(), NULL);
	if(ret != 0) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) { throw mp::system_error(err, "write error"); }
	}

	overlapped.release();
}

//void loop::writev(int fd,
//		const struct iovec* vec, size_t veclen,
//		finalize_t fin, void* user)
//{
//	char xfbuf[ xfer_impl::sizeof_iovec(veclen) + xfer_impl::sizeof_finalize() ];
//	char* p = xfbuf;
//	p = xfer_impl::fill_iovec(p, vec, veclen);
//	p = xfer_impl::fill_finalize(p, fin, user);
//	ANON_out->commit_raw(fd, xfbuf, p);
//}
//
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
	//m_state = new shared_handler[m_kernel.max()];

	// add out handler
	{
		m_out.reset(new out);
		//set_handler(m_out);
		//get_kernel().add_kernel(&m_out->get_kernel());
	}
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
//		m_cond.broadcast();
//		if(m_poll_thread) {  // FIXME signal_stop
//			pthread_kill(m_poll_thread, SIGALRM);
//		}
		// need lock
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

#if 1
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
	// TODO: IPv4ˆÈŠO‚Ö‚Ì‘Î‰ž
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr.s_addr = INADDR_ANY;
	//sin.sin_addr = reinterpret_cast<const sockaddr_in*>(addr)->sin_addr;
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
					mp::bind(callback, fd, 0)));
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

void on_accept(SOCKET s, loop::listen_callback_t callback, HANDLE hiocp)
{
	while(true) {
		SOCKET sock = ::accept(s, NULL, NULL);
		if(sock == INVALID_SOCKET) {
			DWORD err = ::WSAGetLastError();
			if(err == WSAEWOULDBLOCK) {
				return;
			} 
			callback(sock, err);
			throw mp::system_error(err, "accept failed");
		}

		if(::CreateIoCompletionPort(reinterpret_cast<HANDLE>(sock), hiocp, 0, 0) == NULL) {
			::closesocket(sock);
			DWORD err = GetLastError();
			callback(sock, err);
			throw mp::system_error(err, "CreateIoCompletionPort failed");
		}

		try {
			callback(sock, 0);
		} catch(...) {
			::closesocket(sock);
		}
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

mp::shared_ptr<SOCKET> loop::listen(
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
		unique_wait_handle hwait = register_wait_for_single_object(ANON_impl->hiocp.get(), hevent.get(), mp::bind(on_accept, lsock, callback, ANON_impl->hiocp.get()), INFINITE, WT_EXECUTEDEFAULT);
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

}  // namespace windows
}  // namespace impl
}  // namespace rpc
}  // namespace msgpack
