#ifndef IOCPLOOP_H__
#define IOCPLOOP_H__

#include <mp/functional.h>
#include <mp/object_delete.h>
#include <mp/memory.h>
#include <mp/sync.h>


namespace msgpack {
namespace rpc {
namespace impl {
namespace windows {


struct overlapped_callback : ::OVERLAPPED
{
	typedef mp::function<void (::OVERLAPPED const& ov, DWORD transfered, DWORD error)> callback_t;

	overlapped_callback(callback_t callback) : ::OVERLAPPED(), callback(callback) {}
	callback_t callback;

private:
	overlapped_callback(const overlapped_callback&);
	overlapped_callback& operator =(const overlapped_callback&);
};


class xfer2;


typedef int socklen_t;


template<BOOL (WINAPI* deleter)(HANDLE)>
struct handle_deleter : std::unary_function<HANDLE, void>
{
	typedef HANDLE pointer;

	void operator ()(HANDLE h) const
	{
		deleter(h);
	}
};

struct socket_deleter : std::unary_function<SOCKET, void>
{
	typedef SOCKET pointer;

	void operator ()(SOCKET h) const
	{
		::closesocket(h);
	}
};

typedef std::unique_ptr<HANDLE, handle_deleter<::CloseHandle>> unique_handle;
typedef std::unique_ptr<SOCKET, socket_deleter> unique_socket;
typedef std::unique_ptr<HANDLE, handle_deleter<::UnregisterWait>> unique_wait_handle;


class loop {
public:
	loop();
//	loop(function<void ()> thread_init_func);

	~loop();

	void start(size_t num);

	void run(size_t num);   // run = start + join

	bool is_running() const;

	void run_once();

	void end();
	bool is_end() const;

	void join();
	//void detach();

	void add_thread(size_t num);


	void remove_handler(SOCKET fd);


	typedef mp::function<void (int fd, int err)> connect_callback_t;

	//void connect(
	//		int socket_family, int socket_type, int protocol,
	//		const sockaddr* addr, socklen_t addrlen,
	//		const timespec* timeout, connect_callback_t callback);

	void connect(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			double timeout_sec, connect_callback_t callback);


	typedef mp::function<void (unique_socket& fd, int err)> listen_callback_t;

	mp::shared_ptr<SOCKET> listen_accept(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			listen_callback_t callback,
			int backlog = 1024);

	mp::shared_ptr<SOCKET> listen(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int backlog = 1024);

	unique_socket accept(SOCKET fd, sockaddr* addr, int addr_len);

	typedef mp::function<void (DWORD error, DWORD transferred)> read_callback_t;

	void accept_ex(SOCKET listen_socket, SOCKET accept_socket, void* buffer, size_t buffer_size, read_callback_t callback);

	//int add_timer(const timespec* value, const timespec* interval,
	//		function<bool ()> callback);

	intptr_t add_timer(double value_sec, double interval_sec,
			mp::function<bool ()> callback);

	void remove_timer(intptr_t ident);

	void read(SOCKET fd, void* buf, size_t size, read_callback_t callback);


	typedef void (*finalize_t)(void* user);


	//void write(int fd, const void* buf, size_t size);

	void write(SOCKET fd, const void* buf, size_t size,
			finalize_t fin, void* user);

	//template <typename T>
	//void write(int fd, const void* buf, size_t size,
	//		std::auto_ptr<T>& fin);

	//template <typename T>
	//void write(int fd, const void* buf, size_t size,
	//		mp::shared_ptr<T> fin);


	void writev(SOCKET fd, const struct iovec* vec, size_t veclen,
			finalize_t fin, void* user);

	template <typename T>
	void writev(SOCKET fd, const struct iovec* vec, size_t veclen,
			std::auto_ptr<T>& fin);

	//template <typename T>
	//void writev(int fd, const struct iovec* vec, size_t veclen,
	//		mp::shared_ptr<T> fin);


	//void sendfile(int fd, int infd, uint64_t off, size_t size,
	//		finalize_t fin, void* user);
	//
	//void hsendfile(int fd,
	//		const void* header, size_t header_size,
	//		int infd, uint64_t off, size_t size,
	//		finalize_t fin, void* user);
	//
	//void hvsendfile(int fd,
	//		const struct iovec* header_vec, size_t header_veclen,
	//		int infd, uint64_t off, size_t size,
	//		finalize_t fin, void* user);


	void commit(SOCKET fd, xfer2* xf);


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

private:
	void* m_impl;

	loop(const loop&);
};


class xfer2 {
public:
	xfer2();
	~xfer2();

	typedef loop::finalize_t finalize_t;

	void push_write(const void* buf, size_t size);

	void push_writev(const struct iovec* vec, size_t veclen);

