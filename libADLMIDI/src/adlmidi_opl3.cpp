/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2020 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "adlmidi_opl3.hpp"
#include "adlmidi_private.hpp"
#include <stdlib.h>
#include <cassert>

#ifndef DISABLE_EMBEDDED_BANKS
#include "wopl/wopl_file.h"
#endif

#ifdef ADLMIDI_HW_OPL
static const unsigned OPLBase = 0x388;
#else
#   if defined(ADLMIDI_DISABLE_NUKED_EMULATOR) && \
       defined(ADLMIDI_DISABLE_DOSBOX_EMULATOR) && \
       defined(ADLMIDI_DISABLE_OPAL_EMULATOR) && \
       defined(ADLMIDI_DISABLE_JAVA_EMULATOR)
#       error "No emulators enabled. You must enable at least one emulator to use this library!"
#   endif

// Nuked OPL3 emulator, Most accurate, but requires the powerful CPU
#   ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
#       include "chips/nuked_opl3.h"
#       include "chips/nuked_opl3_v174.h"
#   endif

// DosBox 0.74 OPL3 emulator, Well-accurate and fast
#   ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
#       include "chips/dosbox_opl3.h"
#   endif

// Opal emulator
#   ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
#       include "chips/opal_opl3.h"
#   endif

// Java emulator
#   ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
#       include "chips/java_opl3.h"
#   endif
#endif

static const unsigned adl_emulatorSupport = 0
#ifndef ADLMIDI_HW_OPL
#   ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
    | (1u << ADLMIDI_EMU_NUKED) | (1u << ADLMIDI_EMU_NUKED_174)
#   endif

#   ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
    | (1u << ADLMIDI_EMU_DOSBOX)
#   endif

#   ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
    | (1u << ADLMIDI_EMU_OPAL)
#   endif

#   ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
    | (1u << ADLMIDI_EMU_JAVA)
#   endif
#endif
;

//! Check emulator availability
bool adl_isEmulatorAvailable(int emulator)
{
    return (adl_emulatorSupport & (1u << (unsigned)emulator)) != 0;
}

//! Find highest emulator
int adl_getHighestEmulator()
{
    int emu = -1;
    for(unsigned m = adl_emulatorSupport; m > 0; m >>= 1)
        ++emu;
    return emu;
}

//! Find lowest emulator
int adl_getLowestEmulator()
{
    int emu = -1;
    unsigned m = adl_emulatorSupport;
    if(m > 0)
    {
        for(emu = 0; (m & 1) == 0; m >>= 1)
            ++emu;
    }
    return emu;
}

//! Per-channel and per-operator registers map
static const uint16_t g_operatorsMap[(NUM_OF_CHANNELS + NUM_OF_RM_CHANNELS) * 2] =
{
    // Channels 0-2
    0x000, 0x003, 0x001, 0x004, 0x002, 0x005, // operators  0, 3,  1, 4,  2, 5
    // Channels 3-5
    0x008, 0x00B, 0x009, 0x00C, 0x00A, 0x00D, // operators  6, 9,  7,10,  8,11
    // Channels 6-8
    0x010, 0x013, 0x011, 0x014, 0x012, 0x015, // operators 12,15, 13,16, 14,17
    // Same for second card
    0x100, 0x103, 0x101, 0x104, 0x102, 0x105, // operators 18,21, 19,22, 20,23
    0x108, 0x10B, 0x109, 0x10C, 0x10A, 0x10D, // operators 24,27, 25,28, 26,29
    0x110, 0x113, 0x111, 0x114, 0x112, 0x115, // operators 30,33, 31,34, 32,35

    //==For Rhythm-mode percussions
    // Channel 18
    0x010, 0x013,  // operators 12,15
    // Channel 19
    0xFFF, 0x014,  // operator 16
    // Channel 19
    0x012, 0xFFF,  // operator 14
    // Channel 19
    0xFFF, 0x015,  // operator 17
    // Channel 19
    0x011, 0xFFF,  // operator 13

    //==For Rhythm-mode percussions in CMF, snare and cymbal operators has inverted!
    0x010, 0x013,  // operators 12,15
    // Channel 19
    0x014, 0xFFF,  // operator 16
    // Channel 19
    0x012, 0xFFF,  // operator 14
    // Channel 19
    0x015, 0xFFF,  // operator 17
    // Channel 19
    0x011, 0xFFF   // operator 13
};

//! Channel map to regoster offsets
static const uint16_t g_channelsMap[NUM_OF_CHANNELS] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0x007, 0x008, 0x008, 0x008 // <- hw percussions, hihats and cymbals using tom-tom's channel as pitch source
};

//! Channel map to regoster offsets (separated copy for panning)
static const uint16_t g_channelsMapPan[NUM_OF_CHANNELS] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0x007, 0x008, 0xFFF, 0xFFF // <- hw percussions, 0xFFF = no support for pitch/pan
};

