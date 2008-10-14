include ../../base.mk
CXXFLAGS+=-I../.. -fPIC

all: DemoBot.so

%.so: %.o
	$(CXX) -fPIC -shared "$<" -o "$@"

clean:
	rm -f *.o

distclean: clean
	rm -f *.so

.PHONY: all clean distclean
