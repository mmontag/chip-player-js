
MDP_OBJS	= mdp.o font1.o font2.o
MDP_PATH	= src/test/xmdp

xmdp: $(MDP_PATH)/xmdp

$(MDP_PATH)/xmdp: $(addprefix $(MDP_PATH)/,$(MDP_OBJS)) $(TEST_PATH)/alsa.o $(OBJS) $(D_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $+ -lSDL -lasound -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD
