#
# Makefile
#

ifeq ($(OS),Windows_NT)
W32 = 1
export W32
endif

ifeq ($(W32),1)

include mak/w32.mak
EXE_SFX = .exe

else

include mak/general.mak

endif

ifeq ($(DEBUG),1)
MODE = _debug
endif

TARGET = mdxplay$(MODE)$(EXE_SFX)

CFLAGS += -Isrc

LIBS += $(SDL_LIBS)
SLIBS += $(SDL_SLIBS)
CFLAGS += $(SDL_CFLAGS)


#
# definations for packing
#
TITLE = mdxmini

FILES = Makefile Makefile.lib Makefile.psplib $(TITLE).txt 
FILES += fade.h sdlplay.c 
FILES += src/nlg.c src/nlg.h
FILES_ORG = COPYING AUTHORS
LIB = $(OBJDIR)/lib$(TITLE).a

LIBS += $(LIB)

ZIPSRC = $(TITLE)`date +"%y%m%d"`.zip
TOUCH = touch -t `date +"%m%d0000"`


MDXPLAY_OBJS = sdlplay.o nlg.o 
OBJS = $(addprefix $(OBJDIR)/,$(MDXPLAY_OBJS))

SRCDIR = src
MAKDIR = mak

MKLIB = mklib

NLGTEST = nlgtest

#
#
#

all : $(OBJDIR) $(MKLIB) $(TARGET)

$(OBJDIR):
	mkdir $(OBJDIR)

$(TARGET) : $(OBJS) $(LIB)
	$(LD) $(LFLAGS) -o $@ $(OBJS) $(LIBS)

$(LIB): $(MKLIB)

$(MKLIB):
	make -f Makefile.lib

$(OBJDIR)/nlg.o: src/nlg.c
	$(CC) -o $@ $< -c $(CFLAGS)

$(OBJDIR)/sdlplay.o: sdlplay.c
	$(CC) -o $@ $< -c $(CFLAGS)

clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET) $(NLGTEST)
	
release:
	strip $(TARGET)

test:
	gcc -o $(NLGTEST) src/nlg.c -DNLG_TEST

dist :
	find . -name ".DS_Store" -exec rm -f {} \;
	find $(FILES) $(SRCDIR) $(MAKDIR) -exec $(TOUCH) {} \;
	
	rm -f $(ZIPSRC)
	zip -r $(ZIPSRC) $(MAKDIR) $(SRCDIR) $(FILES) $(FILES_ORG)

	
