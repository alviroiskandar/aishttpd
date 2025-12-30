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
	auto it = routes_.find(path);
	if (it == routes_.end()) {
		/*
		 * TODO(viro_ssfs): Handle 404 Not Found.
		 */
		return -ENOENT;
	}

	auto &rv = it->second;
	if (rv.size() <= (size_t)method || !rv[method].cb_) {
		/*
		 * TODO(viro_ssfs): Handle 405 Method Not Allowed.
		 */
		return -ENOSYS;
	}

	return rv[method].invoke(h, r);
}

} /* namespace aishttpd */
