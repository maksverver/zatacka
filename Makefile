
default:
	$(MAKE) $(XMAKEFLAGS) -C common
	$(MAKE) $(XMAKEFLAGS) -C client
	$(MAKE) $(XMAKEFLAGS) -C server

all:
	$(MAKE) $(XMAKEFLAGS) -C common all
	$(MAKE) $(XMAKEFLAGS) -C client all
	$(MAKE) $(XMAKEFLAGS) -C server all

clean:
	$(MAKE) $(XMAKEFLAGS) -C common clean
	$(MAKE) $(XMAKEFLAGS) -C client clean
	$(MAKE) $(XMAKEFLAGS) -C server clean
	
distclean: clean
	$(MAKE) $(XMAKEFLAGS) -C common distclean
	$(MAKE) $(XMAKEFLAGS) -C client distclean
	$(MAKE) $(XMAKEFLAGS) -C server distclean

.PHONY: default all clean distclean