/*
    In OPL3 mode:
         0    1    2    6    7    8     9   10   11    16   17   18
       op0  op1  op2 op12 op13 op14  op18 op19 op20  op30 op31 op32
       op3  op4  op5 op15 op16 op17  op21 op22 op23  op33 op34 op35
         3    4    5                   13   14   15
       op6  op7  op8                 op24 op25 op26
       op9 op10 op11                 op27 op28 op29
    Ports:
        +0   +1   +2  +10  +11  +12  +100 +101 +102  +110 +111 +112
        +3   +4   +5  +13  +14  +15  +103 +104 +105  +113 +114 +115
        +8   +9   +A                 +108 +109 +10A
        +B   +C   +D                 +10B +10C +10D

    Percussion:
      bassdrum = op(0): 0xBD bit 0x10, operators 12 (0x10) and 15 (0x13) / channels 6, 6b
      snare    = op(3): 0xBD bit 0x08, operators 16 (0x14)               / channels 7b
      tomtom   = op(4): 0xBD bit 0x04, operators 14 (0x12)               / channels 8
      cym      = op(5): 0xBD bit 0x02, operators 17 (0x17)               / channels 8b
      hihat    = op(2): 0xBD bit 0x01, operators 13 (0x11)               / channels 7


    In OPTi mode ("extended FM" in 82C924, 82C925, 82C931 chips):
         0   1   2    3    4    5    6    7     8    9   10   11   12   13   14   15   16   17
       op0 op4 op6 op10 op12 op16 op18 op22  op24 op28 op30 op34 op36 op38 op40 op42 op44 op46
       op1 op5 op7 op11 op13 op17 op19 op23  op25 op29 op31 op35 op37 op39 op41 op43 op45 op47
       op2     op8      op14      op20       op26      op32
       op3     op9      op15      op21       op27      op33    for a total of 6 quad + 12 dual
    Ports: ???
*/

// Mapping from MIDI volume level to OPL level value.

static const uint_fast32_t s_dmx_volume_model[128] =
{
    0,  1,  3,  5,  6,  8,  10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127,
};

static const uint_fast32_t s_w9x_sb16_volume_model[32] =
{
    80, 63, 40, 36, 32, 28, 23, 21,
    19, 17, 15, 14, 13, 12, 11, 10,
    9,  8,  7,  6,  5,  5,  4,  4,
    3,  3,  2,  2,  1,  1,  0,  0
};

static const uint_fast32_t s_w9x_generic_fm_volume_model[32] =
{
    40, 36, 32, 28, 23, 21, 19, 17,
    15, 14, 13, 12, 11, 10, 9,  8,
    7,  6,  5,  5,  4,  4,  3,  3,
    2,  2,  1,  1,  1,  0,  0,  0
};

static const uint_fast32_t s_ail_vel_graph[16] =
{
    82,   85,  88,  91,  94,  97, 100, 103,
    106, 109, 112, 115, 118, 121, 124, 127
};

//! a VERY APROXIMAL HMI volume model, TODO: Research accurate one!
static const uint_fast32_t s_hmi_volume_table[128] =
{
    0x3f, 0x3f, 0x3a, 0x35, 0x35, 0x30, 0x30, 0x2c, 0x2c, 0x29, 0x29,
    0x25, 0x25, 0x24, 0x24, 0x23, 0x23, 0x22, 0x21, 0x21, 0x20, 0x20,
    0x1f, 0x1f, 0x1e, 0x1e, 0x1d, 0x1d, 0x1c, 0x1c, 0x1b, 0x1b, 0x1a,
    0x1a, 0x1a, 0x19, 0x19, 0x19, 0x18, 0x18, 0x18, 0x17, 0x17, 0x17,
    0x16, 0x16, 0x16, 0x15, 0x15, 0x15, 0x14, 0x14, 0x14, 0x14, 0x13,
    0x13, 0x13, 0x13, 0x12, 0x12, 0x12, 0x12, 0x11, 0x11, 0x11, 0x10,
    0x10, 0x10, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0d, 0x0d, 0x0d,
    0x0c, 0x0c, 0x0c, 0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a,
    0x09, 0x09, 0x09, 0x09, 0x08, 0x08, 0x08, 0x08, 0x07, 0x07, 0x07,
    0x07, 0x06, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05, 0x04, 0x04,
    0x04, 0x04, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00
};

enum
{
    MasterVolumeDefault = 127
};

enum
{
    OPL_PANNING_LEFT  = 0x10,
    OPL_PANNING_RIGHT = 0x20,
    OPL_PANNING_BOTH  = 0x30
};



static adlinsdata2 makeEmptyInstrument()
{
    adlinsdata2 ins;
    memset(&ins, 0, sizeof(adlinsdata2));
    ins.flags = adlinsdata::Flag_NoSound;
    return ins;
}

const adlinsdata2 OPL3::m_emptyInstrument = makeEmptyInstrument();

OPL3::OPL3() :
    m_numChips(1),
    m_numFourOps(0),
    m_deepTremoloMode(false),
    m_deepVibratoMode(false),
    m_rhythmMode(false),
    m_softPanning(false),
    m_masterVolume(MasterVolumeDefault),
    m_musicMode(MODE_MIDI),
    m_volumeScale(VOLUME_Generic)
{
    m_insBankSetup.volumeModel = OPL3::VOLUME_Generic;
    m_insBankSetup.deepTremolo = false;
    m_insBankSetup.deepVibrato = false;
    m_insBankSetup.scaleModulators = false;

#ifdef DISABLE_EMBEDDED_BANKS
    m_embeddedBank = CustomBankTag;
#else
    setEmbeddedBank(0);
#endif
}

