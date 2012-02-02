TEST_PATH	= src/test

include src/test/xmdp/Makefile


$(TEST_PATH)/test1: $(OBJS) $(TEST_PATH)/test1.o
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS) -lpulse-simple -lpulse

$(TEST_PATH)/test3: $(LOBJS) $(TEST_PATH)/test3.o $(D_LOBJS)
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS)

