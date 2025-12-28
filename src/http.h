// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef AISHTTPD_HTTP_H
#define AISHTTPD_HTTP_H

#include "http_parser/gwnet_http1.h"
#include "tcp.h"

#include <pthread.h>

struct ais_http_req;

typedef int (*ais_http_route_cb_t)(struct ais_http_req *req);
typedef int (*ais_http_accept_cb_t)(struct ais_http_req *req, void *arg);

enum {
	AIS_HREQ_STATE_INIT      = 0,
	AIS_HREQ_STATE_RECV_HDR  = 1,
	AIS_HREQ_STATE_RECV_BODY = 2,
	AIS_HREQ_STATE_BUILD_RES = 3,
	AIS_HREQ_STATE_SEND_RES  = 4,
	AIS_HREQ_STATE_DONE      = 5,
};

enum {
	AIS_RES_BODY_TYPE_NONE   = 0,
	AIS_RES_BODY_TYPE_BUF    = 1,
	AIS_RES_BODY_TYPE_FILE   = 2,
};

struct ais_http_res_body {
	uint8_t		type;
	union {
		struct ais_buf		buf;
	};
};

struct ais_http_res {
	struct gwnet_http_res_hdr	hdr;
	struct ais_http_res_body	body;
};

struct ais_http_req {
	uint8_t				state;
	ais_http_route_cb_t		cb_route;
	struct gwnet_http_hdr_pctx	hdr_pctx;
	struct gwnet_http_req_hdr	hdr;
	struct ais_sock_tcp_cli		*tcp_cli;
	struct ais_http_res		res;
	bool				keep_alive;
	char				addr[INET6_ADDRSTRLEN + sizeof(":65535")];
};

struct ais_http_wrk {
	struct ais_sock_tcp_srv		tcp_srv;
	pthread_t			thread;
};

struct ais_http_ctx {
	struct ais_http_wrk		*workers;
	uint32_t			nr_workers;
	ais_http_accept_cb_t		cb_accept;
	void				*cb_accept_arg;
};

struct ais_http_srv_iarg {
	struct ais_sock_tcp_srv_iarg	tcp;
	uint32_t			nr_workers;
};

int ais_http_ctx_init(struct ais_http_ctx *ctx, const struct ais_http_srv_iarg *iarg);
int ais_http_ctx_run(struct ais_http_ctx *ctx);
void ais_http_ctx_free(struct ais_http_ctx *ctx);
void ais_http_ctx_stop(struct ais_http_ctx *ctx);
const char *ais_http_get_method_name(int m);
int ais_http_res_body_set_buf(struct ais_http_res *res, const void *buf);
int ais_http_res_body_set_bufl(struct ais_http_res *res, const void *buf, size_t len);
void ais_http_res_body_free(struct ais_http_res_body *b);

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

static inline void ais_http_res_set_code(struct ais_http_res *res, uint16_t code)
{
	res->hdr.code = code;
}

static inline int ais_http_res_add_hdr(struct ais_http_res *res, const char *k, const char *v)
{
	return gwnet_http_hdr_fields_add(&res->hdr.fields, k, v);
}

#endif /* #ifndef AISHTTPD_HTTP_H */