OPL3::~OPL3()
{
#ifdef ADLMIDI_HW_OPL
    silenceAll();
    writeRegI(0, 0x0BD, 0);
    writeRegI(0, 0x104, 0);
    writeRegI(0, 0x105, 0);
    silenceAll();
#endif
}

bool OPL3::setupLocked()
{
    return (m_musicMode == MODE_CMF ||
            m_musicMode == MODE_IMF ||
            m_musicMode == MODE_RSXX);
}

void OPL3::setEmbeddedBank(uint32_t bank)
{
#ifndef DISABLE_EMBEDDED_BANKS
    m_embeddedBank = bank;
    //Embedded banks are supports 128:128 GM set only
    m_insBanks.clear();

    if(bank >= static_cast<uint32_t>(g_embeddedBanksCount))
        return;

    const BanksDump::BankEntry &bankEntry = g_embeddedBanks[m_embeddedBank];
    m_insBankSetup.deepTremolo = ((bankEntry.bankSetup >> 8) & 0x01) != 0;
    m_insBankSetup.deepVibrato = ((bankEntry.bankSetup >> 8) & 0x02) != 0;
    m_insBankSetup.volumeModel = (bankEntry.bankSetup & 0xFF);
    m_insBankSetup.scaleModulators = false;

    for(int ss = 0; ss < 2; ss++)
    {
        bank_count_t maxBanks = ss ? bankEntry.banksPercussionCount : bankEntry.banksMelodicCount;
        bank_count_t banksOffset = ss ? bankEntry.banksOffsetPercussive : bankEntry.banksOffsetMelodic;

        for(bank_count_t bankID = 0; bankID < maxBanks; bankID++)
        {
            size_t bankIndex = g_embeddedBanksMidiIndex[banksOffset + bankID];
            const BanksDump::MidiBank &bankData = g_embeddedBanksMidi[bankIndex];
            size_t bankMidiIndex = static_cast<size_t>((bankData.msb * 256) + bankData.lsb) + (ss ? static_cast<size_t>(PercussionTag) : 0);
            Bank &bankTarget = m_insBanks[bankMidiIndex];

            for(size_t instId = 0; instId < 128; instId++)
            {
                midi_bank_idx_t instIndex = bankData.insts[instId];
                if(instIndex < 0)
                    continue;
                BanksDump::InstrumentEntry instIn = g_embeddedBanksInstruments[instIndex];
                adlinsdata2 &instOut = bankTarget.ins[instId];

                adlFromInstrument(instIn, instOut);
            }
        }
    }

#else
    ADL_UNUSED(bank);
#endif
}

void OPL3::writeReg(size_t chip, uint16_t address, uint8_t value)
{
#ifdef ADLMIDI_HW_OPL
    ADL_UNUSED(chip);
    unsigned o = address >> 8;
    unsigned port = OPLBase + o * 2;

#   ifdef __DJGPP__
    outportb(port, address);
    for(unsigned c = 0; c < 6; ++c) inportb(port);
    outportb(port + 1, value);
    for(unsigned c = 0; c < 35; ++c) inportb(port);
#   endif

#   ifdef __WATCOMC__
    outp(port, address);
    for(uint16_t c = 0; c < 6; ++c)  inp(port);
    outp(port + 1, value);
    for(uint16_t c = 0; c < 35; ++c) inp(port);
#   endif//__WATCOMC__

#else//ADLMIDI_HW_OPL
    m_chips[chip]->writeReg(address, value);
#endif
}

void OPL3::writeRegI(size_t chip, uint32_t address, uint32_t value)
{
#ifdef ADLMIDI_HW_OPL
    writeReg(chip, static_cast<uint16_t>(address), static_cast<uint8_t>(value));
#else//ADLMIDI_HW_OPL
    m_chips[chip]->writeReg(static_cast<uint16_t>(address), static_cast<uint8_t>(value));
#endif
}

void OPL3::writePan(size_t chip, uint32_t address, uint32_t value)
{
#ifndef ADLMIDI_HW_OPL
    m_chips[chip]->writePan(static_cast<uint16_t>(address), static_cast<uint8_t>(value));
#else
    ADL_UNUSED(chip);
    ADL_UNUSED(address);
    ADL_UNUSED(value);
#endif
}


void OPL3::noteOff(size_t c)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;

    if(cc >= OPL3_CHANNELS_RHYTHM_BASE)
    {
        m_regBD[chip] &= ~(0x10 >> (cc - OPL3_CHANNELS_RHYTHM_BASE));
        writeRegI(chip, 0xBD, m_regBD[chip]);
        return;
    }

    writeRegI(chip, 0xB0 + g_channelsMap[cc], m_keyBlockFNumCache[c] & 0xDF);
}

