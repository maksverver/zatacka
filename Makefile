
default:
	make -C common
	make -C client
	make -C server

clean:
	make -C common clean
	make -C client clean
	make -C server clean
	
distclean: clean
	make -C common distclean
	make -C client distclean
	make -C server distclean

.PHONY: all clean distclean
