//
// msgpack::rpc::transport::tcp - MessagePack-RPC for C++
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
#include "../types.h"
#include "../transport/base.h"
#include "../transport/tcp.h"
#include "cclog/cclog.h"
#include <mp/functional.h>
#include <mp/sync.h>
#include <mp/utilize.h>
#include <vector>
#include <mswsock.h>

namespace msgpack {
namespace rpc {
namespace transport {
namespace tcp {

namespace {

using namespace mp::placeholders;

using msgpack::rpc::impl::windows::unique_socket;

class client_transport;
class server_transport;


class client_socket : public stream_handler<client_socket> {
public:
	client_socket(SOCKET sock, client_transport* tran, session_impl* s);
	~client_socket();

	void on_response(msgid_t msgid,
			object result, object error, auto_zone z);

private:
	client_transport* m_tran;
	weak_session m_session;

private:
	client_socket();
	client_socket(const client_socket&);
};


class client_transport : public rpc::client_transport {
public:
	client_transport(session_impl* s, const address& addr, const tcp_builder& b);
	~client_transport();

public:
	void send_data(sbuffer* sbuf);
	void send_data(auto_vreflife vbuf);

private:
	typedef std::vector<mp::shared_ptr<client_socket> > sockpool_t;

	struct sync_t {
		sync_t() : sockpool_rr(0), connecting(0) { }
		sockpool_t sockpool;
		size_t sockpool_rr;
		unsigned int connecting;
		mp::wavy::xfer2 pending_xf;
	};

	typedef mp::sync<sync_t>::ref sync_ref;
	mp::sync<sync_t> m_sync;

	session_impl* m_session;

	double m_connect_timeout;
	unsigned int m_reconnect_limit;

private:
	void try_connect(sync_ref& lk_ref);
	static void on_connect(unique_socket& fd, int err, weak_session ws, client_transport* self);
	void on_connect_success(unique_socket& fd, sync_ref& ref);
	void on_connect_failed(int err, sync_ref& ref);

