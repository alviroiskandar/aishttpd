// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef FRAMEWORK__AISHTTPD__AISHTTPD_HPP
#define FRAMEWORK__AISHTTPD__AISHTTPD_HPP

#include <libaishttpd/http.h>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>

#include "HttpReq.hpp"
#include "HttpRoute.hpp"
#include "HttpRouter.hpp"

namespace aishttpd {

enum {
	AIS_HTTP_UNKNOWN = GWNET_HTTP_METHOD_UNKNOWN,
	AIS_HTTP_GET     = GWNET_HTTP_METHOD_GET,
	AIS_HTTP_POST    = GWNET_HTTP_METHOD_POST,
	AIS_HTTP_PUT     = GWNET_HTTP_METHOD_PUT,
	AIS_HTTP_DELETE  = GWNET_HTTP_METHOD_DELETE,
	AIS_HTTP_HEAD    = GWNET_HTTP_METHOD_HEAD,
	AIS_HTTP_OPTIONS = GWNET_HTTP_METHOD_OPTIONS,
	AIS_HTTP_PATCH   = GWNET_HTTP_METHOD_PATCH,
	AIS_HTTP_TRACE   = GWNET_HTTP_METHOD_TRACE,
	AIS_HTTP_CONNECT = GWNET_HTTP_METHOD_CONNECT,
	AIS_HTTP_MAX     = AIS_HTTP_CONNECT + 1,
};

class Httpd {
private:
	struct ais_http_ctx http_ctx_;
	std::unique_ptr<struct ais_http_srv_iarg> iarg_;
	std::unordered_map<std::string, std::shared_ptr<HttpRouter>> routers_;
	std::shared_ptr<HttpRouter> default_router_ = nullptr;

	static int HttpdAcceptCb(struct ais_http_req *req, void *arg);
	static int HttpdRouteCb(struct ais_http_req *req);
	static int InvokeDefaultRouter(Httpd *h, aishttpd::HttpReq *r);

public:
	Httpd(void);
	~Httpd(void);

	inline void setBindAddr(const char *addr, uint16_t port = 0)
	{
		iarg_->tcp.bind_addr = addr;
		if (port != 0)
			iarg_->tcp.port = port;
	}

	inline void setPort(uint16_t port)
	{
		iarg_->tcp.port = port;
	}

	inline void setNrWorkers(uint32_t nr_workers)
	{
		iarg_->nr_workers = nr_workers;
	}

	inline void setSockBacklog(int sock_backlog)
	{
		iarg_->tcp.sock_backlog = sock_backlog;
	}

	inline void setEpollNevents(size_t epoll_nevents)
	{
		iarg_->tcp.epoll_nevents = epoll_nevents;
	}

	inline void setDefaultRouter(std::shared_ptr<HttpRouter> router)
	{
		default_router_ = router;
	}

	inline void addRouter(std::shared_ptr<HttpRouter> router)
	{
		routers_.emplace(router->get_host(), router);
		if (!default_router_)
			setDefaultRouter(router);
	}

	void start(void);
	void stop(void);

	friend class HttpReq;
};

} /* namespace aishttpd */

#endif /* #ifndef FRAMEWORK__AISHTTPD__AISHTTPD_HPP */
