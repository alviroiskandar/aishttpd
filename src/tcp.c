// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */
#include "tcp.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/eventfd.h>
#include <assert.h>

int ais_sock_tcp_srv_init(struct ais_sock_tcp_srv *srv, const struct ais_sock_tcp_srv_iarg *iarg)
{
	struct ais_sock_addr *ba = &srv->bind_addr;
	int err, fd, ep_fd, ev_fd;

	memset(srv, 0, sizeof(*srv));
	if (inet_pton(AF_INET, iarg->bind_addr, &ba->in.sin_addr) == 1) {
		ba->in.sin_family = AF_INET;
		ba->in.sin_port = htons(iarg->port);
	} else if (inet_pton(AF_INET6, iarg->bind_addr, &ba->in6.sin6_addr) == 1) {
		ba->in6.sin6_family = AF_INET6;
		ba->in6.sin6_port = htons(iarg->port);
	} else {
		return -EINVAL;
	}

	if (iarg->sock_backlog < 0 || iarg->sock_backlog > SOMAXCONN)
		return -EINVAL;

	srv->sock_backlog = (iarg->sock_backlog == 0) ? SOMAXCONN : iarg->sock_backlog;

	/*
	 * Only create the socket, don't bind or listen yet. Allow
	 * the caller to call setsockopt() before binding and listening.
	 */
	fd = socket(ba->sa.sa_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
	if (fd < 0)
		return -errno;

	ev_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (ev_fd < 0) {
		err = -errno;
		goto out_err_socket;
	}

	ep_fd = epoll_create1(EPOLL_CLOEXEC);
	if (ep_fd < 0) {
		err = -errno;
		goto out_err_eventfd;
	}

	srv->events = calloc(iarg->epoll_nevents, sizeof(*srv->events));
	if (!srv->events) {
		err = -ENOMEM;
		goto out_err_epoll;
	}

	srv->clients = calloc(iarg->max_clients, sizeof(*srv->clients));
	if (!srv->clients) {
		err = -ENOMEM;
		goto out_err_events;
	}

	srv->fd = fd;
	srv->ep_fd = ep_fd;
	srv->ev_fd = ev_fd;
	srv->nevents = iarg->epoll_nevents;
	srv->max_clients = iarg->max_clients;
	return 0;

out_err_events:
	free(srv->events);
out_err_epoll:
	close(ep_fd);
out_err_eventfd:
	close(ev_fd);
out_err_socket:
	close(fd);
	memset(srv, 0, sizeof(*srv));
	return err;
}

static void ais_sock_tcp_cli_free(struct ais_sock_tcp_cli *cli)
{
	if (cli->cb_close)
		cli->cb_close(cli);

	if (cli->fd >= 0)
		close(cli->fd);

	ais_buf_free(&cli->rx_buf);
	ais_buf_free(&cli->tx_buf);
	free(cli);
}

static void ais_sock_tcp_cli_free_all(struct ais_sock_tcp_srv *srv)
{
	struct ais_sock_tcp_cli *cli;
	size_t i;

	for (i = 0; i < srv->nclients; i++) {
		cli = srv->clients[i];
		if (!cli)
			continue;

		ais_sock_tcp_cli_free(cli);
		srv->clients[i] = NULL;
	}
	srv->nclients = 0;
}

void ais_sock_tcp_srv_free(struct ais_sock_tcp_srv *srv)
{
	if (!srv)
		return;

	ais_sock_tcp_cli_free_all(srv);
	free(srv->clients);
	free(srv->events);
	close(srv->fd);
	close(srv->ep_fd);
	close(srv->ev_fd);
	memset(srv, 0, sizeof(*srv));
}

static int __handle_event_tcp_srv(struct ais_sock_tcp_srv *srv)
{
	struct ais_sock_addr addr;
	struct ais_sock_tcp_cli *cli;
	socklen_t addr_len = sizeof(addr);
	struct epoll_event ev;
	int r, fd;

	fd = accept4(srv->fd, &addr.sa, &addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (fd < 0)
		return -errno;

	if (srv->nclients >= srv->max_clients) {
		/*
		 * Maximum clients reached, drop the connection.
		 */
		close(fd);
		return -EAGAIN;
	}

	cli = calloc(1, sizeof(*cli));
	if (!cli)
		goto out_err_close;

	/*
	 * TODO(viro_ssfs): Make the buffer size configurable from the caller.
	 */
	r = ais_buf_init(&cli->rx_buf, 1024);
	if (r < 0)
		goto out_err_free_cli;
	r = ais_buf_init(&cli->tx_buf, 512);
	if (r < 0)
		goto out_err_free_cli;

	cli->fd = fd;
	cli->srv = srv;
	cli->addr = addr;
	if (srv->cb_accept) {
		r = srv->cb_accept(cli, srv->cb_accept_arg);
		if (r < 0) {
			cli->fd = -1;
			ais_sock_tcp_cli_free(cli);
			return r;
		}
	}

	cli->ep_mask = ev.events = EPOLLIN;
	ev.data.u64 = 0;
	ev.data.ptr = cli;
	ev.data.u64 |= AIS_EV_DATA_TCP_CLI;
	if (epoll_ctl(srv->ep_fd, EPOLL_CTL_ADD, cli->fd, &ev) < 0) {
		r = -errno;
		cli->fd = -1;
		ais_sock_tcp_cli_free(cli);
		return r;
	}

	cli->idx = (uint32_t)srv->nclients++;
	srv->clients[cli->idx] = cli;
	return 0;

out_err_free_cli:
	ais_sock_tcp_cli_free(cli);
out_err_close:
	close(fd);
	return -ENOMEM;
}

static int handle_event_tcp_srv(struct ais_sock_tcp_srv *srv, struct epoll_event *ev)
{
	if (ev->events & (EPOLLERR | EPOLLHUP))
		return -EIO;

	/*
	 * If we get here, it must be an EPOLLIN event.
	 */
	assert(ev->events & EPOLLIN);

	while (true) {
		int r = __handle_event_tcp_srv(srv);
		if (r < 0)
			return (r == -EAGAIN) ? 0 : r;
	}
}

static int handle_event_tcp_cli_recv(struct ais_sock_tcp_cli *cli)
{
	struct ais_buf *rxb = &cli->rx_buf;
	ssize_t recv_ret;
	size_t recv_len;
	void *recv_buf;

	recv_len = rxb->cap - rxb->len;
	if (!recv_len) {
		if (ais_buf_increase(rxb, 4096) < 0)
			return -ENOMEM;
		recv_len = rxb->cap - rxb->len;
	}

	recv_buf = &rxb->buf[rxb->len];
	recv_ret = recv(cli->fd, recv_buf, recv_len, MSG_DONTWAIT);
	if (recv_ret < 0) {
		int err = -errno;
		if (err == -EAGAIN || err == -EINTR) {
			if (cli->rx_buf.len > 0)
				goto invoke_cb;
			return 0;
		}

		return err;
	}

	if (recv_ret == 0) {
		/* Connection closed? */
		return -EIO;
	}

	cli->rx_buf.len += recv_ret;

invoke_cb:
	if (cli->cb_rx) {
		recv_ret = cli->cb_rx(cli);
		if (recv_ret < 0 && recv_ret != -EAGAIN)
			return recv_ret;
	}

	return 0;
}

static int handle_event_tcp_cli_send(struct ais_sock_tcp_cli *cli)
{
	ssize_t send_ret, r = 0;
	size_t send_len;
	void *send_buf;

	send_len = cli->tx_buf.len;
	send_buf = cli->tx_buf.buf;
	send_ret = send(cli->fd, send_buf, send_len, MSG_DONTWAIT | MSG_NOSIGNAL);
	if (send_ret < 0) {
		int err = -errno;
		if (err == -EAGAIN || err == -EINTR)
			return 0;

		return err;
	}

	assert((size_t)send_ret <= cli->tx_buf.len);
	ais_buf_soft_advance(&cli->tx_buf, (size_t)send_ret);
	if (cli->cb_tx)
		r = cli->cb_tx(cli, send_ret);

	return (r < 0) ? r : 0;
}

static int adjust_epoll_events(struct ais_sock_tcp_srv *srv, struct ais_sock_tcp_cli *cli)
{
	bool need_mod = false;
	/*
	 * TODO(viro_ssfs):
	 *  - Add handling for EPOLLIN enabling/disabling as well (when the rx_buf is full).
	 */

	/*
	 * Handle EPOLLOUT enabling/disabling. If the EPOLLOUT is enabled,
	 * but there's no data to send, disable it. If the EPOLLOUT is
	 * disabled, but there's data to send, enable it.
	 */
	if (cli->ep_mask & EPOLLOUT) {
		if (cli->tx_buf.len == 0) {
			need_mod = true;
			cli->ep_mask &= ~EPOLLOUT;
		}
	} else {
		if (cli->tx_buf.len > 0) {
			need_mod = true;
			cli->ep_mask |= EPOLLOUT;
		}
	}

	if (need_mod) {
		struct epoll_event ev;
		ev.events = cli->ep_mask;
		ev.data.u64 = 0;
		ev.data.ptr = cli;
		ev.data.u64 |= AIS_EV_DATA_TCP_CLI;
		if (epoll_ctl(srv->ep_fd, EPOLL_CTL_MOD, cli->fd, &ev) < 0)
			return -errno;
	}

	return 0;
}

static void ais_sock_tcp_srv_close_cli(struct ais_sock_tcp_srv *srv, struct ais_sock_tcp_cli *cli)
{
	uint32_t last_idx = (uint32_t)(srv->nclients - 1);
	struct ais_sock_tcp_cli **clients = srv->clients;

	epoll_ctl(srv->ep_fd, EPOLL_CTL_DEL, cli->fd, NULL);

	/*
	 * If the closed client is not the last one in the array,
	 * move the last one to its place to keep the array dense.
	 * Also update the moved client's index accordingly.
	 */
	if (cli->idx != last_idx) {
		clients[cli->idx] = clients[last_idx];
		clients[cli->idx]->idx = cli->idx;
	}

	clients[last_idx] = NULL;
	srv->nclients--;
	ais_sock_tcp_cli_free(cli);
}

static int handle_event_tcp_cli(struct ais_sock_tcp_srv *srv, struct epoll_event *ev, void *udata)
{
	struct ais_sock_tcp_cli *cli = udata;
	int ret;

	if (ev->events & (EPOLLERR | EPOLLHUP)) {
		ret = -EIO;
		goto out_close;
	}

	if (ev->events & EPOLLIN) {
		ret = handle_event_tcp_cli_recv(cli);
		if (ret < 0)
			goto out_close;

		/*
		 * cli->cb_rx() may give us some data to send.
		 *
		 * If so, enable EPOLLOUT event locally to notify
		 * the code below to handle sending data.
		 */
		if (cli->tx_buf.len > 0)
			ev->events |= EPOLLOUT;
	}

	if (ev->events & EPOLLOUT) {
		ret = handle_event_tcp_cli_send(cli);
		if (ret < 0)
			goto out_close;
	}

	return adjust_epoll_events(srv, cli);

out_close:
	if (ret < 0)
		ais_sock_tcp_srv_close_cli(srv, cli);

	return 0;
}

static int handle_event(struct ais_sock_tcp_srv *srv, struct epoll_event *ev)
{
	uint64_t ev_data;
	void *udata;

	ev_data = AIS_EV_GET_DATA(ev->data.u64);
	ev->data.u64 = AIS_EV_GET_PTR(ev->data.u64);
	udata = ev->data.ptr;

	switch (ev_data) {
	case AIS_EV_DATA_TCP_SRV:
		return handle_event_tcp_srv(srv, ev);
	case AIS_EV_DATA_TCP_CLI:
		return handle_event_tcp_cli(srv, ev, udata);
	case AIS_EV_DATA_EV_FD:
		eventfd_read(srv->ev_fd, &ev_data);
		return 0;
	default:
		assert(false && "Unknown event data");
		return -EINVAL;
	}
}

static int reap_events(struct ais_sock_tcp_srv *srv, int nevents)
{
	struct epoll_event *events = srv->events;
	int r, i;

	for (i = 0; i < nevents; i++) {
		r = handle_event(srv, &events[i]);
		if (r < 0)
			return r;
	}

	return 0;
}

static int fish_events(struct ais_sock_tcp_srv *srv)
{
	struct epoll_event *events = srv->events;
	size_t nevents = srv->nevents;
	int r;

	r = epoll_wait(srv->ep_fd, events, nevents, -1);
	if (r < 0) {
		r = -errno;
		return (r == -EINTR) ? 0 : r;
	}

	return r;
}

static int start_event_loop(struct ais_sock_tcp_srv *srv)
{
	int r = 0;

	while (!srv->should_stop) {
		r = fish_events(srv);
		if (r < 0)
			break;

		r = reap_events(srv, r);
		if (r < 0)
			break;
	}

	return r;
}

static int prepare_initial_epoll(struct ais_sock_tcp_srv *srv)
{
	struct epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.u64 = 0;
	ev.data.ptr = srv;
	ev.data.u64 |= AIS_EV_DATA_TCP_SRV;
	if (epoll_ctl(srv->ep_fd, EPOLL_CTL_ADD, srv->fd, &ev) < 0)
		return -errno;

	ev.events = EPOLLIN;
	ev.data.u64 = 0;
	ev.data.ptr = srv;
	ev.data.u64 |= AIS_EV_DATA_EV_FD;
	if (epoll_ctl(srv->ep_fd, EPOLL_CTL_ADD, srv->ev_fd, &ev) < 0)
		return -errno;

	return 0;
}

static int do_setsockopt(int skfd)
{
	int v = 1, r = 0;

	r |= setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
	r |= setsockopt(skfd, SOL_SOCKET, SO_REUSEPORT, &v, sizeof(v));
	r |= setsockopt(skfd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
	return r ? -errno : 0;
}

int ais_sock_tcp_srv_run(struct ais_sock_tcp_srv *srv)
{
	int err;

	do_setsockopt(srv->fd);

	if (bind(srv->fd, &srv->bind_addr.sa, sizeof(srv->bind_addr)) < 0)
		return -errno;

	if (listen(srv->fd, srv->sock_backlog) < 0)
		return -errno;

	err = prepare_initial_epoll(srv);
	if (err < 0)
		return err;

	return start_event_loop(srv);
}

void ais_sock_tcp_srv_stop(struct ais_sock_tcp_srv *srv)
{
	srv->should_stop = true;
	eventfd_write(srv->ev_fd, 1);
}
