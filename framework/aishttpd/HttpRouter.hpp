// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef FRAMEWORK__AISHTTPD__HTTPROUTER_HPP
#define FRAMEWORK__AISHTTPD__HTTPROUTER_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace aishttpd {

class HttpRouter {
private:
	std::string	host_;
	std::unordered_map<std::string, std::vector<HttpRoute>> routes_;

	int invoke(int method, const std::string &path, Httpd *h, HttpReq *r);

public:
	inline HttpRouter(const std::string &host):
		host_(host)
	{
	}

	~HttpRouter(void) = default;

	inline const std::string &get_host(void) const
	{
		return host_;
	}

	void addRoute(int method, const std::string &path, std::function<int(Httpd *, HttpReq *)> cb);

	friend class Httpd;
};

} /* namespace aishttpd */

#endif /* #ifndef FRAMEWORK__AISHTTPD__HTTPROUTER_HPP */
