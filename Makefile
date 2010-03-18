
default:
	make $(XMAKEFLAGS) -C common
	make $(XMAKEFLAGS) -C client
	make $(XMAKEFLAGS) -C server

all:
	make $(XMAKEFLAGS) -C common all
	make $(XMAKEFLAGS) -C client all
	make $(XMAKEFLAGS) -C server all

clean:
	make $(XMAKEFLAGS) -C common clean
	make $(XMAKEFLAGS) -C client clean
	make $(XMAKEFLAGS) -C server clean
	
distclean: clean
	make $(XMAKEFLAGS) -C common distclean
	make $(XMAKEFLAGS) -C client distclean
	make $(XMAKEFLAGS) -C server distclean

.PHONY: default all clean distclean
