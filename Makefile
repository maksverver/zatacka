FLTKC=fltk/bin/fltk-config
CFLAGS=-g -Wall -Wextra -std=c99 -O2
CXXFLAGS=-g -Wall -Wextra -ansi `$(FLTKC) --cxxflags`
LDFLAGS=`$(FLTKC) --ldstaticflags`
OBJS=ClientSocket.o Config.o Debug.o GameModel.o GameView.o ScoreView.o

all: zatacka zatacka-server

zatacka-server: Debug.c zatacka-server.c
	$(CC) $(CFLAGS) -o zatacka-server Debug.c zatacka-server.c -lm

zatacka: $(OBJS) zatacka.o
	$(CXX) -o zatacka $(OBJS) zatacka.o $(LDFLAGS)

clean:
	rm -f $(OBJS) zatacka.o zatacka-server.o

distclean: clean
	rm -f zatacka zatacka-server

.PHONY: all clean distclean
