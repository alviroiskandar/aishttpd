// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#include "Httpd.hpp"
#include <stdexcept>

namespace aishttpd {

void HttpReq::showHTMLFile(Httpd *h, HttpReq *hr, const std::string &file_path)
{
	struct ais_file_table *ftb = &h->http_ctx_.file_table;
	ais_http_req *req = hr->get_req();
	struct ais_http_res *res = &req->res;
	int r = 0;

	r = ais_http_res_add_hdr(res, "Content-Type", "text/html; charset=UTF-8");
	if (r)
		throw std::bad_alloc();

	r = ais_http_res_body_set_file_path(res, ftb, file_path.c_str());
	if (r) {
		switch (r) {
		case -ENOENT:
			throw std::runtime_error("File not found: " + file_path);
		default:
			throw std::runtime_error("Failed to set file path: " + file_path);
		}
	}
}

} /* namespace aishttpd */
