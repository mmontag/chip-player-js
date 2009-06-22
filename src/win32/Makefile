
TOPDIR	= ../..
OBJS	= ptpopen.o debug.o usleep.o
DFILES	= Makefile $(OBJS:.o=.c) unistd.h config.h version.c ptpopen.h \
	  in_xmp.dsp libxmp.dsp libxmp.dsw osdcomm.h xmp.dsp
DDIRS	= sys
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	= version.c
DCFILES	=

all: $(LIB)

subdist:: version.c

include $(TOPDIR)/Makefile.rules

version.c: $(DFILES) $(TOPDIR)/Makefile.rules
	@echo "Creating version id..."; \
	printf "char *xmp_version = \"$(VERSION)\";\n">$@; \
	printf "char *xmp_date = \"$(DATE)\";\n">>$@; \
        printf "char *xmp_build = 0;\n">>$@
