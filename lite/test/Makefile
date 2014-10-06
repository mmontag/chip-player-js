
TEST_OBJS	= test.o md5.o
TEST_DFILES	= Makefile $(TEST_OBJS:.o=.c) test.it md5.h
TEST_PATH	= test

T_OBJS = $(addprefix $(TEST_PATH)/,$(TEST_OBJS))

default:
	$(MAKE) -C .. check

dist-test:
	mkdir -p $(DIST)/$(TEST_PATH)
	cp -RPp $(addprefix $(TEST_PATH)/,$(TEST_DFILES)) $(DIST)/$(TEST_PATH)

check: $(TEST_PATH)/libxmp-test
	cd $(TEST_PATH); LD_LIBRARY_PATH=../lib DYLD_LIBRARY_PATH=../lib LIBRARY_PATH=../lib:$$LIBRARY_PATH PATH=$$PATH:../lib ./libxmp-test

$(TEST_PATH)/libxmp-test: $(T_OBJS)
	@CMD='$(LD) -o $@ $(T_OBJS) $(LIBS) -Llib -lxmp-lite'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

