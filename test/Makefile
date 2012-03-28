
REPLAYERS	= mod ft2 st3 it
QUIRKS		= 
CONVERTERS	= delta signal endian
DEPACKERS	= pp sqsh s404 mmcmp zip zip_filtered arcfs gzip compress \
		  arc spark j2b lzx it_sample_8bit it_sample_16bit
EFFECTS		= delay
API_CALLS	= get_format_list create_context
STORLEK		= arpeggio_pitch_slide arpeggio_no_value compatible_gxx_off \
		  compatible_gxx_on gxx_fine_slides_memory \
		  volume_column_and_fine_slides note_cut_with_sample \
		  out_of_range_delays sample_change_no_note pattern_loop \
		  infinite_pattern_loop \
		  sample_number_new_note \
		  zero_cut_and_delay

CASE1_TESTS	= $(addprefix new_note_no_ins_,$(REPLAYERS)) \
		  $(addprefix new_note_same_ins_,$(REPLAYERS)) \
		  $(addprefix new_note_valid_ins_,$(REPLAYERS)) \
		  $(addprefix new_note_invalid_ins_,$(REPLAYERS))

CASE2_TESTS	= $(addprefix no_note_same_ins_,$(REPLAYERS)) \
		  $(addprefix no_note_valid_ins_,$(REPLAYERS)) \
		  $(addprefix no_note_invalid_ins_,$(REPLAYERS))

CASE3_TESTS	= $(addprefix porta_no_ins_,$(REPLAYERS)) \
		  $(addprefix porta_same_ins_,$(REPLAYERS)) \
		  $(addprefix porta_valid_ins_,$(REPLAYERS)) \
		  $(addprefix porta_invalid_ins_,$(REPLAYERS))

QUIRK_TESTS	= $(addprefix quirk_,$(QUIRKS)) \
		  $(addprefix no_quirk_,$(QUIRKS))

CONVERT_TESTS	= $(addprefix convert_,$(CONVERTERS))

DEPACK_TESTS	= $(addprefix depack_,$(DEPACKERS))

EFFECT_TESTS	= $(addprefix effect_,$(EFFECTS))

NNA_TESTS	= nna_cut nna_cont nna_off nna_fade dct_note

SEQUENCER_TESTS	= prev_order_start prev_order_skip prev_order_start_seq \
		  next_order_skip

STORLEK_TESTS	= $(addprefix storlek_,$(STORLEK))

API_TESTS	= $(addprefix api_,$(API_CALLS))

TESTS		= $(CONVERT_TESTS) \
		  $(DEPACK_TESTS) \
		  load_sample_8bit load_sample_16bit \
		  player_read_event \
		  $(QUIRK_TESTS) \
		  $(CASE1_TESTS) \
		  $(CASE2_TESTS) \
		  $(CASE3_TESTS) \
		  $(NNA_TESTS) \
		  $(EFFECT_TESTS) \
		  $(SEQUENCER_TESTS) \
		  $(STORLEK_TESTS) \
		  $(API_TESTS)

TEST_NAMES	= $(addprefix test_,$(TESTS))

TEST_OBJS	= util.o md5.o main.o simple_module.o \
		  $(TEST_NAMES:=.o)

TEST_DFILES	= Makefile $(TEST_OBJS:.o=.c) test.h md5.h data

TEST_PATH	= test

T_OBJS 		= $(addprefix $(TEST_PATH)/,$(TEST_OBJS))

default-test:
	$(MAKE) -C.. check

dist-test:
	mkdir -p $(DIST)/$(TEST_PATH)
	cp -RPp $(addprefix $(TEST_PATH)/,$(TEST_DFILES)) $(DIST)/$(TEST_PATH)

check: $(TEST_PATH)/libxmp-tests
	cd $(TEST_PATH); LD_LIBRARY_PATH=../lib DYLD_LIBRARY_PATH=../lib LIBRARY_PATH=../lib:$$LIBRARY_PATH ./libxmp-tests

$(TEST_PATH)/libxmp-tests: $(T_OBJS)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(T_OBJS) -Llib -lxmp'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/main.o: $(TEST_PATH)/main.c $(TEST_PATH)/all_tests.c $(TEST_PATH)/test.h $(TEST_PATH)/all_tests.c

$(TEST_PATH)/test.h $(TEST_PATH)/main.c: $(TEST_PATH)/all_tests.c

$(TEST_PATH)/all_tests.c: $(TEST_PATH)/Makefile
	@echo > $@; \
	for i in $(TEST_NAMES); do \
		echo "declare_test($$i);" >> $@; \
	done
