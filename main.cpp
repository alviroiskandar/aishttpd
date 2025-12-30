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

static void setHttpRouters(Httpd *h)
{
	auto r = std::make_shared<Router>("www.freezing-night.com");

	r->addPreroute([](Httpd *h, Req *r) -> int {
		const char *path = r->get_req()->hdr.uri;
		std::string file_path = "./public/";

		if (!path || strlen(path) == 0)
			return PREROUTE_SKIP;
		if (path[0] != '/')
			return PREROUTE_SKIP;
		if (strstr(path, "..") != nullptr)
			return PREROUTE_SKIP;

		file_path += path + 1;
		r->showFile(h, r, file_path);
		return PREROUTE_MATCH;
	});

	r->addRoute(AIS_HTTP_GET, "/", [](Httpd *h, Req *r) -> int {
		r->showFile(h, r, "index.html");
		return 0;
	});
	r->addRoute(AIS_HTTP_GET, "/index", [](Httpd *h, Req *r) -> int {
		r->redirect(h, r, "/");
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
