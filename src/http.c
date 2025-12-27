// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#include "http.h"
#include "tcp.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

/*
 * TODO(viro_ssfs):
 *   - Make this file more modular.
 *   - Implement proper HTTP request parsing.
 *   - Implement HTTP routing and handling different methods.
 *   - and many more...
 *
 */

static void store_str_ip(struct ais_sock_addr *addr, char *buf, size_t buf_len)
{
	size_t len;

	if (addr->sa.sa_family == AF_INET) {
		inet_ntop(AF_INET, &addr->in.sin_addr, buf, buf_len);
		len = strlen(buf);
		snprintf(buf + len, buf_len - len, ":%hu", ntohs(addr->in.sin_port));
	} else if (addr->sa.sa_family == AF_INET6) {
		inet_ntop(AF_INET6, &addr->in6.sin6_addr, buf, buf_len);
		len = strlen(buf);
		snprintf(buf + len, buf_len - len, ":%hu", ntohs(addr->in6.sin6_port));
	} else {
		snprintf(buf, buf_len, "unknown");
	}
}

const char *ais_http_get_method_name(int m)
{
	switch (m) {
	case GWNET_HTTP_METHOD_GET:
		return "GET";
	case GWNET_HTTP_METHOD_POST:
		return "POST";
	case GWNET_HTTP_METHOD_PUT:
		return "PUT";
	case GWNET_HTTP_METHOD_DELETE:
		return "DELETE";
	case GWNET_HTTP_METHOD_HEAD:
		return "HEAD";
	case GWNET_HTTP_METHOD_OPTIONS:
		return "OPTIONS";
	case GWNET_HTTP_METHOD_PATCH:
		return "PATCH";
	case GWNET_HTTP_METHOD_TRACE:
		return "TRACE";
	case GWNET_HTTP_METHOD_CONNECT:
		return "CONNECT";
	default:
		return "UNKNOWN";
	}
}

static int handle_route_index(struct ais_sock_tcp_cli *cli)
{
	static const char res[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 24\r\n"
		"\r\n"
		"Welcome to HTTP server!\n";
	size_t len = sizeof(res) - 1;

	return ais_sock_buf_append_grow(&cli->tx_buf, res, len);
}

static int handle_route_hello(struct ais_sock_tcp_cli *cli)
{
	static const char res[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 13\r\n"
		"\r\n"
		"Hello, World!\n";
	size_t len = sizeof(res) - 1;

	return ais_sock_buf_append_grow(&cli->tx_buf, res, len);
}

static int handle_route_404(struct ais_sock_tcp_cli *cli)
{
	static const char res[] =
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 14\r\n"
		"\r\n"
		"404 Not Found\n";
	size_t len = sizeof(res) - 1;

	return ais_sock_buf_append_grow(&cli->tx_buf, res, len);
}

static int handle_route(struct ais_sock_tcp_cli *cli, struct ais_http_req *req)
{
	const char *uri = req->hdr_req.uri;

	printf("HTTP Request from %s: %s %s HTTP/1.%d\n",
		req->addr,
		ais_http_get_method_name(req->hdr_req.method),
		uri,
		req->hdr_req.version == GWNET_HTTP_VER_1_1 ? 1 : 0);

	if (!strcmp(uri, "/"))
		return handle_route_index(cli);

	if (!strcmp(uri, "/hello"))
		return handle_route_hello(cli);

	/* 404 Not Found */
	return handle_route_404(cli);
}

static ssize_t http_recv_callback(struct ais_sock_tcp_cli *cli)
{
	struct ais_http_req *req = cli->user_data;
	struct gwnet_http_hdr_pctx *pctx = &req->hdr_pctx;
	int r;

	pctx->off = 0;
	pctx->buf = cli->rx_buf.buf;
	pctx->len = cli->rx_buf.off;
	pctx->max_len = 4096; /* 4 KiB */
	r = gwnet_http_req_hdr_parse(pctx, &req->hdr_req);
	if (r == -EAGAIN) {
		/*
		 * Need more data...
		 */
		return 0;
	}

	if (r != 0)
		return r;

	/*
	 * Successfully parsed the HTTP request header.
	 * Now handle the route.
	 */
	r = handle_route(cli, req);
	if (r < 0)
		return r;

	return pctx->off;
}

static int http_send_callback(struct ais_sock_tcp_cli *cli, ssize_t sent_len)
{
	(void)sent_len;
	if (!cli->tx_buf.off)
		shutdown(cli->fd, SHUT_RDWR);

	return 0;
}

static void http_close_callback(struct ais_sock_tcp_cli *cli)
{
	struct ais_http_req *req = cli->user_data;
	gwnet_http_hdr_pctx_free(&req->hdr_pctx);
	gwnet_http_req_hdr_free(&req->hdr_req);
	free(req);
}

static int http_accept_callback(struct ais_sock_tcp_cli *cli, void *arg)
{
	struct ais_http_ctx *http_ctx = arg;
	struct ais_http_req *req;
	int r;

	req = calloc(1, sizeof(*req));
	if (!req)
		return -ENOMEM;

	cli->user_data = req;
	req->tcp_cli = cli;

	store_str_ip(&cli->addr, req->addr, sizeof(req->addr));
	ais_sock_tcp_cli_set_cb_rx(cli, &http_recv_callback);
	ais_sock_tcp_cli_set_cb_tx(cli, &http_send_callback);
	ais_sock_tcp_cli_set_cb_close(cli, &http_close_callback);

	if (http_ctx->cb_accept) {
		r = http_ctx->cb_accept(req, http_ctx->cb_accept_arg);
		if (r < 0) {
			free(req);
			return r;
		}
	}

	r = gwnet_http_hdr_pctx_init(&req->hdr_pctx);
	if (r < 0) {
		free(req);
		return r;
	}
	return 0;
}

int ais_http_ctx_init(struct ais_http_ctx *ctx, const struct ais_http_srv_iarg *iarg)
{
	int r;

	r = ais_sock_tcp_srv_init(&ctx->tcp_srv, &iarg->tcp);
	if (r < 0)
		return r;

	ais_sock_tcp_srv_set_cb_accept(&ctx->tcp_srv, &http_accept_callback);
	ais_sock_tcp_srv_set_cb_accept_arg(&ctx->tcp_srv, ctx);
	return 0;
}

int ais_http_ctx_run(struct ais_http_ctx *ctx)
{
	return ais_sock_tcp_srv_run(&ctx->tcp_srv);
}

void ais_http_ctx_free(struct ais_http_ctx *ctx)
{
	ais_sock_tcp_srv_free(&ctx->tcp_srv);
}

void ais_http_ctx_stop(struct ais_http_ctx *ctx)
{
	ais_sock_tcp_srv_stop(&ctx->tcp_srv);
}
