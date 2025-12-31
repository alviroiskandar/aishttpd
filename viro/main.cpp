// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#include <aishttpd/Httpd.hpp>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include "signal.hpp"

using namespace aishttpd;

static const char *gen_date_buf(char *buf)
{
	time_t now = time(NULL);
	struct tm tm;

	gmtime_r(&now, &tm);
	strftime(buf, 128, "%a, %d %b %Y %H:%M:%S GMT", &tm);
	return buf;
}

static void setHttpRouters(Httpd *h)
{
	auto r = std::make_shared<Router>("www.freezing-night.com");

	r->addPreroute([](Httpd *h, Req *r) -> int {
		struct ais_http_req *req = r->get_req();
		struct gwnet_http_req_hdr *hdr = &req->hdr;
		const char *path = hdr->uri;
		const char *cf_ip = gwnet_http_hdr_fields_get(&hdr->fields, "cf-connecting-ip");
		char date_buf[128];
		gen_date_buf(date_buf);

		if (!cf_ip)
			cf_ip = "none";

		printf("date=%s|ip=%s|cf_ip=%s|uri=%s|qs=%s\n",
			date_buf,
			req->addr,
			cf_ip,
			hdr->uri,
			hdr->qs ? hdr->qs : "");

		if (!path || strlen(path) < 2)
			return PREROUTE_SKIP;
		if (path[0] != '/')
			return PREROUTE_SKIP;
		if (strstr(path, "..") != nullptr)
			return PREROUTE_SKIP;

		std::string file_path = "./public/";
		file_path += (path + 1);
		r->showFile(h, r, file_path);
		return PREROUTE_MATCH;
	});

	r->addRoute(AIS_HTTP_GET, "/", [](Httpd *h, Req *r) -> int {
		r->showFile(h, r, "public/index.html");
		return 0;
	});

	h->setDefaultRouter(r);
	h->addRouter(r);
}

int main(void)
{
	try {
		Httpd h;

		setupSignalHandler(&h);
		setHttpRouters(&h);

		h.setBindAddr("::");
		h.setPort(9980);
		h.setNrWorkers(4);

		printf("Starting HTTP server at [::]:9980...\n");
		h.start();
		printf("HTTP server stopped.\n");
		return 0;
	} catch (const std::exception &e) {
		fprintf(stderr, "Error: %s\n", e.what());
		return 1;
	}
}
