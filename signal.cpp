// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#include <aishttpd/Httpd.hpp>
#include "signal.hpp"
#include <cstring>
#include <signal.h>

using namespace aishttpd;

static Httpd *g_h = nullptr;

static void handle_signal(int sig)
{
	if (g_h) {
		sig = write(STDOUT_FILENO, "\n", 1);
		g_h->stop();
		g_h = nullptr;
	}

	// To avoid unused variable warning.
	(void)sig;
}

int setupSignalHandler(Httpd *h)
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
