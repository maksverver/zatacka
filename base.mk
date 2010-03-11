-include $(TOP)/local.mk

CC?=gcc
CFLAGS+=-g -Wall -Wextra -O2 -fPIC
CXX?=g++
CXXFLAGS+=$(CFLAGS)
AR?=ar
LDLIBS+=-lm

# For the client:
FLTKCONFIG?=fltk-config
# optional: LIBMIKMODCONFIG?=libmikmod-config
