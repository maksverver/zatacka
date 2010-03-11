TOP=../..
include $(TOP)/base.mk

CXXFLAGS+=-I../.. `$(FLTKCONFIG) --cxxflags`
LDFLAGS+=-fPIC -Wl,--as-needed `$(FLTKCONFIG) --ldflags`

default: DemoBot.so Tipsy.so Twirly.so

all: default DemoBotWin.so Hybrid.so PlotBot.so

SimpleSearch.a: SimpleSearch.o
	$(AR) rcs $@ $^

%.so: %.o SimpleSearch.a ../../common/common.a
	$(CXX) $(LDFLAGS) -shared $^ -o $@
clean:
	rm -f *.o *.a

distclean: clean
	rm -f *.so

.PHONY: all clean distclean