void OPL3::noteOn(size_t c1, size_t c2, double hertz) // Hertz range: 0..131071
{
    size_t chip = c1 / NUM_OF_CHANNELS, cc1 = c1 % NUM_OF_CHANNELS, cc2 = c2 % NUM_OF_CHANNELS;
    uint32_t octave = 0, ftone = 0, mul_offset = 0;

    if(hertz < 0)
        return;

    //Basic range until max of octaves reaching
    while((hertz >= 1023.5) && (octave < 0x1C00))
    {
        hertz /= 2.0;    // Calculate octave
        octave += 0x400;
    }
    //Extended range, rely on frequency multiplication increment
    while(hertz >= 1022.75)
    {
        hertz /= 2.0;    // Calculate octave
        mul_offset++;
    }

    ftone = octave + static_cast<uint32_t>(hertz + 0.5);
    uint32_t chn = g_channelsMap[cc1];
    const adldata &patch1 = m_insCache[c1];
    const adldata &patch2 = m_insCache[c2 < m_insCache.size() ? c2 : 0];

    if(cc1 < OPL3_CHANNELS_RHYTHM_BASE)
    {
        ftone += 0x2000u; /* Key-ON [KON] */

        const bool natural_4op = (m_channelCategory[c1] == ChanCat_4op_Master);
        const size_t opsCount = natural_4op ? 4 : 2;
        const uint16_t op_addr[4] =
        {
            g_operatorsMap[cc1 * 2 + 0], g_operatorsMap[cc1 * 2 + 1],
            g_operatorsMap[cc2 * 2 + 0], g_operatorsMap[cc2 * 2 + 1]
        };
        const uint32_t ops[4] =
        {
            patch1.modulator_E862 & 0xFF,
            patch1.carrier_E862 & 0xFF,
            patch2.modulator_E862 & 0xFF,
            patch2.carrier_E862 & 0xFF
        };

        for(size_t op = 0; op < opsCount; op++)
        {
            if(op_addr[op] == 0xFFF)
                continue;
            if(mul_offset > 0)
            {
                uint32_t dt  = ops[op] & 0xF0;
                uint32_t mul = ops[op] & 0x0F;
                if((mul + mul_offset) > 0x0F)
                {
                    mul_offset = 0;
                    mul = 0x0F;
                }
                writeRegI(chip, 0x20 + op_addr[op],  (dt | (mul + mul_offset)) & 0xFF);
            }
            else
            {
                writeRegI(chip, 0x20 + op_addr[op],  ops[op] & 0xFF);
            }
        }
    }

    if(chn != 0xFFF)
    {
        writeRegI(chip , 0xA0 + chn, (ftone & 0xFF));
        writeRegI(chip , 0xB0 + chn, (ftone >> 8));
        m_keyBlockFNumCache[c1] = (ftone >> 8);
    }

    if(cc1 >= OPL3_CHANNELS_RHYTHM_BASE)
    {
        m_regBD[chip ] |= (0x10 >> (cc1 - OPL3_CHANNELS_RHYTHM_BASE));
        writeRegI(chip , 0x0BD, m_regBD[chip ]);
        //x |= 0x800; // for test
    }
}

static inline uint_fast32_t brightnessToOPL(uint_fast32_t brightness)
{
    double b = static_cast<double>(brightness);
    double ret = ::round(127.0 * ::sqrt(b * (1.0 / 127.0))) / 2.0;
    return static_cast<uint_fast32_t>(ret);
}

