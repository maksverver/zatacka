TOP=..
include $(TOP)/base.mk

CFLAGS+=-std=c99 -I..
LDLIBS+=../common/common.a
OBJS=zatacka-server.o

ifeq "$(shell uname -o)" "GNU/Linux"
CFLAGS+=-D_POSIX_SOURCE -D_BSD_SOURCE
endif

all: zatacka-server

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f zatacka-server

zatacka-server: $(OBJS) ../common/common.a
	$(CC) $(CFLAGS) -o zatacka-server $(OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: all clean distclean