	//void push_sendfile(int infd, uint64_t off, size_t len);

	void push_finalize(finalize_t fin, void* user);

	template <typename T>
	void push_finalize(std::auto_ptr<T> fin);

	//template <typename T>
	//void push_finalize(mp::shared_ptr<T> fin);

	bool empty() const;

	void clear();

	//void migrate(xfer* to);

	friend class loop;

	struct sync_t {
		std::vector<WSABUF> buffer;
		std::vector<mp::function<void ()> > finalizer;

		void clear();
		void swap(sync_t& y);
	};

private:
	typedef mp::sync<sync_t>::ref sync_ref;
	mp::sync<sync_t> m_sync;

	xfer2(const xfer2&);
	xfer2& operator= (const xfer2&);
};


class basic_handler {
public:
	basic_handler(SOCKET ident) :
		m_ident(ident) { }

	virtual ~basic_handler() { }

	SOCKET ident() const { return m_ident; }

	SOCKET fd() const { return ident(); }

private:
	SOCKET m_ident;

private:
	basic_handler();
	basic_handler(const basic_handler&);
};


class handler : public mp::enable_shared_from_this<handler>, public basic_handler {
public:
	handler(SOCKET fd) : basic_handler(fd) { }

	~handler() { ::closesocket(fd()); }

public:
	template <typename IMPL>
	mp::shared_ptr<IMPL> shared_self()
	{
		return mp::static_pointer_cast<IMPL>(enable_shared_from_this<handler>::shared_from_this());
	}

	template <typename IMPL>
	mp::shared_ptr<IMPL const> shared_self() const
	{
		return mp::static_pointer_cast<IMPL>(enable_shared_from_this<handler>::shared_from_this());
	}
};


template <typename F>
inline void loop::submit(F f)
	{ submit_impl(task_t(f)); }
template <typename F, typename A1>
inline void loop::submit(F f, A1 a1)
	{ submit_impl(mp::bind(f, a1)); }
template <typename F, typename A1, typename A2>
inline void loop::submit(F f, A1 a1, A2 a2)
	{ submit_impl(mp::bind(f, a1, a2)); }
template <typename F, typename A1, typename A2, typename A3>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3)
	{ submit_impl(mp::bind(f, a1, a2, a3)); }
template <typename F, typename A1, typename A2, typename A3, typename A4>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15)); }
template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10, typename A11, typename A12, typename A13, typename A14, typename A15, typename A16>
inline void loop::submit(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9, A10 a10, A11 a11, A12 a12, A13 a13, A14 a14, A15 a15, A16 a16)
	{ submit_impl(mp::bind(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16)); }


inline xfer2::xfer2() { }

inline xfer2::~xfer2()
{
	clear();
}

//inline bool xfer2::empty() const
//{
//	sync_ref ref(const_cast<mp::sync<sync_t>&>(m_sync));
//	return ref->buffer.empty();
//}

template <typename T>
inline void xfer2::push_finalize(std::auto_ptr<T> fin)
{
	push_finalize(&mp::object_delete<T>, reinterpret_cast<void*>(fin.get()));
	fin.release();
}

//template <typename T>
//inline void xfer2::push_finalize(mp::shared_ptr<T> fin)
//{
//	std::auto_ptr<mp::shared_ptr<T> > afin(new mp::shared_ptr<T>(fin));
//	push_finalize(afin);
//}
//
//template <typename T>
//inline void loop::write(int fd, const void* buf, size_t size,
//		std::auto_ptr<T>& fin)
//{
//	write(fd, buf, size, &mp::object_delete<T>, fin.get());
//	fin.release();
//}
//
//template <typename T>
//inline void loop::write(int fd, const void* buf, size_t size,
//		mp::shared_ptr<T> fin)
//{
//	std::auto_ptr<mp::shared_ptr<T> > afin(new mp::shared_ptr<T>(fin));
//	write(fd, buf, size, afin);
//}

template <typename T>
inline void loop::writev(SOCKET fd, const struct iovec* vec, size_t veclen,
		std::auto_ptr<T>& fin)
{
	writev(fd, vec, veclen, &mp::object_delete<T>, fin.get());
	fin.release();
}

//template <typename T>
//inline void loop::writev(int fd, const struct iovec* vec, size_t veclen,
//		mp::shared_ptr<T> fin)
//{
//	std::auto_ptr<mp::shared_ptr<T> > afin(new mp::shared_ptr<T>(fin));
//	writev(fd, vec, veclen, afin);
//}



}  // namespace windows
}  // namespace impl
}  // namespace rpc
}  // namespace msgpack


namespace mp {

namespace wavy = msgpack::rpc::impl::windows;

}  // namespace mp

#endif /* iocploop.h */
