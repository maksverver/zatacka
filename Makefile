CXXFLAGS=-g -Wall -Wextra -ansi `fltk-config --cxxflags`
LDFLAGS=`fltk-config --ldflags`
OBJS=GameView.o ClientSocket.o Debug.o

all: zatacka zatacka-server

zatacka-server: $(OBJS) zatacka-server.o
	$(CXX) $(LDFLAGS) -o zatacka-server $(OBJS) zatacka-server.o

zatacka: $(OBJS) zatacka.o
	$(CXX) $(LDFLAGS) -o zatacka $(OBJS) zatacka.o

clean:
	rm -f $(OBJS) zatacka.o zata-server.o

distclean: clean
	rm -f zatacka

.PHONY: all clean distclean
