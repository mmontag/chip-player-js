#CXX=i686-w64-mingw32-g++ -static
CC=gcc
CXX=g++
CXXLINK=$(CXX)
MKDIR=mkdir -p
RM=rm
RM_F=rm -Rf

#DEBUG=-O0 -fno-inline -D_GLIBCXX_DEBUG -g -fstack-protector-all -fdata-sections

DEBUG=-Ofast -g

#DEBUG += -fno-tree-vectorize

# -march=pentium -mno-sse -mno-sse2 -mno-sse3 -mmmx

CFLAGS += -DLIBADLMIDI_VISIBILITY

CPPFLAGS += -DLIBADLMIDI_VISIBILITY

CPPFLAGS += -std=c++11 -pedantic -Wall -Wextra

include make.rules

