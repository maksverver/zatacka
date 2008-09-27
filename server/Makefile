include ../base.mk

CFLAGS+=-std=c99 -I../common/
LDLIBS+=../common/common.a
OBJS=zatacka-server.o

all: zatacka-server

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f zatacka-server

zatacka-server: $(OBJS)
	$(CC) $(CFLAGS) -o zatacka-server $(OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: all clean distclean
