// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef AISHTTPD_HTTP_H
#define AISHTTPD_HTTP_H

#include "http_parser/gwnet_http1.h"
#include "tcp.h"

struct ais_http_req {
	struct gwnet_http_hdr_pctx	hdr_pctx;
	struct gwnet_http_req_hdr	hdr;
	char				addr[INET6_ADDRSTRLEN + sizeof(":65535")];
};

struct ais_http_ctx {
	struct ais_sock_tcp_srv		tcp_srv;
};

int ais_http_ctx_init(struct ais_http_ctx *ctx, const struct ais_sock_tcp_srv_iarg *iarg);
int ais_http_ctx_run(struct ais_http_ctx *ctx);
void ais_http_ctx_free(struct ais_http_ctx *ctx);
void ais_http_ctx_stop(struct ais_http_ctx *ctx);

#endif /* #ifndef AISHTTPD_HTTP_H */
