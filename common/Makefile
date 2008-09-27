include ../base.mk

OBJS=BMP.o Field.o Debug.o Time.o

all: common.a

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f common.a

common.a: $(OBJS)
	$(AR) crs common.a $(OBJS)

.PHONY: all clean distclean