void OPL3::touchNote(size_t c,
                     uint_fast32_t velocity,
                     uint_fast32_t channelVolume,
                     uint_fast32_t channelExpression,
                     uint8_t brightness)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;
    const adldata &adli = m_insCache[c];
    size_t cmf_offset = ((m_musicMode == MODE_CMF) && cc >= OPL3_CHANNELS_RHYTHM_BASE) ? 10 : 0;
    uint16_t o1 = g_operatorsMap[cc * 2 + 0 + cmf_offset];
    uint16_t o2 = g_operatorsMap[cc * 2 + 1 + cmf_offset];
    uint8_t  srcMod = adli.modulator_40,
             srcCar = adli.carrier_40;
    uint32_t mode = 1; // 2-op AM

    uint_fast32_t kslMod = srcMod & 0xC0;
    uint_fast32_t kslCar = srcCar & 0xC0;
    uint_fast32_t tlMod = srcMod & 0x3F;
    uint_fast32_t tlCar = srcCar & 0x3F;

    uint_fast32_t modulator;
    uint_fast32_t carrier;

    uint_fast32_t volume = 0;
    uint_fast32_t midiVolume = 0;

    bool do_modulator = false;
    bool do_carrier = true;

    static const bool do_ops[10][2] =
    {
        { false, true },  /* 2 op FM */
        { true,  true },  /* 2 op AM */
        { false, false }, /* 4 op FM-FM ops 1&2 */
        { true,  false }, /* 4 op AM-FM ops 1&2 */
        { false, true  }, /* 4 op FM-AM ops 1&2 */
        { true,  false }, /* 4 op AM-AM ops 1&2 */
        { false, true  }, /* 4 op FM-FM ops 3&4 */
        { false, true  }, /* 4 op AM-FM ops 3&4 */
        { false, true  }, /* 4 op FM-AM ops 3&4 */
        { true,  true  }  /* 4 op AM-AM ops 3&4 */
    };


    // ------ Mix volumes and compute average ------

    switch(m_volumeScale)
    {
    default:
    case Synth::VOLUME_Generic:
    {
        volume = velocity * m_masterVolume *
                 channelVolume * channelExpression;

        /* If the channel has arpeggio, the effective volume of
             * *this* instrument is actually lower due to timesharing.
             * To compensate, add extra volume that corresponds to the
             * time this note is *not* heard.
             * Empirical tests however show that a full equal-proportion
             * increment sounds wrong. Therefore, using the square root.
             */
        //volume = (int)(volume * std::sqrt( (double) ch[c].users.size() ));
        const double c1 = 11.541560327111707;
        const double c2 = 1.601379199767093e+02;
        const uint_fast32_t minVolume = 1108075; // 8725 * 127

        // The formula below: SOLVE(V=127^4 * 2^( (A-63.49999) / 8), A)
        if(volume > minVolume)
        {
            double lv = std::log(static_cast<double>(volume));
            volume = static_cast<uint_fast32_t>(lv * c1 - c2);
        }
        else
            volume = 0;
    }
    break;

    case Synth::VOLUME_NATIVE:
    {
        volume = velocity * channelVolume * channelExpression;
        // 4096766 = (127 * 127 * 127) / 2
        volume = (volume * m_masterVolume) / 4096766;
    }
    break;

    case Synth::VOLUME_DMX:
    case Synth::VOLUME_DMX_FIXED:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = (s_dmx_volume_model[volume] + 1) << 1;
        volume = (s_dmx_volume_model[(velocity < 128) ? velocity : 127] * volume) >> 9;
    }
    break;

    case Synth::VOLUME_APOGEE:
    case Synth::VOLUME_APOGEE_FIXED:
    {
        midiVolume = (channelVolume * channelExpression * m_masterVolume / 16129);
    }
    break;

    case Synth::VOLUME_9X:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = s_w9x_sb16_volume_model[volume >> 2];
    }
    break;

    case Synth::VOLUME_9X_GENERIC_FM:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = s_w9x_generic_fm_volume_model[volume >> 2];
    }
    break;

    case Synth::VOLUME_AIL:
    {
        midiVolume = (channelVolume * channelExpression) * 2;
        midiVolume >>= 8;
        if(midiVolume != 0)
            midiVolume++;

        velocity = (velocity & 0x7F) >> 3;
        velocity = s_ail_vel_graph[velocity];

        midiVolume = (midiVolume * velocity) * 2;
        midiVolume >>= 8;
        if(midiVolume != 0)
            midiVolume++;

        if(m_masterVolume < 127)
            midiVolume = (midiVolume * m_masterVolume) / 127;
    }
    break;

    case Synth::VOLUME_HMI:
    {
        /* Temporarily copying DMX volume model. TODO: Reverse-engine the actual HMI volume model! */
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = (((velocity < 128) ? velocity : 127) * volume) / 127;
        volume = 63 - s_hmi_volume_table[volume];
    }
    break;
    }

    if(volume > 63)
        volume = 63;

    if(midiVolume > 127)
        midiVolume = 127;


    if(m_channelCategory[c] == ChanCat_Regular ||
       m_channelCategory[c] == ChanCat_Rhythm_Bass)
    {
        mode = adli.feedconn & 1; // 2-op FM or 2-op AM
    }
    else if(m_channelCategory[c] == ChanCat_4op_Master ||
            m_channelCategory[c] == ChanCat_4op_Slave)
    {
        const adldata *i0, *i1;

        if(m_channelCategory[c] == ChanCat_4op_Master)
        {
            i0 = &adli;
            i1 = &m_insCache[c + 3];
            mode = 2; // 4-op xx-xx ops 1&2
        }
        else
        {
            i0 = &m_insCache[c - 3];
            i1 = &adli;
            mode = 6; // 4-op xx-xx ops 3&4
        }

        mode += (i0->feedconn & 1) + (i1->feedconn & 1) * 2;
    }

    do_modulator = do_ops[mode][0] || m_scaleModulators;
    do_carrier   = do_ops[mode][1] || m_scaleModulators;

    // ------ Compute the total level register output data ------

    if(m_musicMode == MODE_RSXX)
    {
        tlCar -= volume / 2;
    }
    else if(m_volumeScale == Synth::VOLUME_APOGEE ||
            m_volumeScale == Synth::VOLUME_APOGEE_FIXED)
    {
        // volume = ((64 * (velocity + 0x80)) * volume) >> 15;

        if(do_carrier)
        {
            tlCar = 63 - tlCar;
            tlCar *= velocity + 0x80;
            tlCar = (midiVolume * tlCar) >> 15;
            tlCar = tlCar ^ 63;
        }

        if(do_modulator)
        {
            uint_fast32_t mod = tlCar;

            tlMod = 63 - tlMod;
            tlMod *= velocity + 0x80;

            if(m_volumeScale == Synth::VOLUME_APOGEE_FIXED || mode > 1)
                mod = tlMod; // Fix the AM voices bug

            // NOTE: Here is a bug of Apogee Sound System that makes modulator
            // to not work properly on AM instruments. The fix of this bug, you
            // need to replace the tlCar with tmMod in this formula.
            // Don't do the bug on 4-op voices.
            tlMod = (midiVolume * mod) >> 15;

            tlMod ^= 63;
        }
    }
    else if(m_volumeScale == Synth::VOLUME_DMX && mode <= 1)
    {
        tlCar = (63 - volume);

        if(do_modulator)
        {
            if(tlMod < tlCar)
                tlMod = tlCar;
        }
    }
    else if(m_volumeScale == Synth::VOLUME_9X)
    {
        if(do_carrier)
            tlCar += volume + s_w9x_sb16_volume_model[velocity >> 2];
        if(do_modulator)
            tlMod += volume + s_w9x_sb16_volume_model[velocity >> 2];

        if(tlCar > 0x3F)
            tlCar = 0x3F;
        if(tlMod > 0x3F)
            tlMod = 0x3F;
    }
    else if(m_volumeScale == Synth::VOLUME_9X_GENERIC_FM)
    {
        if(do_carrier)
            tlCar += volume + s_w9x_generic_fm_volume_model[velocity >> 2];
        if(do_modulator)
            tlMod += volume + s_w9x_generic_fm_volume_model[velocity >> 2];

        if(tlCar > 0x3F)
            tlCar = 0x3F;
        if(tlMod > 0x3F)
            tlMod = 0x3F;
    }
    else if(m_volumeScale == Synth::VOLUME_AIL)
    {
        uint_fast32_t v0_val = (~srcMod) & 0x3f;
        uint_fast32_t v1_val = (~srcCar) & 0x3f;

        if(do_modulator)
            v0_val = (v0_val * midiVolume) / 127;
        if(do_carrier)
            v1_val = (v1_val * midiVolume) / 127;

        tlMod = (~v0_val) & 0x3F;
        tlCar = (~v1_val) & 0x3F;
    }
    else
    {
        if(do_modulator)
            tlMod = 63 - volume + (volume * tlMod) / 63;
        if(do_carrier)
            tlCar = 63 - volume + (volume * tlCar) / 63;
    }

    if(brightness != 127)
    {
        brightness = brightnessToOPL(brightness);
        if(!do_modulator)
            tlMod = 63 - brightness + (brightness * tlMod) / 63;
        if(!do_carrier)
            tlCar = 63 - brightness + (brightness * tlCar) / 63;
    }

    modulator = (kslMod & 0xC0) | (tlMod & 63);
    carrier = (kslCar & 0xC0) | (tlCar & 63);

    if(o1 != 0xFFF)
        writeRegI(chip, 0x40 + o1, modulator);
    if(o2 != 0xFFF)
        writeRegI(chip, 0x40 + o2, carrier);

    // Correct formula (ST3, AdPlug):
    //   63-((63-(instrvol))/63)*chanvol
    // Reduces to (tested identical):
    //   63 - chanvol + chanvol*instrvol/63
    // Also (slower, floats):
    //   63 + chanvol * (instrvol / 63.0 - 1)
}

