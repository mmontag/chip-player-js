
MAIN_OBJS	= getopt.o getopt1.o options.o main.o
MAIN_DFILES	= Makefile $(OBJS:.o=.c)

src/main/options.o: Version

src/main/xmp: $(OBJS) $(M_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(OBJS) $(M_OBJS) $(LIBS)'; \
	if [ "$(V)" -gt 0 ]; then $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

install-xmp: src/main/xmp
	@echo Installing binaries in $(DESTDIR)$(BINDIR)
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	@$(INSTALL) -s -m755 $+ $(DESTDIR)$(BINDIR)


