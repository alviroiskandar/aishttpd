// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef AISHTTPD_BUF_H
#define AISHTTPD_BUF_H

#include <stddef.h>
#include <assert.h>

struct ais_buf {
	char	*buf;
	char	*orig_buf;
	size_t	len;
	size_t	cap;
};

static inline
void ais_buf_soft_advance(struct ais_buf *b, size_t len)
{
	assert(len <= b->len);
	b->len -= len;
	b->buf += len;
}

int ais_buf_init(struct ais_buf *b, size_t cap);
int ais_buf_increase(struct ais_buf *b, size_t inc);
void ais_buf_free(struct ais_buf *b);
void ais_buf_advance(struct ais_buf *b, size_t len);
void ais_buf_soft_advance_sync(struct ais_buf *b);
int ais_buf_prepare_need(struct ais_buf *b, size_t need);
int ais_buf_apfmt(struct ais_buf *b, const char *fmt, ...);
int ais_buf_append(struct ais_buf *b, const void *data, size_t len);
void ais_buf_move(struct ais_buf *dst, struct ais_buf *src);

#endif /* #ifndef AISHTTPD_BUF_H */
