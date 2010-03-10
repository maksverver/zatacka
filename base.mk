CC?=gcc
CFLAGS+=-g -Wall -Wextra -O2 -fPIC
CXX?=g++
CXXFLAGS+=$(CFLAGS)
LDLIBS+=-lm

# For the client:
FLTKCONFIG?=fltk-config
#LIBMIKMODCFG?=libmikmod-config  # optional
