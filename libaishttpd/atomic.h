// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#ifndef AISHTTPD_ATOMIC_H
#define AISHTTPD_ATOMIC_H

#ifdef __cplusplus
#include <atomic>
#define _Atomic(x) std::atomic<x>
extern "C" {
#else
#include <stdatomic.h>
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* #ifndef AISHTTPD_ATOMIC_H */
