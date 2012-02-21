
TEST_OBJS	= util.o md5.o main.o \
		  test_pp.o test_sqsh.o test_s404.o test_mmcmp.o \
		  test_xmp_get_format_list

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
