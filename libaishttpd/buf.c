// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#include "buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

int ais_buf_init(struct ais_buf *b, size_t cap)
{
	if (!cap)
		cap = 1023ull;

	b->orig_buf = b->buf = malloc(cap + 1ull);
	if (!b->buf)
		return -ENOMEM;

	b->buf[cap] = '\0';
	b->len = 0;
	b->cap = cap;
	return 0;
}

static int ais_buf_set_cap(struct ais_buf *b, size_t new_cap)
{
	char *new_buf;

	new_buf = realloc(b->buf, new_cap + 1ull);
	if (!new_buf)
		return -ENOMEM;

	b->orig_buf = b->buf = new_buf;
	b->cap = new_cap;
	b->len = (b->len > new_cap) ? new_cap : b->len;
	b->buf[b->cap] = b->buf[b->len] = '\0';
	return 0;
}

int ais_buf_increase(struct ais_buf *b, size_t inc)
{
	if (!inc)
		return 0;

	return ais_buf_set_cap(b, b->cap + inc);
}

void ais_buf_free(struct ais_buf *b)
{
	if (!b || !b->buf || !b->orig_buf)
		return;

	free(b->orig_buf);
	memset(b, 0, sizeof(*b));
}

void ais_buf_advance(struct ais_buf *b, size_t len)
{
	if (len >= b->len) {
		ais_buf_free(b);
		return;
	}

	b->len -= len;
	memmove(b->buf, &b->buf[len], b->len);
	b->buf[b->len] = '\0';
}

void ais_buf_soft_advance_sync(struct ais_buf *b)
{
	assert(b->buf >= b->orig_buf);
	assert(b->cap >= b->len);

	if (b->buf == b->orig_buf)
		return;

	if (!b->len) {
		ais_buf_free(b);
		return;
	}

	memmove(b->orig_buf, b->buf, b->len);
	b->orig_buf[b->len] = '\0';
	b->buf = b->orig_buf;
}

int ais_buf_prepare_need(struct ais_buf *b, size_t need)
{
	size_t needed_len;
	size_t new_cap;

	if (b->orig_buf != b->buf)
		ais_buf_soft_advance_sync(b);

	if (need <= b->cap - b->len)
		return 0;

	needed_len = b->len + need;
	new_cap = (b->cap + 1ull) * 2ull;
	while (new_cap < needed_len)
		new_cap *= 2ull;

	return ais_buf_set_cap(b, new_cap);
}

int ais_buf_apfmt(struct ais_buf *b, const char *fmt, ...)
{
	va_list args, args2;
	size_t free_space;
	int len, ret;

	va_start(args, fmt);
	va_copy(args2, args);
	len = vsnprintf(NULL, 0, fmt, args2);
	va_end(args2);

	ret = ais_buf_prepare_need(b, len + 1);
	if (ret < 0)
		goto out;

	free_space = b->cap - b->len;
	ret = vsnprintf(&b->buf[b->len], free_space + 1ul, fmt, args);
	b->len += ret;
	ret = 0;
out:
	va_end(args);
	return ret;
}

int ais_buf_append(struct ais_buf *b, const void *data, size_t len)
{
	int ret;

	ret = ais_buf_prepare_need(b, len);
	if (ret < 0)
		return ret;

	memcpy(&b->buf[b->len], data, len);
	b->len += len;
	b->buf[b->len] = '\0';
	return 0;
}

void ais_buf_move(struct ais_buf *dst, struct ais_buf *src)
{
	ais_buf_free(dst);
	*dst = *src;
	memset(src, 0, sizeof(*src));
}
