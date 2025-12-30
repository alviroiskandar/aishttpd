// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#include <aishttpd/httpd.hpp>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <signal.h>

using namespace aishttpd;

static void set_freezing_night_router(httpd *h)
{
	auto r = std::make_shared<http_router>("www.freezing-night.com");
	r->addRoute(AIS_HTTP_GET, "/", [](httpd *h, http_req *r) -> int {
		r->showHTMLFile(h, r, "index.html");
		return 0;
	});
	h->setDefaultRouter(r);
	h->addRouter(r);
}

static int setup_signal_handler(httpd *h);

int main(void)
{
	httpd h;

	setup_signal_handler(&h);
	set_freezing_night_router(&h);

	h.setBindAddr("::", 9980);
	h.setNrWorkers(4);
	printf("Starting HTTP server at [::]:9980...\n");
	h.start();
	return 0;
}

static httpd *g_h = nullptr;

static void handle_signal(int sig)
{
	if (g_h) {
		sig = write(STDOUT_FILENO, "\nStopping HTTP server...\n", 25);
		g_h->stop();
		g_h = nullptr;
	}

	(void)sig;
}

static int setup_signal_handler(httpd *h)
{
	struct sigaction sa;
	int r = 0;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;

	g_h = h;
	r |= sigaction(SIGINT, &sa, NULL);
	r |= sigaction(SIGTERM, &sa, NULL);
	r |= sigaction(SIGHUP, &sa, NULL);
	sa.sa_handler = SIG_IGN;
	r |= sigaction(SIGPIPE, &sa, NULL);
	return r ? -errno : 0;
}
