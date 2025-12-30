// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef FRAMEWORK__AISHTTPD__HTTPROUTE_HPP
#define FRAMEWORK__AISHTTPD__HTTPROUTE_HPP

#include <functional>
#include <string>

namespace aishttpd {

class Httpd;
class HttpReq;

class HttpRoute {
private:
	std::function<int(Httpd *, HttpReq *)> cb_;

	inline int invoke(Httpd *h, HttpReq *r)
	{
		return cb_(h, r);
	}

public:
	inline HttpRoute(std::function<int(Httpd *, HttpReq *)> cb):
		cb_(std::move(cb))
	{
	}

	HttpRoute(void) = default;

	friend class Httpd;
	friend class HttpRouter;
};

} /* namespace aishttpd */

#endif /* #ifndef FRAMEWORK__AISHTTPD__HTTPROUTE_HPP */
