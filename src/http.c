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
#include <assert.h>
#include <time.h>

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

static const char *translate_http_code(uint16_t code)
{
	switch (code) {
	case 100: return "Continue";
	case 101: return "Switching Protocols";
	case 102: return "Processing";
	case 103: return "Early Hints";
	case 200: return "OK";
	case 201: return "Created";
	case 202: return "Accepted";
	case 203: return "Non-Authoritative Information";
	case 204: return "No Content";
	case 205: return "Reset Content";
	case 206: return "Partial Content";
	case 207: return "Multi-Status";
	case 208: return "Already Reported";
	case 226: return "IM Used";
	case 300: return "Multiple Choices";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 303: return "See Other";
	case 304: return "Not Modified";
	case 305: return "Use Proxy";
	case 307: return "Temporary Redirect";
	case 308: return "Permanent Redirect";
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 402: return "Payment Required";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 406: return "Not Acceptable";
	case 407: return "Proxy Authentication Required";
	case 408: return "Request Timeout";
	case 409: return "Conflict";
	case 410: return "Gone";
	case 411: return "Length Required";
	case 412: return "Precondition Failed";
	case 413: return "Payload Too Large";
	case 414: return "URI Too Long";
	case 415: return "Unsupported Media Type";
	case 416: return "Range Not Satisfiable";
	case 417: return "Expectation Failed";
	case 418: return "I'm a teapot";
	case 421: return "Misdirected Request";
	case 422: return "Unprocessable Entity";
	case 423: return "Locked";
	case 424: return "Failed Dependency";
	case 425: return "Too Early";
	case 426: return "Upgrade Required";
	case 428: return "Precondition Required";
	case 429: return "Too Many Requests";
	case 431: return "Request Header Fields Too Large";
	case 451: return "Unavailable For Legal Reasons";
	case 500: return "Internal Server Error";
	case 501: return "Not Implemented";
	case 502: return "Bad Gateway";
	case 503: return "Service Unavailable";
	case 504: return "Gateway Timeout";
	case 505: return "HTTP Version Not Supported";
	case 506: return "Variant Also Negotiates";
	case 507: return "Insufficient Storage";
	case 508: return "Loop Detected";
	case 510: return "Not Extended";
	case 511: return "Network Authentication Required";
	default: return "Unknown";
	}
}

static void ais_http_res_free(struct ais_http_res *res)
{
	if (!res)
		return;

	gwnet_http_res_hdr_free(&res->hdr);
	if (res->hdr.reason)
		free(res->hdr.reason);
	memset(res, 0, sizeof(*res));
}

static int handle_req_state_init(struct ais_http_req *req)
{
	req->state = AIS_HREQ_STATE_RECV_HDR;
	return 0;
}

/*
 * Returns true if the connection should be kept alive, false otherwise.
 */
static bool check_keep_alive(struct ais_http_req *req)
{
	/*
	 * First, check the "Connection" header field.
	 */
	const char *conn;

	conn = gwnet_http_hdr_fields_get(&req->hdr.fields, "connection");
	if (conn)
		return (strcasecmp(conn, "keep-alive") == 0);

	/*
	 * If not present, check the HTTP version.
	 *   - HTTP/1.1 defaults to keep-alive.
	 *   - HTTP/1.0 defaults to close.
	 */
	return (req->hdr.version == GWNET_HTTP_VER_1_1);
}

static int handle_req_state_recv_hdr(struct ais_http_req *req)
{
	struct gwnet_http_hdr_pctx *pctx = &req->hdr_pctx;
	struct ais_sock_tcp_cli *cli = req->tcp_cli;
	int r;

	pctx->off = 0;
	pctx->buf = cli->rx_buf.buf;
	pctx->len = cli->rx_buf.len;
	pctx->max_len = 4096; /* 4 KiB */
	r = gwnet_http_req_hdr_parse(pctx, &req->hdr);
	if (r < 0)
		return r;

	ais_buf_soft_advance(&cli->rx_buf, pctx->off);
	req->keep_alive = check_keep_alive(req);
	req->state = AIS_HREQ_STATE_RECV_BODY;
	return 0;
}

static int invoke_route(struct ais_http_req *req)
{
	int r = 0;

	if (req->cb_route) {
		ais_http_res_set_code(&req->res, 200);
		r = req->cb_route(req);
		if (r < 0)
			return r;
	} else {
		ais_http_res_set_code(&req->res, 204);
		ais_http_res_add_hdr(&req->res, "Content-Length", "0");
	}

	return r;
}

static int handle_req_state_recv_body(struct ais_http_req *req)
{
	int r;

	r = invoke_route(req);
	if (r < 0)
		return r;

	req->state = AIS_HREQ_STATE_BUILD_RES;
	return r;
}

