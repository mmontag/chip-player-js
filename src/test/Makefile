TEST_PATH	= src/test

include src/test/xmdp/Makefile

$(TEST_PATH)/test1: $(TEST_PATH)/test1.o $(TEST_PATH)/alsa.o $(LIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(TEST_PATH)/test1.o $(TEST_PATH)/alsa.o -lasound -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/test2: $(TEST_PATH)/test2.o $(TEST_PATH)/alsa.o $(LIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(TEST_PATH)/test2.o $(TEST_PATH)/alsa.o -lasound -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/test3: $(TEST_PATH)/test3.o $(LIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(TEST_PATH)/test3.o -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/test4: $(TEST_PATH)/test4.o $(LIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(TEST_PATH)/test4.o -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

