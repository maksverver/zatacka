include ../../base.mk
CXXFLAGS+=-I../.. -fPIC

all: DemoBot.so Tipsy.so Twirly.so

%.so: %.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Twirly.so: Twirly.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Tipsy.so: Tipsy.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

clean:
	rm -f *.o

distclean: clean
	rm -f *.so

.PHONY: all clean distclean