static const char *gen_date_buf(char *buf)
{
	time_t now = time(NULL);
	struct tm tm;

	gmtime_r(&now, &tm);
	strftime(buf, 128, "%a, %d %b %Y %H:%M:%S GMT", &tm);
	return buf;
}

static int set_content_length_hdr(struct ais_http_res *res, struct ais_buf *txb)
{
	switch (res->body.type) {
	case AIS_RES_BODY_TYPE_BUF:
		return ais_buf_apfmt(txb, "Content-Length: %zu\r\n", res->body.buf.len);
		break;
	default:
		return 0;
	}
}

static int construct_res_hdr(struct ais_http_req *req)
{
	struct ais_sock_tcp_cli *cli = req->tcp_cli;
	struct ais_http_res *res = &req->res;
	struct ais_buf *txb = &cli->tx_buf;
	char date_buf[128];
	const char *tmp;
	size_t i;
	int r;

	tmp = res->hdr.reason ? res->hdr.reason : translate_http_code(res->hdr.code);
	r = ais_buf_apfmt(txb,
			  "HTTP/%s %u %s\r\n"
			  "Server: aishttpd v0.0.1\r\n"
			  "Date: %s\r\n",
			  (req->hdr.version == GWNET_HTTP_VER_1_1) ? "1.1" : "1.0",
			  res->hdr.code, tmp, gen_date_buf(date_buf));
	if (r < 0)
		return r;

	tmp = req->keep_alive ? "keep-alive" : "close";
	r |= ais_buf_append(txb, "Connection: ", 12);
	r |= ais_buf_append(txb, tmp, strlen(tmp));
	r |= ais_buf_append(txb, "\r\n", 2);
	r |= set_content_length_hdr(res, txb);
	for (i = 0; i < res->hdr.fields.nr; i++) {
		struct gwnet_http_hdr_field *f = &res->hdr.fields.ff[i];
		r |= ais_buf_append(txb, f->key, strlen(f->key));
		r |= ais_buf_append(txb, ": ", 2);
		r |= ais_buf_append(txb, f->val, strlen(f->val));
		r |= ais_buf_append(txb, "\r\n", 2);
	}
	r |= ais_buf_apfmt(txb, "\r\n");

	return r ? -ENOMEM : 0;
}

static int construct_res_body(struct ais_http_req *req)
{
	struct ais_sock_tcp_cli *cli = req->tcp_cli;
	struct ais_http_res *res = &req->res;
	struct ais_http_res_body *b = &res->body;
	struct ais_buf *txb = &cli->tx_buf;
	int r = 0;

	switch (res->body.type) {
	case AIS_RES_BODY_TYPE_BUF:
		r = ais_buf_append(txb, b->buf.buf, b->buf.len);
		break;
	case AIS_RES_BODY_TYPE_NONE:
		break;
	default:
		r = -EINVAL;
		break;
	}

	ais_http_res_body_free(&res->body);
	return r;
}

static int handle_req_state_build_res(struct ais_http_req *req)
{
	int r;

	r = construct_res_hdr(req);
	if (r < 0)
		return r;

	r = construct_res_body(req);
	if (r < 0)
		return r;

	req->state = AIS_HREQ_STATE_SEND_RES;
	return 0;
}

static void reset_req(struct ais_http_req *req)
{
	gwnet_http_hdr_pctx_free(&req->hdr_pctx);
	gwnet_http_req_hdr_free(&req->hdr);
	ais_http_res_free(&req->res);
	memset(&req->hdr, 0, sizeof(req->hdr));
	memset(&req->hdr_pctx, 0, sizeof(req->hdr_pctx));
}

static int handle_req_state_send_res(struct ais_http_req *req)
{
	reset_req(req);
	req->state = AIS_HREQ_STATE_DONE;
	return 0;
}

static int __ais_http_handle_req(struct ais_http_req *req)
{
	int r = 0;

	switch (req->state) {
	case AIS_HREQ_STATE_DONE:
	case AIS_HREQ_STATE_INIT:
		r = handle_req_state_init(req);
		break;
	case AIS_HREQ_STATE_RECV_HDR:
		r = handle_req_state_recv_hdr(req);
		break;
	case AIS_HREQ_STATE_RECV_BODY:
		r = handle_req_state_recv_body(req);
		break;
	case AIS_HREQ_STATE_BUILD_RES:
		r = handle_req_state_build_res(req);
		break;
	case AIS_HREQ_STATE_SEND_RES:
		r = handle_req_state_send_res(req);
		break;
	default:
		assert(false && "Invalid HTTP request state");
		r = -EINVAL;
		break;
	}

	return r;
}

static int ais_http_handle_req(struct ais_http_req *req)
{
	int r;

	while (true) {
		r = __ais_http_handle_req(req);
		if (r)
			break;
	}

	return r;
}

static ssize_t http_recv_callback(struct ais_sock_tcp_cli *cli)
{
	struct ais_http_req *req = cli->user_data;
	int r;

	r = ais_http_handle_req(req);
	if (r < 0 && r != -EAGAIN)
		return r;

	ais_buf_soft_advance_sync(&cli->rx_buf);
	return r;
}

static int http_send_callback(struct ais_sock_tcp_cli *cli, ssize_t sent_len)
{
	struct ais_http_req *req = cli->user_data;
	int r;

	r = ais_http_handle_req(req);
	if (r < 0 && r != -EAGAIN)
		return r;

	if (cli->tx_buf.len == 0 && !req->keep_alive)
		return -ECONNABORTED;

	return 0;
	(void)sent_len;
}

static void ais_http_req_free(struct ais_http_req *req)
{
	if (!req)
		return;

	gwnet_http_hdr_pctx_free(&req->hdr_pctx);
	gwnet_http_req_hdr_free(&req->hdr);
	ais_http_res_free(&req->res);
	free(req);
}

static void http_close_callback(struct ais_sock_tcp_cli *cli)
{
	struct ais_http_req *req = cli->user_data;
	ais_http_req_free(req);
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

static int ais_http_wrk_init(struct ais_http_ctx *http_ctx, struct ais_http_wrk *wrk,
			     const struct ais_http_srv_iarg *iarg)
{
	int r;

	r = ais_sock_tcp_srv_init(&wrk->tcp_srv, &iarg->tcp);
	if (r < 0)
		return r;

	ais_sock_tcp_srv_set_cb_accept(&wrk->tcp_srv, &http_accept_callback);
	ais_sock_tcp_srv_set_cb_accept_arg(&wrk->tcp_srv, http_ctx);
	return 0;
}

int ais_http_ctx_init(struct ais_http_ctx *ctx, const struct ais_http_srv_iarg *iarg)
{
	struct ais_http_wrk *wrk;
	size_t i;
	int r;

	ctx->workers = calloc(iarg->nr_workers, sizeof(*ctx->workers));
	if (!ctx->workers)
		return -ENOMEM;

	ctx->nr_workers = iarg->nr_workers;
	for (i = 0; i < iarg->nr_workers; i++) {
		wrk = &ctx->workers[i];
		r = ais_http_wrk_init(ctx, wrk, iarg);
		if (r < 0) {
			ais_http_ctx_free(ctx);
			return r;
		}
	}

	return 0;
}

static void *ais_wrk_entry(void *arg)
{
	struct ais_http_wrk *wrk = arg;
	intptr_t r;

	r = ais_sock_tcp_srv_run(&wrk->tcp_srv);
	return (void *)r;
}

int ais_http_ctx_run(struct ais_http_ctx *ctx)
{
	size_t i, c = ctx->nr_workers - 1;
	void *p;
	int r;

	for (i = 0; i < c; i++) {
		struct ais_http_wrk *wrk = &ctx->workers[i];
		r = pthread_create(&wrk->thread, NULL, &ais_wrk_entry, wrk);
		if (r != 0)
			goto out_err;
	}

	p = ais_wrk_entry(&ctx->workers[c]);
	return (int) (intptr_t)p;

out_err:
	while (i--) {
		struct ais_http_wrk *wrk = &ctx->workers[i];
		ais_sock_tcp_srv_stop(&wrk->tcp_srv);
		pthread_join(wrk->thread, NULL);
	}
	return -r;
}

void ais_http_ctx_free(struct ais_http_ctx *ctx)
{
	size_t i;

	if (!ctx->workers)
		return;

	for (i = 0; i < ctx->nr_workers; i++)
		ais_sock_tcp_srv_free(&ctx->workers[i].tcp_srv);

	free(ctx->workers);
	memset(ctx, 0, sizeof(*ctx));
}

void ais_http_ctx_stop(struct ais_http_ctx *ctx)
{
	size_t i;

	if (!ctx->workers)
		return;

	for (i = 0; i < ctx->nr_workers; i++)
		ais_sock_tcp_srv_stop(&ctx->workers[i].tcp_srv);
}

int ais_http_res_body_set_buf(struct ais_http_res *res, const void *buf)
{
	return ais_http_res_body_set_bufl(res, buf, strlen(buf));
}

int ais_http_res_body_set_bufl(struct ais_http_res *res, const void *buf, size_t len)
{
	struct ais_http_res_body *b = &res->body;

	if (b->type != AIS_RES_BODY_TYPE_NONE)
		ais_http_res_body_free(b);

	b->type = AIS_RES_BODY_TYPE_BUF;
	return ais_buf_append(&b->buf, buf, len);
}

void ais_http_res_body_free(struct ais_http_res_body *b)
{
	switch (b->type) {
	case AIS_RES_BODY_TYPE_BUF:
		ais_buf_free(&b->buf);
		break;
	default:
		break;
	}
	memset(b, 0, sizeof(*b));
	b->type = AIS_RES_BODY_TYPE_NONE;
}
