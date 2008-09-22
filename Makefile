FLTKC=fltk/bin/fltk-config
CXXFLAGS=-g -Wall -Wextra -ansi `$(FLTKC) --cxxflags`
LDFLAGS=`$(FLTKC) --ldstaticflags`
OBJS=GameView.o ClientSocket.o Debug.o

all: zatacka zatacka-server

zatacka-server: $(OBJS) zatacka-server.o
	$(CXX) -o zatacka-server $(OBJS) zatacka-server.o $(LDFLAGS)

zatacka: $(OBJS) zatacka.o
	$(CXX) -o zatacka $(OBJS) zatacka.o $(LDFLAGS)

clean:
	rm -f $(OBJS) zatacka.o zatacka-server.o

distclean: clean
	rm -f zatacka zatacka-server

.PHONY: all clean distclean
