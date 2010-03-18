TOP=..
include $(TOP)/base.mk

CXXFLAGS+=`$(FLTKCONFIG) --cxxflags` -I..
LDLIBS+=`$(FLTKCONFIG) --ldflags`
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
	$(MAKE) -C bots

all: default
	$(MAKE) -C bots all

clean:
	rm -f $(OBJS)
	$(MAKE) -C bots clean

distclean: clean
	rm -f zatacka
	$(MAKE) -C bots distclean

zatacka: $(OBJS) ../common/common.a
	$(CXX) $(LDFLAGS) -o zatacka $(OBJS) $(LDLIBS)

.PHONY: default all clean distclean
