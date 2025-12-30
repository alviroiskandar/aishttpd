// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#include "file.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

int ais_file_table_init(struct ais_file_table *tb, size_t max)
{
	int r;

	tb->files = calloc(max, sizeof(*tb->files));
	if (!tb->files)
		return -ENOMEM;

	r = pthread_mutex_init(&tb->lock, NULL);
	if (r != 0) {
		free(tb->files);
		return -r;
	}

	tb->cap = max;
	tb->len = 0;
	return 0;
}

void ais_file_table_free(struct ais_file_table *tb)
{
	size_t i;

	for (i = 0; i < tb->len; i++) {
		if (!tb->files[i])
			continue;
		ais_file_put(tb->files[i]);
	}

	free(tb->files);
	pthread_mutex_destroy(&tb->lock);
	memset(tb, 0, sizeof(*tb));
}

void ais_file_put(struct ais_file *f)
{
	if (atomic_fetch_sub(&f->refcnt, 1) == 1) {
		close(f->fd);
		free(f->path);
		free(f);
	}
}

void ais_file_get(struct ais_file *f)
{
	atomic_fetch_add(&f->refcnt, 1);
}

static int ais_file_table_get_path(struct ais_file_table *tb,
				   const char *path,
				   struct ais_file **out)
{
	size_t i;

	pthread_mutex_lock(&tb->lock);
	for (i = 0; i < tb->len; i++) {
		if (!tb->files[i])
			continue;
		if (strcmp(tb->files[i]->path, path) == 0) {
			struct ais_file *f = tb->files[i];
			ais_file_get(f);
			f->last_used = time(NULL);
			*out = f;
			pthread_mutex_unlock(&tb->lock);
			return 0;
		}
	}
	pthread_mutex_unlock(&tb->lock);

	return -ENOENT;
}

static int ais_file_table_insert(struct ais_file_table *tb,
				 struct ais_file *f)
{
	struct ais_file *to_evict = NULL;
	size_t i, evict_idx = 0;

	pthread_mutex_lock(&tb->lock);
	if (tb->len < tb->cap) {
		/*
		 * There's still space in the table. Insert directly.
		 */
		tb->files[tb->len++] = f;
		f->last_used = time(NULL);
		pthread_mutex_unlock(&tb->lock);
		return 0;
	}

	/*
	 * Table is full. Evict the least recently used file.
	 */
	for (i = 0; i < tb->len; i++) {
		if (!tb->files[i])
			continue;
		if (!to_evict || tb->files[i]->last_used < to_evict->last_used) {
			to_evict = tb->files[i];
			evict_idx = i;
		}
	}

	ais_file_put(to_evict);
	tb->files[evict_idx] = f;
	pthread_mutex_unlock(&tb->lock);
	return 0;
}

int ais_file_table_get_or_open(struct ais_file_table *tb,
			       const char *path,
			       struct ais_file **out)
{
	struct ais_file *f;
	struct stat st;
	int ret;

	ret = ais_file_table_get_path(tb, path, out);
	if (!ret) {
		f = *out;
		ret = fstat(f->fd, &st);
		if (ret < 0) {
			ais_file_put(f);
			return -errno;
		}

		if ((uint64_t)st.st_size != READ_ONCE(f->size))
			WRITE_ONCE(f->size, (uint64_t)st.st_size);

		return 0;
	}

	f = malloc(sizeof(*f));
	if (!f)
		return -ENOMEM;

	f->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (f->fd < 0) {
		ret = -errno;
		goto out_f;
	}

	ret = fstat(f->fd, &st);
	if (ret < 0) {
		ret = -errno;
		goto out_fd;
	}

	f->size = (uint64_t)st.st_size;
	f->path = strdup(path);
	if (!f->path) {
		ret = -ENOMEM;
		goto out_fd;
	}

	atomic_store(&f->refcnt, 2); /* one for caller, one for table */
	ret = ais_file_table_insert(tb, f);
	if (ret < 0)
		goto out_fd;

	*out = f;
	return 0;

out_fd:
	close(f->fd);
out_f:
	free(f);
	return ret;
}
