
MAIN_QUIRKS	= mod ft2 st3 it
QUIRKS		= old_ins_vol ignore_invalid_ins
DEPACKERS	= pp sqsh s404 mmcmp
CONVERTERS	= delta signal endian
EFFECTS		= delay
API_CALLS	= get_format_list create_context

CASE1_TESTS	= $(addprefix new_note_same_ins_,$(MAIN_QUIRKS))

CASE2_TESTS	= $(addprefix no_note_same_ins_,$(MAIN_QUIRKS)) \
		  $(addprefix no_note_valid_ins_,$(MAIN_QUIRKS)) \
		  $(addprefix no_note_invalid_ins_,$(MAIN_QUIRKS))

QUIRK_TESTS	= $(addprefix quirk_,$(QUIRKS)) \
		  $(addprefix no_quirk_,$(QUIRKS))

DEPACK_TESTS	= $(addprefix depack_,$(DEPACKERS))

CONVERT_TESTS	= $(addprefix convert_,$(CONVERTERS))

EFFECT_TESTS	= $(addprefix effect_,$(EFFECTS))

API_TESTS	= $(addprefix api_,$(API_CALLS))

TESTS		= $(DEPACK_TESTS) \
		  $(CONVERT_TESTS) \
		  load_sample_8bit load_sample_16bit \
		  player_read_event \
		  $(QUIRK_TESTS) \
		  $(CASE1_TESTS) \
		  $(CASE2_TESTS) \
		  $(EFFECTS_TESTS) \
		  $(API_TESTS)

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
