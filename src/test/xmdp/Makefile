
MDP_OBJS	= mdp.o font1.o font2.o drivers.o
MDP_PATH	= src/test/xmdp

$(MDP_PATH)/xmdp: $(addprefix $(MDP_PATH)/,$(MDP_OBJS)) $(OBJS) $(D_OBJS)
	$(LD) -o $@ $+ -lXext -lX11 $(LIBS)
