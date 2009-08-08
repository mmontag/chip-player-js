
MDP_OBJS	= mdp.o font1.o font2.o drivers.o
MDP_PATH	= src/test/xmdp

xmdp: $(MDP_PATH)/xmdp

$(MDP_PATH)/xmdp: $(addprefix $(MDP_PATH)/,$(MDP_OBJS)) $(OBJS) $(D_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $+ -lSDL $(LIBS)'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD
