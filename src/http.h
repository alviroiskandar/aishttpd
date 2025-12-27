// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef AISHTTPD_HTTP_H
#define AISHTTPD_HTTP_H

#include "http_parser/gwnet_http1.h"
#include "tcp.h"

struct ais_http_req;

typedef int (*ais_http_route_cb_t)(struct ais_http_req *req);
typedef int (*ais_http_accept_cb_t)(struct ais_http_req *req, void *arg);

struct ais_http_req {
	ais_http_route_cb_t		cb_route;
	struct gwnet_http_hdr_pctx	hdr_pctx;
	struct gwnet_http_req_hdr	hdr_req;
	struct gwnet_http_res_hdr	hdr_res;
	struct ais_sock_tcp_cli		*tcp_cli;
	char				addr[INET6_ADDRSTRLEN + sizeof(":65535")];
};

struct ais_http_ctx {
	struct ais_sock_tcp_srv		tcp_srv;
	ais_http_accept_cb_t		cb_accept;
	void				*cb_accept_arg;
};

struct ais_http_srv_iarg {
	struct ais_sock_tcp_srv_iarg	tcp;
};

int ais_http_ctx_init(struct ais_http_ctx *ctx, const struct ais_http_srv_iarg *iarg);
int ais_http_ctx_run(struct ais_http_ctx *ctx);
void ais_http_ctx_free(struct ais_http_ctx *ctx);
void ais_http_ctx_stop(struct ais_http_ctx *ctx);

static inline void ais_http_req_set_route_cb(struct ais_http_req *req, ais_http_route_cb_t cb)
{
	req->cb_route = cb;
}

static inline void ais_http_req_set_accept_cb(struct ais_http_ctx *ctx, ais_http_accept_cb_t cb)
{
	ctx->cb_accept = cb;
}

static inline void ais_http_req_set_accept_cb_arg(struct ais_http_ctx *ctx, void *arg)
{
	ctx->cb_accept_arg = arg;
}

#endif /* #ifndef AISHTTPD_HTTP_H */
