include ../../base.mk

FLTKCONFIG=fltk-config
CXXFLAGS+=-I../.. -fPIC

all: DemoBot.so Tipsy.so Twirly.so

%.so: %.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Twirly.so: Twirly.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Tipsy.so: Tipsy.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ -o $@

Hybrid.o: Hybrid.cpp; $(CXX) $(CXXFLAGS) `$(FLTKCONFIG) --cxxflags` -c $<
Hybrid.so: Hybrid.o SimpleSearch.o ../../common/common.a
	$(CXX) -fPIC -shared $^ `$(FLTKCONFIG) --ldflags` -o $@

DemoBotWin.o: DemoBotWin.cpp
	$(CXX) $(CXXFLAGS) `$(FLTKCONFIG) --cxxflags` -c $<
DemoBotWin.so: DemoBotWin.o ../../common/common.a
	$(CXX) -fPIC -shared $^ `$(FLTKCONFIG) --ldflags` -o $@

clean:
	rm -f *.o

distclean: clean
	rm -f *.so

.PHONY: all clean distclean
