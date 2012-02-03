TEST_PATH	= src/test

include src/test/xmdp/Makefile


$(TEST_PATH)/test1: $(TEST_PATH)/test1.o $(LIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(TEST_PATH)/test1.o $(LIBS) -lpulse-simple -lpulse -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/test3: $(LOBJS) $(TEST_PATH)/test3.o $(D_LOBJS)
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS)

