include ../base.mk

FLTKC=fltk-config
CXXFLAGS+=`$(FLTKC) --cxxflags` -I..
LDFLAGS+=`$(FLTKC) --ldstaticflags`
LDLIBS+=../common/common.a
OBJS=	ClientSocket.o Config.o GameModel.o GameView.o KeyWindow.o \
	KeyboardPlayerController.o MainWindow.o MainGameView.o \
	ScoreView.o zatacka.o

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
