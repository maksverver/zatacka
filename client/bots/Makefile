BOTS=DemoBot.so

include ../../base.mk
CXXFLAGS+=-I../.. -fPIC

all: $(BOTS)

%.so: %.o
	$(CXX) -fPIC -shared -o DemoBot.so DemoBot.o

clean:
	rm -f *.o

distclean: clean
	rm -f *.so

.PHONY: all clean distclean
