
default:
	make $(MAKEFLAGS) -C common
	make $(MAKEFLAGS) -C client
	make $(MAKEFLAGS) -C server

all:
	make $(MAKEFLAGS) -C common all
	make $(MAKEFLAGS) -C client all
	make $(MAKEFLAGS) -C server all

clean:
	make $(MAKEFLAGS) -C common clean
	make $(MAKEFLAGS) -C client clean
	make $(MAKEFLAGS) -C server clean
	
distclean: clean
	make $(MAKEFLAGS) -C common distclean
	make $(MAKEFLAGS) -C client distclean
	make $(MAKEFLAGS) -C server distclean

.PHONY: default all clean distclean
