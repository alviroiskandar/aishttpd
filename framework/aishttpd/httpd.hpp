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

class httpd;

class http_req {
private:
	struct ais_http_req *req_;

public:
	inline http_req(struct ais_http_req *r):
		req_(r)
	{
	}

	~http_req(void) = default;

	inline struct ais_http_req *get_req(void)
	{
		return req_;
	}

	void showHTMLFile(httpd *h, http_req *hr, const std::string &file_path);

	friend class httpd;
};

class http_route {
private:
	std::function<int(httpd *, http_req *)> cb_;

	inline int invoke(httpd *h, http_req *r)
	{
		return cb_(h, r);
	}

public:
	inline http_route(std::function<int(httpd *, http_req *)> cb):
		cb_(std::move(cb))
	{
	}

	http_route(void) = default;

	friend class httpd;
	friend class http_router;
};

class http_router {
private:
	std::string	host_;
	std::unordered_map<std::string, std::vector<http_route>> routes_;

	int invoke(int method, const std::string &path, httpd *h, http_req *r);

public:
	inline http_router(const std::string &host):
		host_(host)
	{
	}

	~http_router(void) = default;

	inline const std::string &get_host(void) const
	{
		return host_;
	}

	void addRoute(int method, const std::string &path, std::function<int(httpd *, http_req *)> cb);

	friend class httpd;
};

class httpd {
private:
	struct ais_http_ctx http_ctx_;
	std::unique_ptr<struct ais_http_srv_iarg> iarg_;
	std::unordered_map<std::string, std::shared_ptr<http_router>> routers_;
	std::shared_ptr<http_router> default_router_ = nullptr;

	static int httpd_accept_cb(struct ais_http_req *req, void *arg);
	static int httpd_route_cb(struct ais_http_req *req);
	static int invoke_default_router(httpd *h, aishttpd::http_req *r);

public:
	httpd(void);
	~httpd(void);

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

	inline void setDefaultRouter(std::shared_ptr<http_router> router)
	{
		default_router_ = router;
	}

	inline void addRouter(std::shared_ptr<http_router> router)
	{
		routers_.emplace(router->get_host(), router);
		if (!default_router_)
			setDefaultRouter(router);
	}

	void start(void);
	void stop(void);

	friend class http_req;
};

} /* namespace aishttpd */

#endif /* #ifndef FRAMEWORK__AISHTTPD__AISHTTPD_HPP */
