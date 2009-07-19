
V = 0

all: binaries

include Makefile.rules
include src/drivers/Makefile
include src/loaders/Makefile
include src/loaders/prowizard/Makefile
include src/misc/Makefile
include src/player/Makefile
OBJS += $(addprefix src/drivers/, $(DRIVERS_OBJS))
OBJS += $(addprefix src/loaders/, $(LOADERS_OBJS))
OBJS += $(addprefix src/loaders/prowizard/, $(PROWIZ_OBJS))
OBJS += $(addprefix src/misc/, $(MISC_OBJS))
OBJS += $(addprefix src/player/, $(PLAYER_OBJS))

ifneq ($(PLUGINS),)
LOBJS = $(OBJS:.o=.lo)
endif

include src/main/Makefile
M_OBJS += $(addprefix src/main/, $(MAIN_OBJS))

include src/plugin/Makefile

XCFLAGS = -Isrc/include -DSYSCONFDIR=\"$(SYSCONFDIR)\" -DVERSION=\"$(VERSION)\"

.SUFFIXES: .c .o .lo .a .so .dll

	    #echo $(CC) $(CFLAGS) $(XCFLAGS) -o $*.o $< ; \
	#@$(CC) $(CFLAGS) $(XCFLAGS) -o $*.o $<

.c.o:
	@CMD='$(CC) $(CFLAGS) $(XCFLAGS) -o $*.o $<'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $*.o ; fi; \
	eval $$CMD

.c.lo:
	@CMD='$(CC) $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) -o $*.lo $<'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $*.lo ; fi; \
	eval $$CMD

binaries: src/main/xmp $(PLUGINS)

src/main/xmp: $(OBJS) $(M_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(OBJS) $(M_OBJS) $(LIBS)'; \
	if [ "$(V)" -gt 0 ]; then $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

clean:
	@rm -f $(OBJS) $(OBJS:.o=.lo) $(M_OBJS)

depend:
	@echo Building dependencies...
	@$(CC) $(CFLAGS) $(XCFLAGS) -MM -MG $(OBJS:.o=.c) >$@

$(OBJS): Makefile.rules

$(LOBJS): Makefile.rules

include depend

