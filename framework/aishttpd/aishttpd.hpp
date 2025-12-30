// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 * Copyright (C) 2025  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */
#ifndef FRAMEWORK__AISHTTPD__AISHTTPD_HPP
#define FRAMEWORK__AISHTTPD__AISHTTPD_HPP

#include <libaishttpd/http.h>
#include <memory>

namespace aishttpd {

class aishttpd {
private:
	struct ais_http_ctx http_ctx_;
	std::unique_ptr<struct ais_http_srv_iarg> iarg_;
public:
};

} /* namespace aishttpd */

#endif /* #ifndef FRAMEWORK__AISHTTPD__AISHTTPD_HPP */
