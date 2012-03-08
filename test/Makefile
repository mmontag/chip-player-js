
TESTS		= depack_pp depack_sqsh depack_s404 depack_mmcmp \
		  convert_delta convert_signal convert_endian \
		  load_sample_8bit load_sample_16bit \
		  player_read_event player_valid_instrument \
		  effect_delay \
		  api_get_format_list api_create_context

TEST_NAMES	= $(addprefix test_,$(TESTS))

TEST_OBJS	= util.o md5.o main.o simple_module.o \
		  $(TEST_NAMES:=.o)

TEST_DFILES	= Makefile $(TEST_OBJS:.o=.c) test.h md5.h

TEST_PATH	= test

T_OBJS 		= $(addprefix $(TEST_PATH)/,$(TEST_OBJS))

default-test:
	$(MAKE) -C.. check

dist-test:
	mkdir -p $(DIST)/$(TEST_PATH)
	cp -RPp $(addprefix $(TEST_PATH)/,$(TEST_DFILES)) $(DIST)/$(TEST_PATH)

check: $(TEST_PATH)/libxmp-tests
	cd $(TEST_PATH); ./libxmp-tests

$(TEST_PATH)/libxmp-tests: $(T_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(T_OBJS) -Llib -lxmp'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/main.o: $(TEST_PATH)/main.c $(TEST_PATH)/all_tests.c $(TEST_PATH)/test.h $(TEST_PATH)/all_tests.c

$(T_OBJS): $(TEST_PATH)/all_tests.c

$(TEST_PATH)/all_tests.c: $(TEST_PATH)/Makefile
	@echo > $@; \
	for i in $(TEST_NAMES); do \
		echo "declare_test($$i);" >> $@; \
	done
