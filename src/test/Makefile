TEST_PATH	= src/test

include src/test/xmdp/Makefile

$(TEST_PATH)/test1: $(TEST_PATH)/test1.o $(TEST_PATH)/alsa.o $(LIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(TEST_PATH)/test1.o $(TEST_PATH)/alsa.o $(LIBS) -lasound -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/test2: $(TEST_PATH)/test2.o $(TEST_PATH)/alsa.o $(LIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(TEST_PATH)/test2.o $(TEST_PATH)/alsa.o $(LIBS) -lasound -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/test3: $(LOBJS) $(TEST_PATH)/test3.o $(D_LOBJS)
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS)

