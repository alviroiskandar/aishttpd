// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#include "http.h"
#include <stdio.h>
#include <signal.h>

static struct ais_http_ctx *g_http_ctx;

static void handle_signal(int sig)
{
	struct ais_http_ctx *ghc = g_http_ctx;

	if (ghc) {
		g_http_ctx = NULL;
		ais_http_ctx_stop(ghc);
		sig = write(STDOUT_FILENO, "\n", 1);
	}

	(void)sig;
}

int main(void)
{
	static const struct ais_sock_tcp_srv_iarg iarg = {
		.bind_addr = "::",
		.port = 9980,
		.sock_backlog = 128,
		.epoll_nevents = 64,
		.max_clients = 1024,
	};
	struct ais_http_ctx http_ctx;
	struct sigaction sa = { .sa_handler = handle_signal };
	int r;

	r = ais_http_ctx_init(&http_ctx, &iarg);
	if (r < 0) {
		fprintf(stderr, "Failed to initialize HTTP context: %d\n", r);
		return -r;
	}

	g_http_ctx = &http_ctx;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);

	printf("Starting HTTP server on [%s]:%hu...\n", iarg.bind_addr, iarg.port);
	r = ais_http_ctx_run(&http_ctx);
	if (r < 0)
		fprintf(stderr, "Failed to run HTTP server: %d\n", r);

	printf("Shutting down HTTP server...\n");
	ais_http_ctx_free(&http_ctx);
	return -r;
}