void OPL3::setPatch(size_t c, const adldata &instrument)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;
    static const uint8_t data[4] = {0x20, 0x60, 0x80, 0xE0};
    m_insCache[c] = instrument;
    size_t cmf_offset = ((m_musicMode == MODE_CMF) && (cc >= OPL3_CHANNELS_RHYTHM_BASE)) ? 10 : 0;
    uint16_t o1 = g_operatorsMap[cc * 2 + 0 + cmf_offset];
    uint16_t o2 = g_operatorsMap[cc * 2 + 1 + cmf_offset];
    unsigned x = instrument.modulator_E862, y = instrument.carrier_E862;

    for(size_t a = 0; a < 4; ++a, x >>= 8, y >>= 8)
    {
        if(o1 != 0xFFF)
            writeRegI(chip, data[a] + o1, x & 0xFF);
        if(o2 != 0xFFF)
            writeRegI(chip, data[a] + o2, y & 0xFF);
    }
}

void OPL3::setPan(size_t c, uint8_t value)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;
    if(g_channelsMapPan[cc] != 0xFFF)
    {
#ifndef ADLMIDI_HW_OPL
        if (m_softPanning)
        {
            writePan(chip, g_channelsMapPan[cc], value);
            writeRegI(chip, 0xC0 + g_channelsMapPan[cc], m_insCache[c].feedconn | OPL_PANNING_BOTH);
        }
        else
        {
#endif
            int panning = 0;
            if(value  < 64 + 32) panning |= OPL_PANNING_LEFT;
            if(value >= 64 - 32) panning |= OPL_PANNING_RIGHT;
            writePan(chip, g_channelsMapPan[cc], 64);
            writeRegI(chip, 0xC0 + g_channelsMapPan[cc], m_insCache[c].feedconn | panning);
#ifndef ADLMIDI_HW_OPL
        }
#endif
    }
}

void OPL3::silenceAll() // Silence all OPL channels.
{
    for(size_t c = 0; c < m_numChannels; ++c)
    {
        noteOff(c);
        touchNote(c, 0, 0, 0);
    }
}

