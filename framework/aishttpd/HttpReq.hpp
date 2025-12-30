// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef FRAMEWORK__AISHTTPD__HTTPREQ_HPP
#define FRAMEWORK__AISHTTPD__HTTPREQ_HPP

#include <libaishttpd/http.h>

namespace aishttpd {

class Httpd;

class HttpReq {
private:
	struct ais_http_req *req_;

public:
	inline HttpReq(struct ais_http_req *r):
		req_(r)
	{
	}

	~HttpReq(void) = default;

	inline struct ais_http_req *get_req(void)
	{
		return req_;
	}

	void showHTMLFile(Httpd *h, HttpReq *hr, const std::string &file_path);

	friend class Httpd;
};

} /* namespace aishttpd */

#endif /* #ifndef FRAMEWORK__AISHTTPD__HTTPREQ_HPP */
