TOP=../..
include $(TOP)/base.mk

CXXFLAGS+=-I../.. `$(FLTKCONFIG) --cxxflags`
LDFLAGS+=-fPIC -Wl,--as-needed
LDLIBS+=`$(FLTKCONFIG) --ldflags`

default: DemoBot.so Tipsy.so Twirly.so

all: default DemoBotWin.so Hybrid.so PlotBot.so

SimpleSearch.a: SimpleSearch.o
	$(AR) rcs $@ $^

%.so: %.o SimpleSearch.a ../../common/common.a
	$(CXX) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS)
clean:
	rm -f *.o *.a

distclean: clean
	rm -f *.so

.PHONY: default all clean distclean
