// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef FRAMEWORK__AISHTTPD__ROUTER_HPP
#define FRAMEWORK__AISHTTPD__ROUTER_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

enum {
	PREROUTE_MATCH = 0,
	PREROUTE_SKIP  = 1,
};

namespace aishttpd {

class Router {
private:
	std::string		host_;
	std::vector<std::function<int(Httpd *, Req *)>> preroutes_;
	std::unordered_map<std::string, std::vector<Route>> routes_;

	int invoke(int method, const std::string &path, Httpd *h, Req *r);

public:
	inline Router(const std::string &host):
		host_(host)
	{
	}

	~Router(void) = default;

	inline const std::string &get_host(void) const
	{
		return host_;
	}

	void addPreroute(std::function<int(Httpd *, Req *)> cb);
	void addRoute(int method, const std::string &path, std::function<int(Httpd *, Req *)> cb);

	friend class Httpd;
};

} /* namespace aishttpd */

#endif /* #ifndef FRAMEWORK__AISHTTPD__ROUTER_HPP */
