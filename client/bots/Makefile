include ../../base.mk

FLTKC=fltk-config
CXXFLAGS+=-I../.. -fPIC

all: DemoBot.so Tipsy.so Twirly.so

%.so: %.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Twirly.so: Twirly.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Tipsy.so: Tipsy.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Hybrid.o: Hybrid.cpp; $(CXX) $(CXXFLAGS) `$(FLTKC) --cxxflags` -c $<
Hybrid.so: Hybrid.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ `fltk-config --ldstaticflags` -o $@

clean:
	rm -f *.o

distclean: clean
	rm -f *.so

.PHONY: all clean distclean
