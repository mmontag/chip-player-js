CFLAGS = -c -fPIC

OBJS = psflib.o psf2fs.o 


OPTS = -O2

all: libpsflib.a

libpsflib.a : $(OBJS)
	$(AR) rcs $@ $^

.c.o:
	$(CC) $(CFLAGS) $(OPTS) $*.c

clean:
	rm -f $(OBJS) libpsflib.a > /dev/null

