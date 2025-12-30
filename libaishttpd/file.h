// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef AISHTTPD_FILE_H
#define AISHTTPD_FILE_H

#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include "atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ais_file {
	_Atomic(int32_t)	refcnt;
	int			fd;
	uint64_t		size;
	char			*path;
	size_t			tb_idx;
	time_t			last_used;
};

/*
 * TODO(ammarfaizi2 + viro_ssfs):
 *   - Use hashtable lookup to avoid O(n) search time.
 *   - Use min heap to manage LRU cache.
 */
struct ais_file_table {
	struct ais_file		**files;
	size_t			cap;
	size_t			len;
	pthread_mutex_t		lock;
};

void ais_file_get(struct ais_file *f);
void ais_file_put(struct ais_file *f);
void ais_file_table_free(struct ais_file_table *tb);
int ais_file_table_init(struct ais_file_table *tb, size_t max);
int ais_file_table_get_or_open(struct ais_file_table *tb,
			       const char *path,
			       struct ais_file **out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* #ifndef AISHTTPD_FILE_H */