void OPL3::updateChannelCategories()
{
    const uint32_t fours = m_numFourOps;

    for(uint32_t chip = 0, fours_left = fours; chip < m_numChips; ++chip)
    {
        m_regBD[chip] = (m_deepTremoloMode * 0x80 + m_deepVibratoMode * 0x40 + m_rhythmMode * 0x20);
        writeRegI(chip, 0x0BD, m_regBD[chip]);
        uint32_t fours_this_chip = std::min(fours_left, static_cast<uint32_t>(6u));
        writeRegI(chip, 0x104, (1 << fours_this_chip) - 1);
        fours_left -= fours_this_chip;
    }

    if(!m_rhythmMode)
    {
        for(size_t a = 0, n = m_numChips; a < n; ++a)
        {
            for(size_t b = 0; b < NUM_OF_CHANNELS; ++b)
            {
                m_channelCategory[a * NUM_OF_CHANNELS + b] =
                    (b >= OPL3_CHANNELS_RHYTHM_BASE) ? ChanCat_Rhythm_Slave : ChanCat_Regular;
            }
        }
    }
    else
    {
        for(size_t a = 0, n = m_numChips; a < n; ++a)
        {
            for(size_t b = 0; b < NUM_OF_CHANNELS; ++b)
            {
                m_channelCategory[a * NUM_OF_CHANNELS + b] =
                    (b >= OPL3_CHANNELS_RHYTHM_BASE) ? static_cast<ChanCat>(ChanCat_Rhythm_Bass + (b - OPL3_CHANNELS_RHYTHM_BASE)) :
                    (b >= 6 && b < 9) ? ChanCat_Rhythm_Slave : ChanCat_Regular;
            }
        }
    }

    uint32_t nextfour = 0;
    for(uint32_t a = 0; a < fours; ++a)
    {
        m_channelCategory[nextfour] = ChanCat_4op_Master;
        m_channelCategory[nextfour + 3] = ChanCat_4op_Slave;

        switch(a % 6)
        {
        case 0:
        case 1:
            nextfour += 1;
            break;
        case 2:
            nextfour += 9 - 2;
            break;
        case 3:
        case 4:
            nextfour += 1;
            break;
        case 5:
            nextfour += NUM_OF_CHANNELS - 9 - 2;
            break;
        }
    }

/**/
/*
    In two-op mode, channels 0..8 go as follows:
                  Op1[port]  Op2[port]
      Channel 0:  00  00     03  03
      Channel 1:  01  01     04  04
      Channel 2:  02  02     05  05
      Channel 3:  06  08     09  0B
      Channel 4:  07  09     10  0C
      Channel 5:  08  0A     11  0D
      Channel 6:  12  10     15  13
      Channel 7:  13  11     16  14
      Channel 8:  14  12     17  15
    In four-op mode, channels 0..8 go as follows:
                  Op1[port]  Op2[port]  Op3[port]  Op4[port]
      Channel 0:  00  00     03  03     06  08     09  0B
      Channel 1:  01  01     04  04     07  09     10  0C
      Channel 2:  02  02     05  05     08  0A     11  0D
      Channel 3:  CHANNEL 0 SLAVE
      Channel 4:  CHANNEL 1 SLAVE
      Channel 5:  CHANNEL 2 SLAVE
      Channel 6:  12  10     15  13
      Channel 7:  13  11     16  14
      Channel 8:  14  12     17  15
     Same goes principally for channels 9-17 respectively.
    */
}

void OPL3::commitDeepFlags()
{
    for(size_t chip = 0; chip < m_numChips; ++chip)
    {
        m_regBD[chip] = (m_deepTremoloMode * 0x80 + m_deepVibratoMode * 0x40 + m_rhythmMode * 0x20);
        writeRegI(chip, 0x0BD, m_regBD[chip]);
    }
}

void OPL3::setVolumeScaleModel(ADLMIDI_VolumeModels volumeModel)
{
    switch(volumeModel)
    {
    case ADLMIDI_VolumeModel_AUTO://Do nothing until restart playing
        break;

    case ADLMIDI_VolumeModel_Generic:
        m_volumeScale = OPL3::VOLUME_Generic;
        break;

    case ADLMIDI_VolumeModel_NativeOPL3:
        m_volumeScale = OPL3::VOLUME_NATIVE;
        break;

    case ADLMIDI_VolumeModel_DMX:
        m_volumeScale = OPL3::VOLUME_DMX;
        break;

    case ADLMIDI_VolumeModel_APOGEE:
        m_volumeScale = OPL3::VOLUME_APOGEE;
        break;

    case ADLMIDI_VolumeModel_9X:
        m_volumeScale = OPL3::VOLUME_9X;
        break;

    case ADLMIDI_VolumeModel_DMX_Fixed:
        m_volumeScale = OPL3::VOLUME_DMX_FIXED;
        break;

    case ADLMIDI_VolumeModel_APOGEE_Fixed:
        m_volumeScale = OPL3::VOLUME_APOGEE_FIXED;
        break;

    case ADLMIDI_VolumeModel_AIL:
        m_volumeScale = OPL3::VOLUME_AIL;
        break;

    case ADLMIDI_VolumeModel_9X_GENERIC_FM:
        m_volumeScale = OPL3::VOLUME_9X_GENERIC_FM;
        break;

    case ADLMIDI_VolumeModel_HMI:
        m_volumeScale = OPL3::VOLUME_HMI;
        break;
    }
}

