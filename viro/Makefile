# SPDX-License-Identifier: GPL-2.0-only
CC  := gcc
CXX := g++
LD  := $(CXX)

INCLUDES := -I../ -I../framework
DEPFLAGS := -MMD -MP
CFLAGS   := -Wall -Wextra -O2 -ggdb3 -fpic -fPIC -D_GNU_SOURCE $(DEPFLAGS) $(INCLUDES) -std=gnu11
CXXFLAGS := -Wall -Wextra -O2 -ggdb3 -fpic -fPIC -D_GNU_SOURCE $(DEPFLAGS) $(INCLUDES) -std=gnu++17
LDFLAGS  := -O2 -ggdb3
LIBS     := -lpthread

WEB_SOURCES := \
	../framework/aishttpd/Httpd.cpp \
	../framework/aishttpd/Req.cpp \
	../framework/aishttpd/Route.cpp \
	../framework/aishttpd/Router.cpp \
	../signal.cpp \
	main.cpp
WEB_OBJECTS := $(WEB_SOURCES:.cpp=.cpp.o)
WEB_DEPS := $(WEB_SOURCES:.cpp=.cpp.d)

all: web

web: $(WEB_OBJECTS) ../libaishttpd.a
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

../libaishttpd.a:
	+$(MAKE) -C ../ libaishttpd.a;

-include $(WEB_DEPS)

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f web $(WEB_OBJECTS) $(WEB_DEPS);
	+$(MAKE) -C ../ clean;

.PHONY: all clean
