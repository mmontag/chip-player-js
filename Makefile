
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

.SUFFIXES: .c .o .lo .a .S .so

.c.o:
	@if [ "$(V)" -gt 0 ]; then \
	    echo $(CC) $(CFLAGS) $(XCFLAGS) -o $*.o $< ; \
	else echo CC $< ; fi
	@$(CC) $(CFLAGS) $(XCFLAGS) -o $*.o $<

.c.lo:
	@if [ "$(V)" -gt 0 ]; then \
	    echo $(CC) $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) -o $*.lo $< ; \
	else echo "CC(fPIC)" $< ; fi
	@$(CC) $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) -o $*.lo $<

binaries: src/main/xmp $(PLUGINS)

src/main/xmp: $(OBJS) $(M_OBJS)
	@if [ "$(V)" -gt 0 ]; then \
	    echo $(LD) -o $@ $(LDFLAGS) $(OBJS) $(M_OBJS) $(LIBS); \
	else echo LD $@ ; fi
	@$(LD) -o $@ $(LDFLAGS) $(OBJS) $(M_OBJS) $(LIBS)

audacious: src/plugin/plugin-audacious.so

src/plugin/plugin-audacious.so: $(LOBJS) src/plugin/audacious.lo
	@if [ "$(V)" -gt 0 ]; then \
	    echo $(LD) -shared -o $@ $(OBJS) src/plugin/audacious.lo `pkg-config --libs audacious`; \
	else echo LD $@ ; fi
	@$(LD) -shared -o $@ $(OBJS) src/plugin/audacious.lo `pkg-config --libs audacious`

clean:
	@rm -f $(OBJS) $(OBJS:.o=.lo) $(M_OBJS)

depend:
	@echo Building dependencies...
	@$(CC) $(CFLAGS) $(XCFLAGS) -MM -MG $(OBJS:.o=.c) >$@

$(OBJS): Makefile.rules

include depend

