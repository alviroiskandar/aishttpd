// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#include "Httpd.hpp"
#include <stdexcept>

namespace aishttpd {

void Router::addRoute(int method, const std::string &path,
			   std::function<int(Httpd *, Req *)> cb)
{
	auto it = routes_.find(path);
	if (it == routes_.end()) {
		std::vector<Route> v(AIS_HTTP_MAX);
		v[method] = Route(std::move(cb));
		routes_.emplace(path, v);
	} else {
		it->second[method] = Route(std::move(cb));
	}
}

int Router::invoke(int method, const std::string &path, Httpd *h, Req *r)
{
	struct ais_http_req *req = r->get_req();
	struct ais_http_res *res = &req->res;

	auto it = routes_.find(path);
	if (it == routes_.end()) {
		int q = 0;
		ais_http_res_set_code(res, 404);
		q |= ais_http_res_add_hdr(res, "Content-Type", "text/plain; charset=utf-8");
		q |= ais_http_res_body_set_bufl(res, "404 Not Found!\n", 15);
		return q;
	}

	auto &rv = it->second;
	if (rv.size() <= (size_t)method || !rv[method].cb_) {
		int q = 0;
		ais_http_res_set_code(res, 405);
		q |= ais_http_res_add_hdr(res, "Content-Type", "text/plain; charset=utf-8");
		q |= ais_http_res_body_set_bufl(res, "405 Method Not Allowed!\n", 24);
		return q;
	}

	return rv[method].invoke(h, r);
}

} /* namespace aishttpd */
