TOP=..
include $(TOP)/base.mk

CXXFLAGS+=`$(FLTKCONFIG) --cxxflags` -I..
LDFLAGS+=`$(FLTKCONFIG) --ldflags`
LDLIBS+=../common/common.a
OBJS=	ClientSocket.o Config.o GameModel.o GameView.o KeyWindow.o \
	KeyboardPlayerController.o MainWindow.o MainGameView.o \
	ScoreView.o zatacka.o

ifdef LIBMIKMODCONFIG
OBJS+=Audio.o
CXXFLAGS+=-DWITH_AUDIO=1 `$(LIBMIKMODCONFIG) --cflags`
LDLIBS+=`$(LIBMIKMODCONFIG) --libs`
endif

default: zatacka
	make -C bots

clean:
	rm -f $(OBJS)
	make -C bots clean

distclean: clean
	rm -f zatacka
	make -C bots distclean

zatacka: $(OBJS) ../common/common.a
	$(CXX) -o zatacka $(OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: all clean distclean
