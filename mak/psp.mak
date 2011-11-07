CC = psp-gcc
LD = psp-gcc
AR = psp-ar

LFLAGS = 

ifdef DEBUG
CFLAGS = -g -O0 -G0
else
CFLAGS = -g -O2 -G0
endif

