
TOPDIR	= ../..
OBJS	= ptpopen.o debug.o usleep.o
DFILES	= Makefile $(OBJS:.o=.c) unistd.h config.h version.c \
	  in_xmp.dsp libxmp.dsp libxmp.dsw osdcomm.h xmp.dsp
DDIRS	= sys
CFILES	= version.c
DCFILES	=

all:

subdist:: version.c

include $(TOPDIR)/Makefile.rules

version.c: $(DFILES) $(TOPDIR)/Makefile.rules
	@echo "Creating version id..."; \
	printf "char *xmp_version = \"$(VERSION)\";\n">$@; \
	printf "char *xmp_date = \"$(DATE)\";\n">>$@; \
        printf "char *xmp_build = NULL;\n">>$@
