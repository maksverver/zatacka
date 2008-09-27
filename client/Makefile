include ../base.mk

FLTKC=../fltk/bin/fltk-config
CXXFLAGS+=`$(FLTKC) --cxxflags` -I../common/
LDFLAGS+=`$(FLTKC) --ldstaticflags`
LDLIBS+=../common/common.a
OBJS=ClientSocket.o Config.o GameModel.o GameView.o ScoreView.o zatacka.o

all: zatacka

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f zatacka

zatacka: $(OBJS) 
	$(CXX) -o zatacka $(OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: all clean distclean
