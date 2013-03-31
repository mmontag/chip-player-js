
REPLAYERS	= mod ft2 st3 it

QUIRKS		= 

SMPLOADERS	= delta signal endian

DEPACKERS	= pp sqsh s404 mmcmp zip zip_filtered zip_store arcfs \
		  gzip compress arc_method2 arc_method8 \
		  spark j2b lzx bzip2 xz lha_l0_lzhuff1 lha_l0_lzhuff5 \
		  lha_l1_lzhuff5 lha_l1_lzhuff6 lha_l1_lzhuff7 zoo \
		  vorbis it_sample_8bit it_sample_16bit

PROWIZARD	= zen fuchs starpack

EFFECTS		= 0_arpeggio 1_slide_up 2_slide_down a_volslide \
		  c_volset \
		  ed_delay ef_invert_loop \
		  f_set_speed \
		  okt_arpeggio per_slide per_toneporta setpan it_panslide \
		  panbrello globalvol gvol_slide s3m_bpm it_bpm \
		  note_slide_up

API		= get_format_list create_context test_module load_module \
		  set_player stop_module restart_module seek_time \
		  channel_mute channel_vol inject_event

STORLEK		= 01_arpeggio_pitch_slide \
		  02_arpeggio_no_value \
		  03_compatible_gxx_off \
		  04_compatible_gxx_on \
		  05_gxx_fine_slides_memory \
		  06_volume_column_and_fine_slides \
		  07_note_cut_with_sample \
		  08_out_of_range_delays \
		  09_sample_change_no_note \
		  10_pattern_loop \
		  11_infinite_pattern_loop \
		  12_tremor \
		  13_tremor_with_old_effects \
		  14_pingpong_loop_and_sample_number \
		  15_retrigger \
		  16_global_volume \
		  17_pattern_row_delay \
		  18_sample_number_new_note \
		  22_zero_cut_and_delay \
		  23_portamento_with_no_note \
		  24_short_envelope_loops \
		  25_portamento_and_pitch_slide

MIXER		= interpolation_default \
		  mono_8bit_nearest mono_16bit_nearest \
		  stereo_8bit_nearest stereo_16bit_nearest \
		  mono_8bit_linear mono_16bit_linear \
		  stereo_8bit_linear stereo_16bit_linear \
		  mono_8bit_linear_filter mono_16bit_linear_filter \
		  stereo_8bit_linear_filter stereo_16bit_linear_filter \
		  mono_8bit_spline mono_16bit_spline \
		  stereo_8bit_spline stereo_16bit_spline \
		  mono_8bit_spline_filter mono_16bit_spline_filter \
		  stereo_8bit_spline_filter stereo_16bit_spline_filter \
		  downmix_8bit downmix_16bit

READ		= file_32bit_little_endian file_32bit_big_endian \
		  file_24bit_little_endian file_24bit_big_endian \
		  file_16bit_little_endian file_16bit_big_endian \
		  file_8bit \
		  mem_32bit_little_endian mem_32bit_big_endian \
		  mem_16bit_little_endian mem_16bit_big_endian

WRITE		= file_32bit_little_endian file_32bit_big_endian \
		  file_16bit_little_endian file_16bit_big_endian \
		  file_8bit file_move_data

PLAYER		= read_event scan med_synth period_amiga period_mod_range \
		  note_off_ft2 note_off_it \
		  nna_cut nna_cont nna_off nna_fade dct_note \
		  it_keyoff

SYNTH		= adlib spectrum

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

SMPLOAD_TESTS	= $(addprefix sample_load_,$(SMPLOADERS))

DEPACK_TESTS	= $(addprefix depack_,$(DEPACKERS))

PROWIZARD_TESTS	= $(addprefix prowizard_,$(PROWIZARD))

EFFECT_TESTS	= $(addprefix effect_,$(EFFECTS))

SEQUENCER_TESTS	= prev_order_start prev_order_skip prev_order_start_seq \
		  next_order_skip

STORLEK_TESTS	= $(addprefix storlek_,$(STORLEK))

MIXER_TESTS	= $(addprefix mixer_,$(MIXER))

READ_TESTS	= $(addprefix read_,$(READ))

