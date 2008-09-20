CXXFLAGS=-g -Wall -Wextra -ansi `fltk-config --cxxflags`
LDFLAGS=`fltk-config --ldflags`
OBJS=main.o GameView.o ClientSocket.o Debug.o

all: zatacka

zatacka: $(OBJS)
	$(CXX) $(LDFLAGS) -o zatacka $(OBJS)

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f zatacka

.PHONY: all clean distclean
