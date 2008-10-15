include ../base.mk

OBJS=BMP.o Colors.o Debug.o Field.o Time.o

all: common.a

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f common.a

common.a: $(OBJS)
	$(AR) crs common.a $(OBJS)

.PHONY: all clean distclean
