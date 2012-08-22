EXAMPLES_PATH	= examples

include examples/xmdp/Makefile

examples: $(EXAMPLES_PATH)/test1 $(EXAMPLES_PATH)/test2 $(EXAMPLES_PATH)/test3 $(EXAMPLES_PATH)/test4 $(EXAMPLES_PATH)/xmdp/xmdp

$(EXAMPLES_PATH)/test1: $(EXAMPLES_PATH)/test1.o $(EXAMPLES_PATH)/alsa.o
	@CMD='$(LD) -o $@ $(LDFLAGS) $(EXAMPLES_PATH)/test1.o $(EXAMPLES_PATH)/alsa.o -lasound -Llib -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(EXAMPLES_PATH)/test2: $(EXAMPLES_PATH)/test2.o $(EXAMPLES_PATH)/alsa.o
	@CMD='$(LD) -o $@ $(LDFLAGS) $(EXAMPLES_PATH)/test2.o $(EXAMPLES_PATH)/alsa.o -lasound -Llib -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(EXAMPLES_PATH)/test3: $(EXAMPLES_PATH)/test3.o
	@CMD='$(LD) -o $@ $(LDFLAGS) $(EXAMPLES_PATH)/test3.o -Llib -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(EXAMPLES_PATH)/test4: $(EXAMPLES_PATH)/test4.o
	@CMD='$(LD) -o $@ $(LDFLAGS) $(EXAMPLES_PATH)/test4.o -Llib -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

