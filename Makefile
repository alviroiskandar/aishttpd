# SPDX-License-Identifier: GPL-2.0-only
CC := gcc
CXX := g++
CFLAGS := -Wall -Wextra -O2 -ggdb3 -std=gnu11 -D_GNU_SOURCE
CXXFLAGS := -Wall -Wextra -O2 -ggdb3 -std=gnu++17 -D_GNU_SOURCE
LDFLAGS := -O2 -ggdb3
DEPFLAGS := -MMD -MP
AISHTTPD_SOURCES := \
	libaishttpd/http_parser/gwnet_http1.c \
	libaishttpd/main.c \
	libaishttpd/http.c \
	libaishttpd/tcp.c \
	libaishttpd/buf.c \
	libaishttpd/file.c
AISHTTPD_OBJECTS := $(AISHTTPD_SOURCES:.c=.c.o)
AISHTTPD_DEPS := $(AISHTTPD_SOURCES:.c=.c.d)

ifeq ($(SANITIZE),1)
	CFLAGS += -fsanitize=address -fsanitize=undefined
	LDFLAGS += -fsanitize=address -fsanitize=undefined
endif

all: aishttpd

aishttpd: $(AISHTTPD_OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

-include $(AISHTTPD_DEPS)

%.c.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f aishttpd $(AISHTTPD_OBJECTS) $(AISHTTPD_DEPS)

.PHONY: all clean