	friend class client_socket;
	void on_close(client_socket* sock);

private:
	client_transport();
	client_transport(const client_transport&);
};


client_socket::client_socket(SOCKET sock, client_transport* tran, session_impl* s) :
	stream_handler<client_socket>(sock, s->get_loop()),
	m_tran(tran), m_session(s->shared_from_this()) { }

client_socket::~client_socket()
{
	shared_session s = m_session.lock();
	if(!s) {
		return;
	}
	m_tran->on_close(this);
}

void client_socket::on_response(msgid_t msgid,
			object result, object error, auto_zone z)
{
	shared_session s = m_session.lock();
	if(!s) {
		throw closed_exception();
	}
	s->on_response(msgid, result, error, z);
}


client_transport::client_transport(session_impl* s, const address& addr, const tcp_builder& b) :
	m_session(s),
	m_connect_timeout(b.connect_timeout()),
	m_reconnect_limit(b.reconnect_limit()) { }

client_transport::~client_transport()
{
	// avoids dead lock
	sockpool_t sockpool;

	{
		sync_ref ref(m_sync);
		sockpool.swap(ref->sockpool);
		sockpool.clear();
	}
}

mp::shared_ptr<client_socket> make_client_socket(unique_socket& s, client_transport* t, session_impl* session)
{
	mp::shared_ptr<client_socket> cs(mp::make_shared<client_socket>(s.get(), t, session));
	s.release();
	return cs;
}

inline void client_transport::on_connect_success(unique_socket& fd, sync_ref& ref)
{
	LOG_DEBUG("connect success to ",m_session->get_address()," fd=",fd.get());

	mp::shared_ptr<client_socket> cs(make_client_socket(fd, this, m_session));
	cs->async_read();

	ref->sockpool.push_back(cs);

	m_session->get_loop()->commit(cs->fd(), &ref->pending_xf);
	ref->pending_xf.clear();

	ref->connecting = 0;
}

void client_transport::on_connect_failed(int err, sync_ref& ref)
{
	if(ref->connecting < m_reconnect_limit) {
		LOG_WARN("connect to ",m_session->get_address()," failed, retrying: ",mp::system_error::errno_string(err));
		try_connect(ref);
		++ref->connecting;
		return;
	}

	LOG_WARN("connect to ",m_session->get_address()," failed, abort: ",mp::system_error::errno_string(err));
	ref->connecting = 0;
	ref->pending_xf.clear();

	ref.reset();
	m_session->on_connect_failed();
}

void client_transport::on_connect(unique_socket& fd, int err, weak_session ws, client_transport* self)
{
	shared_session s = ws.lock();
	if(!s) {
		return;
	}

	sync_ref ref(self->m_sync);

	if(fd) {
		if(setsockopt(fd.get(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0) == 0) {
			// success
			try {
				self->on_connect_success(fd, ref);
				return;
			} catch (...) {
				err = E_UNEXPECTED;
				LOG_WARN("attach failed or send pending failed");
			}
		} else {
			err = WSAGetLastError();
		}
	}

	self->on_connect_failed(err, ref);
}

void client_transport::try_connect(sync_ref& lk_ref)
{
	address addr = m_session->get_address();

	LOG_INFO("connecting to ",addr);

	sockaddr_storage addrbuf;
	addr.get_addr((sockaddr*)&addrbuf);

	m_session->get_loop()->connect(
			PF_INET, SOCK_STREAM, 0,
			(sockaddr*)&addrbuf, addr.get_addrlen(),
			m_connect_timeout,
			mp::bind(
				&client_transport::on_connect,
				_1, _2,
				weak_session(m_session->shared_from_this()), this
				));
}

void client_transport::on_close(client_socket* sock)
{
	sync_ref ref(m_sync);
	sockpool_t::iterator found = std::find_if(
			ref->sockpool.begin(), ref->sockpool.end(), mp::bind(std::equal_to<client_socket*>(), mp::bind(&mp::shared_ptr<client_socket>::get, _1), sock));
	if(found != ref->sockpool.end()) {
		ref->sockpool.erase(found);
	}
}


void client_transport::send_data(sbuffer* sbuf)
{
	sync_ref ref(m_sync);
	if(ref->sockpool.empty()) {
		if(ref->connecting == 0) {
			try_connect(ref);
			ref->connecting = 1;
		}
		ref->pending_xf.push_write(sbuf->data(), sbuf->size());
		ref->pending_xf.push_finalize(&::free, sbuf->data());
		sbuf->release();
	} else {
		// FIXME pesudo connecting load balance
		mp::shared_ptr<client_socket> sock = ref->sockpool[0];
		sock->send_data(sbuf);
	}
}

void client_transport::send_data(auto_vreflife vbuf)
{
	sync_ref ref(m_sync);
	if(ref->sockpool.empty()) {
		if(ref->connecting == 0) {
			try_connect(ref);
			ref->connecting = 1;
		}
		ref->pending_xf.push_writev(vbuf->vector(), vbuf->vector_size());
		ref->pending_xf.push_finalize(vbuf);
	} else {
		// FIXME pesudo connecting load balance
		mp::shared_ptr<client_socket> sock = ref->sockpool[0];
		sock->send_data(vbuf);
	}
}


class server_socket : public stream_handler<server_socket> {
public:
	server_socket(SOCKET sock, shared_server svr);
	server_socket(shared_server svr);
	~server_socket();

	void on_request(
			msgid_t msgid,
			object method, object params, auto_zone z);

	void on_notify(
			object method, object params, auto_zone z);

	void accept_ex(SOCKET lsock, mp::function<void ()> accept_callback);

	static void on_accept_ex(mp::weak_ptr<stream_handler> whandler, DWORD error, DWORD transferred, SOCKET lsock, mp::function<void ()> callback);
private:
	weak_server m_svr;

private:
	server_socket();
	server_socket(const server_socket&);
};


class server_transport : public rpc::server_transport {
	typedef mp::sync<std::vector<mp::shared_ptr<server_socket> > > sync_socket;
	typedef sync_socket::ref sync_ref;
public:
	server_transport(server_impl* svr, const address& addr);
	~server_transport();

	void close();

	static void next_accept_ex(mp::weak_ptr<SOCKET> wlsock, mp::weak_ptr<server_impl> wsvr, mp::weak_ptr<sync_socket> wsock);

	static void on_accept(impl::windows::unique_socket& fd, int err, weak_server wsvr, mp::weak_ptr<sync_socket> sock);

private:
	mp::shared_ptr<SOCKET> m_lsock;
	mp::weak_ptr<server_impl> m_svr;
	loop m_loop;

	mp::shared_ptr<sync_socket> m_sock;

private:
	server_transport();
	server_transport(const server_transport&);
};


server_socket::server_socket(SOCKET sock, shared_server svr) :
	stream_handler<server_socket>(sock, svr->get_loop()),
	m_svr(svr) { }

server_socket::server_socket(shared_server svr) :
	stream_handler<server_socket>(::socket(AF_INET, SOCK_STREAM, 0), svr->get_loop()),
	m_svr(svr) { }

server_socket::~server_socket() { }

void server_socket::on_request(
		msgid_t msgid,
		object method, object params, auto_zone z)
{
	shared_server svr = m_svr.lock();
	if(!svr) {
		throw closed_exception();
	}
	svr->on_request(get_response_sender(), msgid, method, params, z);
}

void server_socket::on_notify(
		object method, object params, auto_zone z)
{
	shared_server svr = m_svr.lock();
	if(!svr) {
		throw closed_exception();
	}
	svr->on_notify(method, params, z);
}

void server_socket::accept_ex(SOCKET lsock, mp::function<void ()> accept_callback)
{
	using namespace mp::placeholders;

	mp::weak_ptr<stream_handler> whandler(mp::static_pointer_cast<stream_handler>(shared_from_this()));
	m_pac.reserve_buffer(MSGPACK_RPC_STREAM_RESERVE_SIZE);
	m_loop->accept_ex(lsock, ident(), m_pac.buffer(), m_pac.buffer_capacity(), mp::bind(on_accept_ex, whandler, _1, _2, lsock, accept_callback));
}

void server_socket::on_accept_ex(mp::weak_ptr<stream_handler<server_socket> > whandler, DWORD error, DWORD transferred, SOCKET lsock, mp::function<void ()> callback)
{
	mp::shared_ptr<stream_handler<server_socket> > handler = whandler.lock();
	if(handler) {
		setsockopt(handler->fd(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char*>(&lsock), sizeof (lsock));
	}
	on_read2(whandler, error, transferred);
	callback();
}


server_transport::server_transport(server_impl* svr, const address& addr) :
	m_lsock(), m_svr(mp::static_pointer_cast<server_impl>(svr->shared_from_this())), m_loop(svr->get_loop()), m_sock(mp::make_shared<sync_socket>())
{
	sockaddr_storage addrbuf;
	addr.get_addr((sockaddr*)&addrbuf);
#if 0
	m_lsock = m_loop->listen_accept(
			PF_INET, SOCK_STREAM, 0,
			(sockaddr*)&addrbuf, addr.get_addrlen(),
			mp::bind(
				&server_transport::on_accept,
				_1, _2,
				weak_server(mp::static_pointer_cast<server_impl>(svr->shared_from_this())),
				mp::weak_ptr<sync_socket>(m_sock)
				));
#else
	m_lsock = m_loop->listen(
			PF_INET, SOCK_STREAM, 0,
			(sockaddr*)&addrbuf, addr.get_addrlen());

	next_accept_ex(m_lsock, m_svr, m_sock);
#endif
}

void server_transport::next_accept_ex(mp::weak_ptr<SOCKET> wlsock, mp::weak_ptr<server_impl> wsvr, mp::weak_ptr<sync_socket> wsock)
{
	mp::shared_ptr<SOCKET> lsock = wlsock.lock();
	mp::shared_ptr<server_impl> svr = wsvr.lock();
	mp::shared_ptr<sync_socket> sock = wsock.lock();
	if(lsock && svr && sock) {
		mp::shared_ptr<server_socket> accept_sock = mp::make_shared<server_socket>(svr);
		accept_sock->accept_ex(*lsock, mp::bind(&server_transport::next_accept_ex, wlsock, wsvr, wsock));
		sock->lock()->push_back(accept_sock);
	}
}

server_transport::~server_transport()
{
	close();
}

void server_transport::close()
{
	m_lsock.reset();
	m_sock->lock()->clear();
}

void server_transport::on_accept(impl::windows::unique_socket& fd, int err, weak_server wsvr, mp::weak_ptr<sync_socket> wsock)
{
	shared_server svr = wsvr.lock();
	if(!svr) {
		return;
	}

	mp::shared_ptr<sync_socket> sock = wsock.lock();
	if(!sock) {
		return;
	}

	//// FIXME
	//if(fd == INVALID_SOCKET) {
	//	LOG_DEBUG("accept failed");
	//	return;
	//}
	LOG_TRACE("accepted fd=",fd.get());

	mp::shared_ptr<server_socket> accepted_sock = mp::make_shared<server_socket>(fd.release(), svr);
	accepted_sock->async_read();
	sock->lock()->push_back(accepted_sock);
}


}  // noname namespace

}  // namespace tcp
}  // namespace transport


tcp_builder::tcp_builder() :
	m_connect_timeout(10.0),  // FIXME default connect timeout
	m_reconnect_limit(3)      // FIXME default connect reconnect limit
{ }

tcp_builder::~tcp_builder() { }

std::auto_ptr<client_transport> tcp_builder::build(session_impl* s, const address& addr) const
{
	return std::auto_ptr<client_transport>(
			new transport::tcp::client_transport(s, addr, *this));
}


tcp_listener::tcp_listener(const std::string& host, uint16_t port) :
	m_addr(ip_address(host, port)) { }

tcp_listener::tcp_listener(const address& addr) :
	m_addr(addr) { }

tcp_listener::~tcp_listener() { }

std::auto_ptr<server_transport> tcp_listener::listen(server_impl* svr) const
{
	return std::auto_ptr<server_transport>(
			new transport::tcp::server_transport(svr, m_addr));
}


}  // namespace rpc
}  // namespace msgpack

