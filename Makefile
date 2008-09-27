
all:
	make -C common all
	make -C client all
	make -C server all

clean:
	make -C common clean
	make -C client clean
	make -C server clean
	
distclean: clean
	make -C common distclean
	make -C client distclean
	make -C server distclean

.PHONY: all clean distclean
