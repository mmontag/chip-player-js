
MDP_OBJS	= mdp.o font1.o font2.o
MDP_PATH	= examples/xmdp

xmdp: $(MDP_PATH)/xmdp

$(MDP_PATH)/xmdp: $(addprefix $(MDP_PATH)/,$(MDP_OBJS)) $(EXAMPLES_PATH)/alsa.o
	@CMD='$(LD) -o $@ $(LDFLAGS) $+ -lSDL -lasound -Llib -lxmp -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD
