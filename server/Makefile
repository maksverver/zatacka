TOP=..
include $(TOP)/base.mk

CFLAGS+=-std=c99 -D_XOPEN_SOURCE=500 -I.. -DBMP -DREPLAY 
LDLIBS+=../common/common.a
OBJS=zatacka-server.o

all: zatacka-server

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f zatacka-server

zatacka-server: $(OBJS) ../common/common.a
	$(CC) $(CFLAGS) -o zatacka-server $(OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: all clean distclean
