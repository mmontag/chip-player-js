
MAIN_OBJS	= getopt.o getopt1.o options.o main.o
MAIN_DFILES	= Makefile $(MAIN_OBJS:.o=.c)
MAIN_PATH	= src/main

dist-main:
	mkdir -p $(DIST)/$(MAIN_PATH)
	cp -dp $(addprefix $(MAIN_PATH)/,$(MAIN_DFILES)) $(DIST)/$(MAIN_PATH)

src/main/options.o: Makefile

M_OBJS	= $(addprefix $(MAIN_PATH)/,$(MAIN_OBJS))
D_OBJS	= $(addprefix $(DRIVERS_PATH)/,$(DRIVERS_OBJS) $(DRIVERS))

src/main/xmp: $(OBJS) $(M_OBJS) $(D_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(OBJS) $(M_OBJS) $(D_OBJS) $(LIBS)'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

install-xmp: src/main/xmp
	@echo Installing xmp in $(DESTDIR)$(BINDIR)
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	@$(INSTALL) -s -m755 $+ $(DESTDIR)$(BINDIR)


# Test

src/main/simple: $(OBJS) src/main/simple.o $(D_OBJS)
	$(LD) -o $@ $(LDFLAGS) $(OBJS) src/main/simple.o $(D_OBJS) $(LIBS)

src/main/simple2: $(OBJS) src/main/simple2.o $(D_OBJS)
	$(LD) -o $@ $(LDFLAGS) $(OBJS) src/main/simple2.o $(D_OBJS) $(LIBS)

