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

static int setup_signal_handler(struct ais_http_ctx *ctx)
{
	struct sigaction sa = { .sa_handler = handle_signal };
	int r = 0;

	g_http_ctx = ctx;
	r |= sigaction(SIGINT, &sa, NULL);
	r |= sigaction(SIGTERM, &sa, NULL);
	r |= sigaction(SIGHUP, &sa, NULL);
	sa.sa_handler = SIG_IGN;
	r |= sigaction(SIGPIPE, &sa, NULL);
	return r;
}

static int route_cb(struct ais_http_req *req)
{
	int r = 0;
	ais_http_res_set_code(&req->res, 200);
	r |= ais_http_res_add_hdr(&req->res, "Content-Type", "text/plain");
	r |= ais_http_res_body_set_buf(&req->res, "Hello, World!\n");
	return r;
}

static int accept_cb(struct ais_http_req *req, void *)
{
	ais_http_req_set_route_cb(req, &route_cb);
	return 0;
}

int main(void)
{
	static const struct ais_http_srv_iarg iarg = {
		.tcp = {
			.bind_addr = "::",
			.port = 9980,
			.sock_backlog = 128,
			.epoll_nevents = 64,
			.max_clients = 50000,
		},
		.nr_workers = 4,
	};
	struct ais_http_ctx http_ctx;
	int r;

	r = ais_http_ctx_init(&http_ctx, &iarg);
	if (r < 0) {
		fprintf(stderr, "Failed to initialize HTTP context: %d\n", r);
		return -r;
	}

	r = setup_signal_handler(&http_ctx);
	if (r < 0) {
		fprintf(stderr, "Failed to setup signal handler: %d\n", r);
		ais_http_ctx_free(&http_ctx);
		return -r;
	}

	ais_http_req_set_accept_cb(&http_ctx, &accept_cb);
	printf("Starting HTTP server on [%s]:%hu...\n", iarg.tcp.bind_addr, iarg.tcp.port);
	r = ais_http_ctx_run(&http_ctx);
	if (r < 0)
		fprintf(stderr, "Failed to run HTTP server: %d\n", r);

	printf("Shutting down HTTP server...\n");
	ais_http_ctx_free(&http_ctx);
	return -r;
}
