include ../base.mk

FLTKCFG=fltk-config
#LIBMIKMODCFG=libmikmod-config
CXXFLAGS+=`$(FLTKCFG) --cxxflags` -I..
LDFLAGS+=`$(FLTKCFG) --ldflags`
LDLIBS+=../common/common.a
OBJS=	ClientSocket.o Config.o GameModel.o GameView.o KeyWindow.o \
	KeyboardPlayerController.o MainWindow.o MainGameView.o \
	ScoreView.o zatacka.o

ifdef LIBMIKMODCFG
OBJS+=Audio.o
CXXFLAGS+=-DWITH_AUDIO=1 `$(LIBMIKMODCFG) --cflags`
LDLIBS+=`$(LIBMIKMODCFG) --libs`
endif

all: zatacka
	make -C bots all

clean:
	rm -f $(OBJS)
	make -C bots clean

distclean: clean
	rm -f zatacka
	make -C bots distclean

zatacka: $(OBJS) ../common/common.a
	$(CXX) -o zatacka $(OBJS) $(LDFLAGS) $(LDLIBS)

.PHONY: all clean distclean
