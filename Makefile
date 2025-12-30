# SPDX-License-Identifier: GPL-2.0-only
CC  := gcc
CXX := g++
LD  := $(CXX)

INCLUDES := -I./ -I./framework
DEPFLAGS := -MMD -MP
CFLAGS   := -Wall -Wextra -O2 -ggdb3 -fpic -fPIC -D_GNU_SOURCE $(DEPFLAGS) $(INCLUDES) -std=gnu11
CXXFLAGS := -Wall -Wextra -O2 -ggdb3 -fpic -fPIC -D_GNU_SOURCE $(DEPFLAGS) $(INCLUDES) -std=gnu++17
LDFLAGS  := -O2 -ggdb3
LIBS     := -lpthread

LIBAISHTTPD_SOURCES := \
	libaishttpd/http_parser/gwnet_http1.c \
	libaishttpd/http.c \
	libaishttpd/tcp.c \
	libaishttpd/buf.c \
	libaishttpd/file.c
LIBAISHTTPD_OBJECTS := $(LIBAISHTTPD_SOURCES:.c=.c.o)
LIBAISHTTPD_DEPS := $(LIBAISHTTPD_SOURCES:.c=.c.d)

AISHTTPD_SOURCES := \
	framework/aishttpd/Httpd.cpp \
	framework/aishttpd/Req.cpp \
	framework/aishttpd/Route.cpp \
	framework/aishttpd/Router.cpp \
	main.cpp
AISHTTPD_OBJECTS := $(AISHTTPD_SOURCES:.cpp=.cpp.o)
AISHTTPD_DEPS := $(AISHTTPD_SOURCES:.cpp=.cpp.d)

ifeq ($(SANITIZE),1)
	SANITIZE_FLAGS = -fsanitize=address -fsanitize=undefined
	CFLAGS += $(SANITIZE_FLAGS)
	CXXFLAGS += $(SANITIZE_FLAGS)
	LDFLAGS += $(SANITIZE_FLAGS)
endif

all: libaishttpd.so aishttpd

libaishttpd.so: $(LIBAISHTTPD_OBJECTS)
	$(LD) -shared -o $@ $^

libaishttpd.a: $(LIBAISHTTPD_OBJECTS)
	ar rcs $@ $^

aishttpd: $(AISHTTPD_OBJECTS) libaishttpd.a
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

-include $(LIBAISHTTPD_DEPS)
-include $(AISHTTPD_DEPS)

%.c.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f aishttpd $(LIBAISHTTPD_OBJECTS) $(LIBAISHTTPD_DEPS)

.PHONY: all clean