WRITE_TESTS	= $(addprefix write_,$(WRITE))

PLAYER_TESTS	= $(addprefix player_,$(PLAYER))

SYNTH_TESTS	= $(addprefix synth_,$(SYNTH))

API_TESTS	= $(addprefix api_,$(API))

TESTS		= $(READ_TESTS) \
		  $(WRITE_TESTS) \
		  $(SMPLOAD_TESTS) \
		  $(DEPACK_TESTS) \
		  $(PROWIZARD_TESTS) \
		  load_sample_8bit load_sample_16bit \
		  string_adjustment \
		  $(API_TESTS) \
		  $(QUIRK_TESTS) \
		  $(CASE1_TESTS) \
		  $(CASE2_TESTS) \
		  $(CASE3_TESTS) \
		  $(NNA_TESTS) \
		  $(EFFECT_TESTS) \
		  $(SEQUENCER_TESTS) \
		  $(STORLEK_TESTS) \
		  $(PLAYER_TESTS) \
		  $(MIXER_TESTS) \
		  $(SYNTH_TESTS)

TEST_NAMES	= $(addprefix test_,$(TESTS))

TEST_OBJS	= util.o md5.o main.o simple_module.o \
		  $(TEST_NAMES:=.o)

TEST_DFILES	= Makefile $(TEST_OBJS:.o=.c) test.h md5.h data

TEST_PATH	= test

TEST_INTERNAL	= load_helpers.o depackers/s404_dec.o loaders/itsex.o \
		  dataio.o scan.o misc.o loaders/sample.o synth_null.o \
		  fnmatch.o

T_OBJS 		= $(addprefix $(TEST_PATH)/,$(TEST_OBJS)) \
		  $(addprefix $(SRC_PATH)/,$(TEST_INTERNAL))

GCT_OBJS 	= $(addprefix $(TEST_PATH)/,$(TEST_OBJS))

default-test:
	$(MAKE) -C.. check

dist-test:
	mkdir -p $(DIST)/$(TEST_PATH)
	cp -RPp $(addprefix $(TEST_PATH)/,$(TEST_DFILES)) $(DIST)/$(TEST_PATH)

#
# Run standard tests
#

check: $(TEST_PATH)/all_tests.c $(TEST_PATH)/libxmp-tests
	cd $(TEST_PATH); LD_LIBRARY_PATH=../lib DYLD_LIBRARY_PATH=../lib LIBRARY_PATH=../lib:$$LIBRARY_PATH PATH=$$PATH:../lib ./libxmp-tests

$(TEST_PATH)/libxmp-tests: $(T_OBJS)
	@CMD='$(LD) -o $@ $(T_OBJS) -lm -Llib -lxmp'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

#
# Run coverage test
#

covercheck: coverclean $(TEST_PATH)/all_tests.c $(TEST_PATH)/libxmp-covertest
	cd $(TEST_PATH); LD_LIBRARY_PATH=../lib DYLD_LIBRARY_PATH=../lib LIBRARY_PATH=../lib:$$LIBRARY_PATH ./libxmp-covertest ||:
	lcov -c -b . -d . -k src -k include -o libxmp.cov
	rm -Rf coverage
	genhtml -o coverage libxmp.cov

coverclean:
	@rm -f $(GCOBJS) $(OBJS:.o=.gcno) $(OBJS:.o=.gcda)

$(TEST_PATH)/libxmp-covertest: $(GCT_OBJS) lib/$(GCLIB)
	@CMD='$(LD) -o $@ $(LDFLAGS) $(GCT_OBJS) lib/$(GCLIB) -lgcov -lm'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(TEST_PATH)/main.o: $(TEST_PATH)/main.c $(TEST_PATH)/all_tests.c $(TEST_PATH)/test.h $(TEST_PATH)/all_tests.c

$(TEST_PATH)/test.h $(TEST_PATH)/main.c: $(TEST_PATH)/all_tests.c

$(TEST_PATH)/all_tests.c: $(TEST_PATH)/Makefile
	@echo > $@; \
	for i in $(TEST_NAMES); do \
		echo "declare_test($$i);" >> $@; \
	done
