CC = gcc
LD = gcc
AR = ar

LFLAGS = 

ifdef DEBUG
CFLAGS = -g -O0
else
CFLAGS = -g -O3
endif
