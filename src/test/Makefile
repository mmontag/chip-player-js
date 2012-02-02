TEST_PATH	= src/test

include src/test/xmdp/Makefile


$(TEST_PATH)/test1: $(OBJS) $(TEST_PATH)/test1.o
	@CMD='$(LD) -o $@ $(LDFLAGS) $+ $(LIBS) -lpulse-simple -lpulse'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/test3: $(LOBJS) $(TEST_PATH)/test3.o $(D_LOBJS)
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS)

