#
# Makefile
#


include mak/general.mak
CFLAGS += -Isrc

SDL_LIBS = `sdl-config --libs`
SDL_CFLAGS = `sdl-config --cflags`

#
# definations for packing
#
TITLE = mdxmini
TARGET = mdxplay

FILES = Makefile Makefile.lib Makefile.psplib $(TITLE).txt 
FILES += fade.h sdlplay.c
FILES_ORG = COPYING AUTHORS
LIB = lib$(TITLE).a


ZIPSRC = $(TITLE)`date +"%y%m%d"`.zip
TOUCH = touch -t `date +"%m%d0000"`


OBJS = sdlplay.o $(LIB)
SRCDIR = src
MAKDIR = mak


all : $(TARGET)

$(TARGET) : $(OBJS)
	$(LD) $(LFLAGS) -o $@ $(OBJS) $(SDL_LIBS)

$(LIB):
	make -f Makefile.lib

%.o : %.c
	$(CC) -o $@ $< -c $(CFLAGS) $(SDL_CFLAGS) 

clean :
	make -f Makefile.lib clean
	rm -f $(TARGET) $(OBJS)

dist :
	find . -name ".DS_Store" -exec rm -f {} \;
	find $(FILES) $(SRCDIR) $(MAKDIR) -exec $(TOUCH) {} \;
	
	rm -f $(ZIPSRC)
	zip -r $(ZIPSRC) $(MAKDIR) $(SRCDIR) $(FILES) $(FILES_ORG)

	