ADLMIDI_VolumeModels OPL3::getVolumeScaleModel()
{
    switch(m_volumeScale)
    {
    default:
    case OPL3::VOLUME_Generic:
        return ADLMIDI_VolumeModel_Generic;
    case OPL3::VOLUME_NATIVE:
        return ADLMIDI_VolumeModel_NativeOPL3;
    case OPL3::VOLUME_DMX:
        return ADLMIDI_VolumeModel_DMX;
    case OPL3::VOLUME_APOGEE:
        return ADLMIDI_VolumeModel_APOGEE;
    case OPL3::VOLUME_9X:
        return ADLMIDI_VolumeModel_9X;
    case OPL3::VOLUME_DMX_FIXED:
        return ADLMIDI_VolumeModel_DMX_Fixed;
    case OPL3::VOLUME_APOGEE_FIXED:
        return ADLMIDI_VolumeModel_APOGEE_Fixed;
    case OPL3::VOLUME_AIL:
        return ADLMIDI_VolumeModel_AIL;
    case OPL3::VOLUME_9X_GENERIC_FM:
        return ADLMIDI_VolumeModel_9X_GENERIC_FM;
    case OPL3::VOLUME_HMI:
        return ADLMIDI_VolumeModel_HMI;
    }
}

#ifndef ADLMIDI_HW_OPL
void OPL3::clearChips()
{
    for(size_t i = 0; i < m_chips.size(); i++)
        m_chips[i].reset(NULL);
    m_chips.clear();
}
#endif

void OPL3::reset(int emulator, unsigned long PCM_RATE, void *audioTickHandler)
{
#ifndef ADLMIDI_HW_OPL
    clearChips();
#else
    (void)emulator;
    (void)PCM_RATE;
#endif
#if !defined(ADLMIDI_AUDIO_TICK_HANDLER)
    (void)audioTickHandler;
#endif
    m_insCache.clear();
    m_keyBlockFNumCache.clear();
    m_regBD.clear();

#ifndef ADLMIDI_HW_OPL
    m_chips.resize(m_numChips, AdlMIDI_SPtr<OPLChipBase>());
#endif

    const struct adldata defaultInsCache = { 0x1557403,0x005B381, 0x49,0x80, 0x4, +0 };
    m_numChannels = m_numChips * NUM_OF_CHANNELS;
    m_insCache.resize(m_numChannels, defaultInsCache);
    m_keyBlockFNumCache.resize(m_numChannels,   0);
    m_regBD.resize(m_numChips,    0);
    m_channelCategory.resize(m_numChannels, 0);

    for(size_t p = 0, a = 0; a < m_numChips; ++a)
    {
        for(size_t b = 0; b < OPL3_CHANNELS_RHYTHM_BASE; ++b)
            m_channelCategory[p++] = ChanCat_Regular;
        for(size_t b = 0; b < NUM_OF_RM_CHANNELS; ++b)
            m_channelCategory[p++] = ChanCat_Rhythm_Slave;
    }

    static const uint16_t data[] =
    {
        0x004, 96, 0x004, 128,        // Pulse timer
        0x105, 0, 0x105, 1, 0x105, 0, // Pulse OPL3 enable
        0x001, 32, 0x105, 1           // Enable wave, OPL3 extensions
    };
//    size_t fours = m_numFourOps;

    for(size_t i = 0; i < m_numChips; ++i)
    {
#ifndef ADLMIDI_HW_OPL
        OPLChipBase *chip;
        switch(emulator)
        {
        default:
            assert(false);
            abort();
#ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
        case ADLMIDI_EMU_NUKED: /* Latest Nuked OPL3 */
            chip = new NukedOPL3;
            break;
        case ADLMIDI_EMU_NUKED_174: /* Old Nuked OPL3 1.4.7 modified and optimized */
            chip = new NukedOPL3v174;
            break;
#endif
#ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
        case ADLMIDI_EMU_DOSBOX:
            chip = new DosBoxOPL3;
            break;
#endif
#ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
        case ADLMIDI_EMU_OPAL:
            chip = new OpalOPL3;
            break;
#endif
#ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
        case ADLMIDI_EMU_JAVA:
            chip = new JavaOPL3;
            break;
#endif
        }
        m_chips[i].reset(chip);
        chip->setChipId((uint32_t)i);
        chip->setRate((uint32_t)PCM_RATE);
        if(m_runAtPcmRate)
            chip->setRunningAtPcmRate(true);
#   if defined(ADLMIDI_AUDIO_TICK_HANDLER)
        chip->setAudioTickHandlerInstance(audioTickHandler);
#   endif
#endif // ADLMIDI_HW_OPL

        /* Clean-up channels from any playing junk sounds */
        for(size_t a = 0; a < OPL3_CHANNELS_RHYTHM_BASE; ++a)
            writeRegI(i, 0xB0 + g_channelsMap[a], 0x00);
        for(size_t a = 0; a < sizeof(data) / sizeof(*data); a += 2)
            writeRegI(i, data[a], (data[a + 1]));
    }

    updateChannelCategories();
    silenceAll();
}
