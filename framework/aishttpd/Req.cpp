// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#include "Httpd.hpp"
#include <stdexcept>
#include <cstring>
#include <sys/stat.h>
#include <cerrno>

namespace aishttpd {

void Req::showFile(Httpd *h, Req *hr, const std::string &file_path)
{
	struct ais_file_table *ftb = &h->http_ctx_.file_table;
	struct ais_http_req *req = hr->get_req();
	struct ais_http_res *res = &req->res;
	struct stat st;
	char ext[32];
	int r = 0;

	r = ais_http_res_body_set_file_path(res, ftb, file_path.c_str());
	if (r) {
		switch (r) {
		case -ENOENT:
			abort(404, h, hr);
			return;
		default:
			abort(500, h, hr);
			return;
		}
	}

	std::memset(ext, 0, sizeof(ext));
	const char *dot = strrchr(file_path.c_str(), '.');
	if (dot && strlen(dot) < sizeof(ext))
		std::strncpy(ext, dot + 1, sizeof(ext) - 1);

	r = ais_http_res_add_hdr(res, "Content-Type", ais_http_get_mime_type(ext));
	if (r)
		throw std::bad_alloc();

	if (fstat(res->body.file->fd, &st) == 0) {
		char date[64];
		struct tm tm;
		gmtime_r(&st.st_mtim.tv_sec, &tm);
		strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", &tm);
		r = ais_http_res_add_hdr(res, "Last-Modified", date);
		if (r)
			throw std::bad_alloc();
	}
}

void Req::redirect(Httpd *h, Req *hr, const std::string &url)
{
	struct ais_http_req *req = hr->get_req();
	struct ais_http_res *res = &req->res;
	int r = 0;

	ais_http_res_set_code(res, 302);
	r = ais_http_res_add_hdr(res, "Location", url.c_str());
	if (r)
		throw std::bad_alloc();

	r = ais_http_res_body_set_bufl(res, "Redirecting...\n", 15);
	if (r)
		throw std::bad_alloc();

	(void)h; // currently unused
}

void Req::abort(uint16_t code, Httpd *h, Req *hr)
{
	const char *reason = ais_http_translate_code(code);
	struct ais_http_req *req = hr->get_req();
	struct ais_http_res *res = &req->res;
	int r = 0;

	ais_http_res_set_code(res, code);
	r = ais_http_res_add_hdr(res, "Content-Type", "text/plain");
	if (r)
		throw std::bad_alloc();

	std::string body = std::to_string(code) + " " + reason + "\n";
	r = ais_http_res_body_set_bufl(res, body.c_str(), body.length());
	if (r)
		throw std::bad_alloc();

	(void)h; // currently unused
}

} /* namespace aishttpd */
