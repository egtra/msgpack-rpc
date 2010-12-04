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
#include <concurrent_vector.h>

namespace msgpack {
namespace rpc {
namespace transport {
namespace tcp {

namespace {

using namespace mp::placeholders;

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
	typedef std::vector<client_socket*> sockpool_t;

	struct sync_t {
		sync_t() : sockpool_rr(0), connecting(0) { }
		sockpool_t sockpool;
		size_t sockpool_rr;
		unsigned int connecting;
		mp::wavy::xfer pending_xf;
	};

	typedef mp::sync<sync_t>::ref sync_ref;
	mp::sync<sync_t> m_sync;

	session_impl* m_session;

	double m_connect_timeout;
	unsigned int m_reconnect_limit;

private:
	void try_connect(sync_ref& lk_ref);
	static void on_connect(SOCKET fd, int err, weak_session ws, client_transport* self);
	void on_connect_success(SOCKET fd, sync_ref& ref);
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

		for(sockpool_t::iterator it(sockpool.begin()), it_end(sockpool.end());
				it != it_end; ++it) {
			(*it)->remove_handler();
		}
	}
}

inline void client_transport::on_connect_success(SOCKET fd, sync_ref& ref)
{
	LOG_DEBUG("connect success to ",m_session->get_address()," fd=",fd);

	mp::shared_ptr<client_socket> cs(new client_socket(fd, this, m_session));
	cs->async_read();

	ref->sockpool.push_back(cs.get());

	m_session->get_loop()->commit(fd, &ref->pending_xf);
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

void client_transport::on_connect(SOCKET fd, int err, weak_session ws, client_transport* self)
{
	shared_session s = ws.lock();
	if(!s) {
		if(fd != INVALID_SOCKET) {
			::closesocket(fd);
		}
		return;
	}

	sync_ref ref(self->m_sync);

	if(fd != INVALID_SOCKET) {
		// success
		try {
			self->on_connect_success(fd, ref);
			return;
		} catch (...) {
			::closesocket(fd);
			LOG_WARN("attach failed or send pending failed");
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
	sockpool_t::iterator found = std::find(
			ref->sockpool.begin(), ref->sockpool.end(), sock);
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
		client_socket* sock = ref->sockpool[0];
		sock->send_data(sbuf);
	}
}

void client_transport::send_data(auto_vreflife vbuf)
{abort();
//	sync_ref ref(m_sync);
//	if(ref->sockpool.empty()) {
//		if(ref->connecting == 0) {
//			try_connect(ref);
//			ref->connecting = 1;
//		}
//		ref->pending_xf.push_writev(vbuf->vector(), vbuf->vector_size());
//		ref->pending_xf.push_finalize(vbuf);
//	} else {
//		// FIXME pesudo connecting load balance
//		client_socket* sock = ref->sockpool[0];
//		sock->send_data(vbuf);
//	}
}


class server_socket : public stream_handler<server_socket> {
public:
	server_socket(int sock, shared_server svr);
	~server_socket();

	void on_request(
			msgid_t msgid,
			object method, object params, auto_zone z);

	void on_notify(
			object method, object params, auto_zone z);

private:
	weak_server m_svr;

private:
	server_socket();
	server_socket(const server_socket&);
};


class server_transport : public rpc::server_transport {
public:
	server_transport(server_impl* svr, const address& addr);
	~server_transport();

	void close();

	static void on_accept(SOCKET fd, int err, weak_server wsvr, mp::weak_ptr<Concurrency::concurrent_vector<mp::shared_ptr<server_socket> > > sock);

private:
	mp::shared_ptr<SOCKET> m_lsock;
	loop m_loop;
	mp::shared_ptr<Concurrency::concurrent_vector<mp::shared_ptr<server_socket> > > m_sock;

private:
	server_transport();
	server_transport(const server_transport&);
};


server_socket::server_socket(int sock, shared_server svr) :
	stream_handler<server_socket>(sock, svr->get_loop()),
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


server_transport::server_transport(server_impl* svr, const address& addr) :
	m_lsock(), m_loop(svr->get_loop()), m_sock(std::make_shared<Concurrency::concurrent_vector<mp::shared_ptr<server_socket> > >())
{
	sockaddr_storage addrbuf;
	addr.get_addr((sockaddr*)&addrbuf);

	m_lsock = m_loop->listen(
			PF_INET, SOCK_STREAM, 0,
			(sockaddr*)&addrbuf, addr.get_addrlen(),
			mp::bind(
				&server_transport::on_accept,
				_1, _2,
				weak_server(mp::static_pointer_cast<server_impl>(svr->shared_from_this())),
				mp::weak_ptr<Concurrency::concurrent_vector<mp::shared_ptr<server_socket> > >(m_sock)
				));
}

server_transport::~server_transport()
{
	close();
}

void server_transport::close()
{
	m_lsock.reset();
	m_sock.reset();
}

void server_transport::on_accept(SOCKET fd, int err, weak_server wsvr, mp::weak_ptr<Concurrency::concurrent_vector<mp::shared_ptr<server_socket> > > wsock)
{
	shared_server svr = wsvr.lock();
	if(!svr) {
		return;
	}

	mp::shared_ptr<Concurrency::concurrent_vector<mp::shared_ptr<server_socket> > > sock = wsock.lock();
	if(!sock) {
		return;
	}

	// FIXME
	if(fd == INVALID_SOCKET) {
		LOG_DEBUG("accept failed");
		return;
	}
	LOG_TRACE("accepted fd=",fd);

	try {
		mp::shared_ptr<server_socket> accepted_sock = std::make_shared<server_socket>(fd, svr);
		accepted_sock->async_read();
		sock->push_back(accepted_sock);
	} catch (...) {
		::closesocket(fd);
		throw;
	}
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

