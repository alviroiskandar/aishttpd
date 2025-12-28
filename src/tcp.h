// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#ifndef AISHTTPD_TCP_H
#define AISHTTPD_TCP_H

#include "buf.h"

#include <stdbool.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>

struct ais_sock_addr {
	union {
		struct sockaddr		sa;
		struct sockaddr_in	in;
		struct sockaddr_in6	in6;
	};
};

struct ais_sock_tcp_cli;

typedef int (*ais_sock_tcp_srv_cb_accept_t)(struct ais_sock_tcp_cli *cli, void *arg);
typedef ssize_t (*ais_sock_tcp_cli_cb_rx_t)(struct ais_sock_tcp_cli *cli);
typedef int (*ais_sock_tcp_cli_cb_tx_t)(struct ais_sock_tcp_cli *cli, ssize_t sent_len);
typedef void (*ais_sock_tcp_cli_cb_close_t)(struct ais_sock_tcp_cli *cli);

/*
 * TCP client socket structure.
 *
 * This struct represents a connected TCP client. For each client,
 * we maintain its socket file descriptor and associated rx and tx
 * buffers. It also includes a user_data pointer for storing
 * application-specific data related to the client. Initially, the
 * user_data pointer is expected to contain HTTP context (which will
 * be used by higher-level abstractions).
 */
struct ais_sock_tcp_srv;
struct ais_sock_tcp_cli {
	int			fd;
	uint32_t		idx;
	void			*user_data;
	struct ais_buf		rx_buf;
	struct ais_buf		tx_buf;
	struct ais_sock_addr	addr;
	struct ais_sock_tcp_srv	*srv;
	uint32_t		ep_mask;

	ais_sock_tcp_cli_cb_rx_t	cb_rx;
	ais_sock_tcp_cli_cb_tx_t	cb_tx;
	ais_sock_tcp_cli_cb_close_t	cb_close;
};

/*
 * TCP server socket structure.
 *
 * This struct represents the TCP server socket that listens for
 * incoming client connections. It maintains the main listening
 * socket file descriptor, an array of epoll events for monitoring
 * socket activity, and an array of connected client sockets.
 */
struct ais_sock_tcp_srv {
	volatile bool		should_stop;

	/*
	 * Main server socket file descriptor (listening socket).
	 */
	int			fd;

	/*
	 * Epoll file descriptor for monitoring events.
	 */
	int			ep_fd;

	/*
	 * Eventfd for waking up the epoll wait loop.
	 * Useful for signaling server shutdown.
	 */
	int			ev_fd;

	int			sock_backlog;
	struct ais_sock_addr	bind_addr;

	/*
	 * Epoll event array and count. The nevents is expected
	 * to be set to the maximum number of events that can be
	 * handled at once.
	 */
	struct epoll_event	*events;
	size_t			nevents;

	/*
	 * Array of connected TCP clients and count.
	 */
	struct ais_sock_tcp_cli	**clients;
	size_t			nclients;
	size_t			cap_clients;
	size_t			max_clients;

	/*
	 * Callback function invoked when a new client connection
	 * is accepted.
	 */
	ais_sock_tcp_srv_cb_accept_t	cb_accept;
	void				*cb_accept_arg;
};

/*
 * Initialization argument structure for TCP server.
 */
struct ais_sock_tcp_srv_iarg {
	const char	*bind_addr;
	uint16_t	port;
	int		sock_backlog;
	size_t		epoll_nevents;
	size_t		max_clients;
};

int ais_sock_tcp_srv_init(struct ais_sock_tcp_srv *srv, const struct ais_sock_tcp_srv_iarg *iarg);
void ais_sock_tcp_srv_free(struct ais_sock_tcp_srv *srv);

static inline void ais_sock_tcp_srv_set_cb_accept(struct ais_sock_tcp_srv *srv, ais_sock_tcp_srv_cb_accept_t cb)
{
	srv->cb_accept = cb;
}

static inline void ais_sock_tcp_srv_set_cb_accept_arg(struct ais_sock_tcp_srv *srv, void *arg)
{
	srv->cb_accept_arg = arg;
}

static inline void ais_sock_tcp_cli_set_cb_rx(struct ais_sock_tcp_cli *cli, ais_sock_tcp_cli_cb_rx_t cb)
{
	cli->cb_rx = cb;
}

static inline void ais_sock_tcp_cli_set_cb_tx(struct ais_sock_tcp_cli *cli, ais_sock_tcp_cli_cb_tx_t cb)
{
	cli->cb_tx = cb;
}

static inline void ais_sock_tcp_cli_set_cb_close(struct ais_sock_tcp_cli *cli, ais_sock_tcp_cli_cb_close_t cb)
{
	cli->cb_close = cb;
}

int ais_sock_tcp_srv_run(struct ais_sock_tcp_srv *srv);
void ais_sock_tcp_srv_stop(struct ais_sock_tcp_srv *srv);

enum {
	AIS_EV_DATA_TCP_SRV = (1ull << 48ull),
	AIS_EV_DATA_TCP_CLI = (2ull << 48ull),
	AIS_EV_DATA_EV_FD   = (3ull << 48ull),
};

#define AIS_EV_GET_DATA(X)	((X) & 0xffff000000000000ull)
#define AIS_EV_GET_PTR(X)	((X) & 0x0000ffffffffffffull)

#endif /* #ifndef AISHTTPD_TCP_H */
