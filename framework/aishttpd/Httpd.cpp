// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#include "Httpd.hpp"
#include <stdexcept>

namespace aishttpd {

Httpd::Httpd(void)
{
	iarg_ = std::make_unique<struct ais_http_srv_iarg>();
	iarg_->tcp.bind_addr = "::";
	iarg_->tcp.port = 8890;
	iarg_->tcp.sock_backlog = 4096;
	iarg_->tcp.epoll_nevents = 64;
	iarg_->tcp.max_clients = 65536;
	iarg_->nr_workers = 4;
}

Httpd::~Httpd(void)
{
	ais_http_ctx_stop(&http_ctx_);
	ais_http_ctx_free(&http_ctx_);
}

int Httpd::InvokeDefaultRouter(Httpd *h, Req *hr)
{
	if (h->default_router_ == nullptr) {
		/*
		 * TODO(viro_ssfs): Handle 404 Not Found.
		 */
		return -ENOENT;
	}

	struct gwnet_http_req_hdr *hdr = &hr->get_req()->hdr;
	const char *path = hdr->path ? hdr->path : "/";
	return h->default_router_->invoke(hdr->method, path, h, hr);
}

// static
int Httpd::HttpdRouteCb(struct ais_http_req *req)
{
	Httpd *h = static_cast<Httpd *>(req->user_data);
	struct gwnet_http_req_hdr *hdr = &req->hdr;
	const char *host;
	Req hr(req);

	host = gwnet_http_hdr_fields_get(&hdr->fields, "host");
	if (!host)
		return InvokeDefaultRouter(h, &hr);

	auto it = h->routers_.find(host);
	if (it == h->routers_.end())
		return InvokeDefaultRouter(h, &hr);

	return it->second->invoke(hdr->method, hdr->path ? hdr->path : "/", h, &hr);
}

// static
int Httpd::HttpdAcceptCb(struct ais_http_req *req, void *arg)
{
	req->user_data = arg;
	ais_http_req_set_route_cb(req, &Httpd::HttpdRouteCb);
	return 0;
}

void Httpd::start(void)
{
	int r;

	r = ais_http_ctx_init(&http_ctx_, iarg_.get());
	if (r != 0)
		throw std::runtime_error("Failed to initialize HTTP context");

	// Discard iarg_ after initializing the context.
	iarg_.reset();

	ais_http_req_set_accept_cb(&http_ctx_, &Httpd::HttpdAcceptCb);
	ais_http_req_set_accept_cb_arg(&http_ctx_, this);

	r = ais_http_ctx_run(&http_ctx_);
	if (r != 0)
		throw std::runtime_error("Failed to start HTTP context");
}

void Httpd::stop(void)
{
	ais_http_ctx_stop(&http_ctx_);
}

} /* namespace aishttpd */
