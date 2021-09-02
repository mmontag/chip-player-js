#include <catch.hpp>
#include <random>
#include "adlmidi.h"
#include "adlmidi_cvt.hpp"

static std::mt19937 rng;

static ADL_Instrument random_instrument()
{
    ADL_Instrument ins;
    ins.version = ADLMIDI_InstrumentVersion;
    ins.note_offset1 = std::uniform_int_distribution<int>(-128, 127)(rng);
    ins.note_offset2 = std::uniform_int_distribution<int>(-128, 127)(rng);
    ins.midi_velocity_offset = std::uniform_int_distribution<int>(-128, 127)(rng);
    ins.second_voice_detune = std::uniform_int_distribution<int>(-128, 127)(rng);
    ins.second_voice_detune -= ins.second_voice_detune % 2; // Make sure number is even
    ins.percussion_key_number = std::uniform_int_distribution<unsigned>(0, 127)(rng);
    ins.inst_flags =
        std::uniform_int_distribution<unsigned>(0, 255)(rng) &
        (ADLMIDI_Ins_IsBlank|ADLMIDI_Ins_RhythmModeMask);
    switch (std::uniform_int_distribution<unsigned>(0, 3)(rng)) {
    case 0:  // 2op
        ins.inst_flags |= ADLMIDI_Ins_2op;
        break;
    case 1:  // pseudo 4op
        ins.inst_flags |= ADLMIDI_Ins_Pseudo4op;
        // fall through
    case 2:  // real 4op
        ins.inst_flags |= ADLMIDI_Ins_4op;
        break;
    }
    ins.fb_conn1_C0 = std::uniform_int_distribution<unsigned>(0, 255)(rng);
    ins.fb_conn2_C0 = std::uniform_int_distribution<unsigned>(0, 255)(rng);
    ins.delay_on_ms = std::uniform_int_distribution<unsigned>(0, 40000)(rng);
    ins.delay_off_ms = std::uniform_int_distribution<unsigned>(0, 40000)(rng);
    for (unsigned op = 0; op < 4; ++op) {
        ins.operators[op].avekf_20 = std::uniform_int_distribution<unsigned>(0, 255)(rng);
        ins.operators[op].ksl_l_40 = std::uniform_int_distribution<unsigned>(0, 255)(rng);
        ins.operators[op].atdec_60 = std::uniform_int_distribution<unsigned>(0, 255)(rng);
        ins.operators[op].susrel_80 = std::uniform_int_distribution<unsigned>(0, 255)(rng);
        ins.operators[op].waveform_E0 = std::uniform_int_distribution<unsigned>(0, 255)(rng);
    }
    return ins;
}

static void check_instrument_equality(const ADL_Instrument &a, const ADL_Instrument &b)
{
    REQUIRE((int)a.note_offset1 == (int)b.note_offset1);
    REQUIRE((int)a.note_offset2 == (int)b.note_offset2);
    REQUIRE((int)a.midi_velocity_offset == (int)b.midi_velocity_offset);
    REQUIRE((int)a.second_voice_detune == (int)b.second_voice_detune);
    REQUIRE((int)a.percussion_key_number == (int)b.percussion_key_number);
    REQUIRE((int)a.inst_flags == (int)b.inst_flags);
    REQUIRE((int)a.fb_conn1_C0 == (int)b.fb_conn1_C0);
    REQUIRE((int)a.fb_conn2_C0 == (int)b.fb_conn2_C0);
    REQUIRE((int)a.delay_on_ms == (int)b.delay_on_ms);
    REQUIRE((int)a.delay_off_ms == (int)b.delay_off_ms);
    for (unsigned op = 0; op < 4; ++op) {
        REQUIRE((int)a.operators[op].avekf_20 == (int)b.operators[op].avekf_20);
        REQUIRE((int)a.operators[op].ksl_l_40 == (int)b.operators[op].ksl_l_40);
        REQUIRE((int)a.operators[op].atdec_60 == (int)b.operators[op].atdec_60);
        REQUIRE((int)a.operators[op].susrel_80 == (int)b.operators[op].susrel_80);
        REQUIRE((int)a.operators[op].waveform_E0 == (int)b.operators[op].waveform_E0);
    }
}

TEST_CASE("[Conversion] Main")
{
    rng.seed(777);

    for (unsigned i = 0; i < 1000000; ++i) {
        ADL_Instrument adl_ins = random_instrument();

        OplInstMeta internal_ins;
        cvt_generic_to_FMIns(internal_ins, adl_ins);

        ADL_Instrument adl_ins2;
        cvt_FMIns_to_generic(adl_ins2, internal_ins);

        check_instrument_equality(adl_ins, adl_ins2);
    }
}
