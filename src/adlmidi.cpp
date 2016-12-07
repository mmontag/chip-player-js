/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2016 Vitaly Novichkov <admin@wohlnet.ru>
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

// Setup compiler defines useful for exporting required public API symbols in gme.cpp
#ifndef ADLMIDI_EXPORT
#if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#define ADLMIDI_EXPORT __declspec(dllexport)
#elif defined (LIBADLMIDI_VISIBILITY)
#define ADLMIDI_EXPORT __attribute__((visibility ("default")))
#else
#define ADLMIDI_EXPORT
#endif
#endif

#ifdef _WIN32
#undef NO_OLDNAMES
#endif
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
#include <stdio.h>
#include <limits> // numeric_limit

#include <deque>
#include <algorithm>

#include "fraction.h"
#ifdef ADLMIDI_USE_DOSBOX_OPL
#include "dbopl.h"
#else
#include "nukedopl3.h"
#endif

#include "adldata.hh"
#include "adlmidi.h"

#ifdef ADLMIDI_buildAsApp
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#endif

class MIDIplay;

static const unsigned MaxCards = 100;
static std::string ADLMIDI_ErrorString;

static const unsigned short Operators[23 * 2] =
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
    // Channel 18
    0x010, 0x013,  // operators 12,15
    // Channel 19
    0x014, 0xFFF,  // operator 16
    // Channel 19
    0x012, 0xFFF,  // operator 14
    // Channel 19
    0x015, 0xFFF,  // operator 17
    // Channel 19
    0x011, 0xFFF
}; // operator 13

static const unsigned short Channels[23] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0x007, 0x008, 0xFFF, 0xFFF
}; // <- hw percussions, 0xFFF = no support for pitch/pan

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

struct OPL3
{
        friend class MIDIplay;
        uint32_t NumChannels;
        char ____padding[4];
        ADL_MIDIPlayer *_parent;
#ifdef ADLMIDI_USE_DOSBOX_OPL
        std::vector<DBOPL::Handler> cards;
#else
        std::vector<_opl3_chip> cards;
#endif
    private:
        std::vector<uint16_t>   ins; // index to adl[], cached, needed by Touch()
        std::vector<uint8_t>    pit;  // value poked to B0, cached, needed by NoteOff)(
        std::vector<uint8_t>    regBD;

        std::vector<adlinsdata> dynamic_metainstruments; // Replaces adlins[] when CMF file
        std::vector<adldata>    dynamic_instruments;     // Replaces adl[]    when CMF file
        const unsigned DynamicInstrumentTag /* = 0x8000u*/, DynamicMetaInstrumentTag /* = 0x4000000u*/;
        const adlinsdata &GetAdlMetaIns(unsigned n)
        {
            return (n & DynamicMetaInstrumentTag) ?
                   dynamic_metainstruments[n & ~DynamicMetaInstrumentTag]
                   : adlins[n];
        }
        unsigned GetAdlMetaNumber(unsigned midiins)
        {
            return (AdlBank == ~0u) ?
                   (midiins | DynamicMetaInstrumentTag)
                   : banks[AdlBank][midiins];
        }
        const adldata &GetAdlIns(unsigned short insno)
        {
            return (insno & DynamicInstrumentTag)
                   ? dynamic_instruments[insno & ~DynamicInstrumentTag]
                   : adl[insno];
        }

    public:
        unsigned int NumCards;
        unsigned int AdlBank;
        unsigned int NumFourOps;
        bool HighTremoloMode;
        bool HighVibratoMode;
        bool AdlPercussionMode;
        bool ScaleModulators;
        bool LogarithmicVolumes;
        OPL3() :
            DynamicInstrumentTag(0x8000u),
            DynamicMetaInstrumentTag(0x4000000u),
            NumCards(1),
            AdlBank(0),
            NumFourOps(0),
            HighTremoloMode(false),
            HighVibratoMode(false),
            AdlPercussionMode(false),
            LogarithmicVolumes(false)
        {}
        char padding2[7];
        std::vector<char> four_op_category; // 1 = quad-master, 2 = quad-slave, 0 = regular
        // 3 = percussion BassDrum
        // 4 = percussion Snare
        // 5 = percussion Tom
        // 6 = percussion Crash cymbal
        // 7 = percussion Hihat
        // 8 = percussion slave

        void Poke(size_t card, uint32_t index, uint32_t value)
        {
#ifdef ADLMIDI_USE_DOSBOX_OPL
            cards[card].WriteReg(index, static_cast<uint8_t>(value));
#else
            OPL3_WriteReg(&cards[card], static_cast<Bit16u>(index), static_cast<Bit8u>(value));
#endif
        }
        void PokeN(size_t card, uint16_t index, uint8_t value)
        {
#ifdef ADLMIDI_USE_DOSBOX_OPL
            cards[card].WriteReg(static_cast<Bit32u>(index), value);
#else
            OPL3_WriteReg(&cards[card], index, value);
#endif
        }
        void NoteOff(size_t c)
        {
            size_t card = c / 23, cc = c % 23;

            if(cc >= 18)
            {
                regBD[card] &= ~(0x10 >> (cc - 18));
                Poke(card, 0xBD, regBD[card]);
                return;
            }

            Poke(card, 0xB0 + Channels[cc], pit[c] & 0xDF);
        }
        void NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
        {
            unsigned card = c / 23, cc = c % 23;
            unsigned x = 0x2000;

            if(hertz < 0 || hertz > 131071) // Avoid infinite loop
                return;

            while(hertz >= 1023.5)
            {
                hertz /= 2.0;    // Calculate octave
                x += 0x400;
            }

            x += static_cast<unsigned int>(hertz + 0.5);
            unsigned chn = Channels[cc];

            if(cc >= 18)
            {
                regBD[card] |= (0x10 >> (cc - 18));
                Poke(card, 0x0BD, regBD[card]);
                x &= ~0x2000u;
                //x |= 0x800; // for test
            }

            if(chn != 0xFFF)
            {
                Poke(card, 0xA0 + chn, x & 0xFF);
                Poke(card, 0xB0 + chn, pit[c] = static_cast<uint8_t>(x >> 8));
            }
        }
        void Touch_Real(unsigned c, unsigned volume)
        {
            if(volume > 63) volume = 63;

            unsigned card = c / 23, cc = c % 23;
            uint16_t i = ins[c];
            unsigned o1 = Operators[cc * 2 + 0];
            unsigned o2 = Operators[cc * 2 + 1];
            const adldata &adli = GetAdlIns(i);
            unsigned x = adli.modulator_40, y = adli.carrier_40;
            unsigned mode = 1; // 2-op AM

            if(four_op_category[c] == 0 || four_op_category[c] == 3)
            {
                mode = adli.feedconn & 1; // 2-op FM or 2-op AM
            }
            else if(four_op_category[c] == 1 || four_op_category[c] == 2)
            {
                uint16_t i0, i1;

                if(four_op_category[c] == 1)
                {
                    i0 = i;
                    i1 = ins[c + 3];
                    mode = 2; // 4-op xx-xx ops 1&2
                }
                else
                {
                    i0 = ins[c - 3];
                    i1 = i;
                    mode = 6; // 4-op xx-xx ops 3&4
                }

                mode += (GetAdlIns(i0).feedconn & 1) + (GetAdlIns(i1).feedconn & 1) * 2;
            }

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
            bool do_modulator = do_ops[ mode ][ 0 ] || ScaleModulators;
            bool do_carrier   = do_ops[ mode ][ 1 ] || ScaleModulators;
            Poke(card, 0x40 + o1, do_modulator ? (x | 63) - volume + volume * (x & 63) / 63 : x);

            if(o2 != 0xFFF)
                Poke(card, 0x40 + o2, do_carrier   ? (y | 63) - volume + volume * (y & 63) / 63 : y);

            // Correct formula (ST3, AdPlug):
            //   63-((63-(instrvol))/63)*chanvol
            // Reduces to (tested identical):
            //   63 - chanvol + chanvol*instrvol/63
            // Also (slower, floats):
            //   63 + chanvol * (instrvol / 63.0 - 1)
        }
        void Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
        {
            if(LogarithmicVolumes)
                Touch_Real(c, volume * 127 / (127 * 127 * 127));
            else
            {
                // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
                Touch_Real(c, volume > 8725 ? static_cast<unsigned int>(std::log(volume) * 11.541561 + (0.5 - 104.22845)) : 0);
                // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
                //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
            }
        }
        void Patch(uint16_t c, uint16_t i)
        {
            uint16_t card = c / 23, cc = c % 23;
            static const uint8_t data[4] = {0x20, 0x60, 0x80, 0xE0};
            ins[c] = i;
            uint16_t o1 = Operators[cc * 2 + 0];
            uint16_t o2 = Operators[cc * 2 + 1];
            const adldata &adli = GetAdlIns(i);
            unsigned x = adli.modulator_E862, y = adli.carrier_E862;

            for(unsigned a = 0; a < 4; ++a, x >>= 8, y >>= 8)
            {
                Poke(card, data[a] + o1, x & 0xFF);

                if(o2 != 0xFFF)
                    Poke(card, data[a] + o2, y & 0xFF);
            }
        }
        void Pan(unsigned c, unsigned value)
        {
            unsigned card = c / 23, cc = c % 23;

            if(Channels[cc] != 0xFFF)
                Poke(card, 0xC0 + Channels[cc], GetAdlIns(ins[c]).feedconn | value);
        }
        void Silence() // Silence all OPL channels.
        {
            for(unsigned c = 0; c < NumChannels; ++c)
            {
                NoteOff(c);
                Touch_Real(c, 0);
            }
        }

        void updateFlags()
        {
            unsigned fours = NumFourOps;

            for(unsigned card = 0; card < NumCards; ++card)
            {
                Poke(card, 0x0BD, regBD[card] = (HighTremoloMode * 0x80
                                                 + HighVibratoMode * 0x40
                                                 + AdlPercussionMode * 0x20));
                unsigned fours_this_card = std::min(fours, 6u);
                Poke(card, 0x104, (1 << fours_this_card) - 1);
                fours -= fours_this_card;
            }

            // Mark all channels that are reserved for four-operator function
            if(AdlPercussionMode == 1)
                for(unsigned a = 0; a < NumCards; ++a)
                {
                    for(unsigned b = 0; b < 5; ++b)
                        four_op_category[a * 23 + 18 + b] = static_cast<char>(b + 3);

                    for(unsigned b = 0; b < 3; ++b)
                        four_op_category[a * 23 + 6  + b] = 8;
                }

            unsigned nextfour = 0;

            for(unsigned a = 0; a < NumFourOps; ++a)
            {
                four_op_category[nextfour  ] = 1;
                four_op_category[nextfour + 3] = 2;

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
                    nextfour += 23 - 9 - 2;
                    break;
                }
            }
        }
        void Reset()
        {
            LogarithmicVolumes = false;
#ifdef ADLMIDI_USE_DOSBOX_OPL
            DBOPL::Handler emptyChip;
            memset(&emptyChip, 0, sizeof(DBOPL::Handler));
#else
            _opl3_chip emptyChip;
            memset(&emptyChip, 0, sizeof(_opl3_chip));
#endif
            cards.clear();
            ins.clear();
            pit.clear();
            regBD.clear();
            cards.resize(NumCards, emptyChip);
            NumChannels = NumCards * 23;
            ins.resize(NumChannels, 189);
            pit.resize(NumChannels,   0);
            regBD.resize(NumCards,    0);
            four_op_category.resize(NumChannels, 0);

            for(unsigned p = 0, a = 0; a < NumCards; ++a)
            {
                for(unsigned b = 0; b < 18; ++b)
                    four_op_category[p++] = 0;

                for(unsigned b = 0; b < 5; ++b)
                    four_op_category[p++] = 8;
            }

            static const uint16_t data[] =
            {
                0x004, 96, 0x004, 128,        // Pulse timer
                0x105, 0, 0x105, 1, 0x105, 0, // Pulse OPL3 enable
                0x001, 32, 0x105, 1           // Enable wave, OPL3 extensions
            };
            unsigned fours = NumFourOps;

            for(unsigned card = 0; card < NumCards; ++card)
            {
#ifdef ADLMIDI_USE_DOSBOX_OPL
                cards[card].Init(_parent->PCM_RATE);
#else
                OPL3_Reset(&cards[card], static_cast<Bit32u>(_parent->PCM_RATE));
#endif

                for(unsigned a = 0; a < 18; ++a) Poke(card, 0xB0 + Channels[a], 0x00);

                for(unsigned a = 0; a < sizeof(data) / sizeof(*data); a += 2)
                    PokeN(card, data[a], static_cast<uint8_t>(data[a + 1]));

                Poke(card, 0x0BD, regBD[card] = (HighTremoloMode * 0x80
                                                 + HighVibratoMode * 0x40
                                                 + AdlPercussionMode * 0x20));
                unsigned fours_this_card = std::min(fours, 6u);
                Poke(card, 0x104, (1 << fours_this_card) - 1);
                //fprintf(stderr, "Card %u: %u four-ops.\n", card, fours_this_card);
                fours -= fours_this_card;
            }

            // Mark all channels that are reserved for four-operator function
            if(AdlPercussionMode == 1)
                for(unsigned a = 0; a < NumCards; ++a)
                {
                    for(unsigned b = 0; b < 5; ++b)
                        four_op_category[a * 23 + 18 + b] = static_cast<char>(b + 3);

                    for(unsigned b = 0; b < 3; ++b)
                        four_op_category[a * 23 + 6  + b] = 8;
                }

            unsigned nextfour = 0;

            for(unsigned a = 0; a < NumFourOps; ++a)
            {
                four_op_category[nextfour  ] = 1;
                four_op_category[nextfour + 3] = 2;

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
                    nextfour += 23 - 9 - 2;
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
            Silence();
        }
};

//static const char MIDIsymbols[256+1] =
//"PPPPPPhcckmvmxbd"  // Ins  0-15
//"oooooahoGGGGGGGG"  // Ins 16-31
//"BBBBBBBBVVVVVHHM"  // Ins 32-47
//"SSSSOOOcTTTTTTTT"  // Ins 48-63
//"XXXXTTTFFFFFFFFF"  // Ins 64-79
//"LLLLLLLLpppppppp"  // Ins 80-95
//"XXXXXXXXGGGGGTSS"  // Ins 96-111
//"bbbbMMMcGXXXXXXX"  // Ins 112-127
//"????????????????"  // Prc 0-15
//"????????????????"  // Prc 16-31
//"???DDshMhhhCCCbM"  // Prc 32-47
//"CBDMMDDDMMDDDDDD"  // Prc 48-63
//"DDDDDDDDDDDDDDDD"  // Prc 64-79
//"DD??????????????"  // Prc 80-95
//"????????????????"  // Prc 96-111
//"????????????????"; // Prc 112-127

static const uint8_t PercussionMap[256] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GM
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 3 = bass drum
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 4 = snare
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 5 = tom
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 6 = cymbal
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 7 = hihat
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP0
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP16
    //2 3 4 5 6 7 8 940 1 2 3 4 5 6 7
    "\0\0\0\3\3\7\4\7\4\5\7\5\7\5\7\5"//GP32
    //8 950 1 2 3 4 5 6 7 8 960 1 2 3
    "\5\6\5\6\6\0\5\6\0\6\0\6\5\5\5\5"//GP48
    //4 5 6 7 8 970 1 2 3 4 5 6 7 8 9
    "\5\0\0\0\0\0\7\0\0\0\0\0\0\0\0\0"//GP64
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

class MIDIplay
{
        // Information about each track
        struct Position
        {
            bool began;
            char padding[7];
            double wait;
            struct TrackInfo
            {
                size_t ptr;
                long   delay;
                int    status;
                char padding2[4];
                TrackInfo(): ptr(0), delay(0), status(0) {}
            };
            std::vector<TrackInfo> track;

            Position(): began(false), wait(0.0), track() { }
        } CurrentPosition, LoopBeginPosition, trackBeginPosition;

        std::map<std::string, uint64_t> devices;
        std::map<uint64_t /*track*/, uint64_t /*channel begin index*/> current_device;

        // Persistent settings for each MIDI channel
        struct MIDIchannel
        {
            uint16_t portamento;
            uint8_t bank_lsb, bank_msb;
            uint8_t patch;
            uint8_t volume, expression;
            uint8_t panning, vibrato, sustain;
            char ____padding[6];
            double  bend, bendsense;
            double  vibpos, vibspeed, vibdepth;
            int64_t vibdelay;
            uint8_t lastlrpn, lastmrpn;
            bool nrpn;
            struct NoteInfo
            {
                // Current pressure
                uint8_t vol;
                // Tone selected on noteon:
                char ____padding[1];
                int16_t tone;
                // Patch selected on noteon; index to banks[AdlBank][]
                uint8_t midiins;
                // Index to physical adlib data structure, adlins[]
                char ____padding2[3];
                uint32_t insmeta;
                char ____padding3[4];
                // List of adlib channels it is currently occupying.
                std::map<uint16_t /*adlchn*/, uint16_t /*ins, inde to adl[]*/ > phys;
            };
            typedef std::map<uint8_t, NoteInfo> activenotemap_t;
            typedef activenotemap_t::iterator activenoteiterator;
            char ____padding2[5];
            activenotemap_t activenotes;

            MIDIchannel()
                : portamento(0),
                  bank_lsb(0), bank_msb(0), patch(0),
                  volume(100), expression(100),
                  panning(0x30), vibrato(0), sustain(0),
                  bend(0.0), bendsense(2 / 8192.0),
                  vibpos(0), vibspeed(2 * 3.141592653 * 5.0),
                  vibdepth(0.5 / 127), vibdelay(0),
                  lastlrpn(0), lastmrpn(0), nrpn(false),
                  activenotes() { }
        };
        std::vector<MIDIchannel> Ch;
        bool cmf_percussion_mode = false;

        // Additional information about AdLib channels
        struct AdlChannel
        {
            // For collisions
            struct Location
            {
                uint16_t    MidCh;
                uint8_t     note;
                bool operator==(const Location &b) const
                {
                    return MidCh == b.MidCh && note == b.note;
                }
                bool operator< (const Location &b) const
                {
                    return MidCh < b.MidCh || (MidCh == b.MidCh && note < b.note);
                }
                char ____padding[1];
            };
            struct LocationData
            {
                bool sustained;
                char ____padding[1];
                uint16_t ins;  // a copy of that in phys[]
                char ____padding2[4];
                int64_t kon_time_until_neglible;
                int64_t vibdelay;
            };
            typedef std::map<Location, LocationData> users_t;
            users_t users;

            // If the channel is keyoff'd
            long koff_time_until_neglible;
            // For channel allocation:
            AdlChannel(): users(), koff_time_until_neglible(0) { }

            void AddAge(int64_t ms)
            {
                if(users.empty())
                    koff_time_until_neglible =
                        std::max(koff_time_until_neglible - ms, static_cast<int64_t>(-0x1FFFFFFFl));
                else
                {
                    koff_time_until_neglible = 0;

                    for(users_t::iterator i = users.begin(); i != users.end(); ++i)
                    {
                        i->second.kon_time_until_neglible =
                            std::max(i->second.kon_time_until_neglible - ms, static_cast<int64_t>(-0x1FFFFFFFl));
                        i->second.vibdelay += ms;
                    }
                }
            }
        };
    public:
        char ____padding[7];
    private:
        std::vector<AdlChannel> ch;

        std::vector<std::vector<uint8_t> > TrackData;
    public:
        MIDIplay():
            cmf_percussion_mode(false),
            config(NULL),
            trackStart(false),
            loopStart(false),
            loopEnd(false),
            loopStart_passed(false),
            invalidLoop(false),
            loopStart_hit(false)
        {
            devices.clear();
        }
        ~MIDIplay()
        {}

        ADL_MIDIPlayer *config;
        std::string musTitle;
        fraction<long> InvDeltaTicks, Tempo;
        bool    trackStart,
                loopStart,
                loopEnd,
                loopStart_passed /*Tells that "loopStart" already passed*/,
                invalidLoop /*Loop points are invalid (loopStart after loopEnd or loopStart and loopEnd are on same place)*/,
                loopStart_hit /*loopStart entry was hited in previous tick*/;
        char ____padding2[2];
        OPL3 opl;
    public:
        static uint64_t ReadBEint(const void *buffer, size_t nbytes)
        {
            uint64_t result = 0;
            const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

            for(unsigned n = 0; n < nbytes; ++n)
                result = (result << 8) + data[n];

            return result;
        }
        static uint64_t ReadLEint(const void *buffer, size_t nbytes)
        {
            uint64_t result = 0;
            const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

            for(unsigned n = 0; n < nbytes; ++n)
                result = result + static_cast<uint64_t>(data[n] << (n * 8));

            return result;
        }
        uint64_t ReadVarLen(size_t tk)
        {
            uint64_t result = 0;

            for(;;)
            {
                unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
                result = (result << 7) + (byte & 0x7F);

                if(!(byte & 0x80)) break;
            }

            return result;
        }
        uint64_t ReadVarLenEx(size_t tk, bool &ok)
        {
            uint64_t result = 0;
            ok = false;

            for(;;)
            {
                if(tk >= TrackData.size())
                    return 1;

                if(tk >= CurrentPosition.track.size())
                    return 2;

                size_t ptr = CurrentPosition.track[tk].ptr;

                if(ptr >= TrackData[tk].size())
                    return 3;

                unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
                result = (result << 7) + (byte & 0x7F);

                if(!(byte & 0x80)) break;
            }

            ok = true;
            return result;
        }

        /*
         * A little class gives able to read filedata from disk and also from a memory segment
        */
        class fileReader
        {
            public:
                enum relTo
                {
                    SET = 0,
                    CUR = 1,
                    END = 2
                };

                fileReader()
                {
                    fp = NULL;
                    mp = NULL;
                    mp_size = 0;
                    mp_tell = 0;
                }
                ~fileReader()
                {
                    close();
                }

                void openFile(const char *path)
                {
                    fp = std::fopen(path, "rb");
                    _fileName = path;
                    mp = NULL;
                    mp_size = 0;
                    mp_tell = 0;
                }

                void openData(void *mem, size_t lenght)
                {
                    fp = NULL;
                    mp = mem;
                    mp_size = lenght;
                    mp_tell = 0;
                }

                void seek(long pos, int rel_to)
                {
                    if(fp)
                        std::fseek(fp, pos, rel_to);
                    else
                    {
                        switch(rel_to)
                        {
                        case SET:
                            mp_tell = static_cast<size_t>(pos);
                            break;

                        case END:
                            mp_tell = mp_size - static_cast<size_t>(pos);
                            break;

                        case CUR:
                            mp_tell = mp_tell + static_cast<size_t>(pos);
                            break;
                        }

                        if(mp_tell > mp_size)
                            mp_tell = mp_size;
                    }
                }

                inline void seeku(unsigned long pos, int rel_to)
                {
                    seek(static_cast<long>(pos), rel_to);
                }

                size_t read(void *buf, size_t num, size_t size)
                {
                    if(fp)
                        return std::fread(buf, num, size, fp);
                    else
                    {
                        size_t pos = 0;
                        size_t maxSize = static_cast<size_t>(size * num);

                        while((pos < maxSize) && (mp_tell < mp_size))
                        {
                            reinterpret_cast<unsigned char *>(buf)[pos] = reinterpret_cast<unsigned char *>(mp)[mp_tell];
                            mp_tell++;
                            pos++;
                        }

                        return pos;
                    }
                }

                int getc()
                {
                    if(fp)
                        return std::getc(fp);
                    else
                    {
                        if(mp_tell >= mp_size)
                            return -1;

                        int x = reinterpret_cast<unsigned char *>(mp)[mp_tell];
                        mp_tell++;
                        return x;
                    }
                }

                size_t tell()
                {
                    if(fp)
                        return static_cast<size_t>(std::ftell(fp));
                    else
                        return mp_tell;
                }

                void close()
                {
                    if(fp) std::fclose(fp);

                    fp = NULL;
                    mp = NULL;
                    mp_size = 0;
                    mp_tell = 0;
                }

                bool isValid()
                {
                    return (fp) || (mp);
                }

                bool eof()
                {
                    return mp_tell >= mp_size;
                }
                std::string _fileName;
                std::FILE   *fp;
                void        *mp;
                size_t      mp_size;
                size_t      mp_tell;
        };

        bool LoadMIDI(const std::string &filename)
        {
            fileReader file;
            file.openFile(filename.c_str());

            if(!LoadMIDI(file))
            {
                std::perror(filename.c_str());
                return false;
            }

            return true;
        }

        bool LoadMIDI(void *data, unsigned long size)
        {
            fileReader file;
            file.openData(data, size);
            return LoadMIDI(file);
        }

        bool LoadMIDI(fileReader &fr)
        {
#define qqq(x) (void)x
            size_t  fsize;
            qqq(fsize);

            //std::FILE* fr = std::fopen(filename.c_str(), "rb");
            if(!fr.isValid())
            {
                ADLMIDI_ErrorString = "Invalid data stream!";
                return false;
            }

            const size_t HeaderSize = 4 + 4 + 2 + 2 + 2; // 14
            char HeaderBuf[HeaderSize] = "";
riffskip:
            ;
            fsize = fr.read(HeaderBuf, 1, HeaderSize);

            if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
            {
                fr.seek(6l, SEEK_CUR);
                goto riffskip;
            }

            size_t DeltaTicks = 192, TrackCount = 1;
            config->stored_samples = 0;
            config->backup_samples_size = 0;
            opl.AdlPercussionMode = config->AdlPercussionMode;
            opl.HighTremoloMode = config->HighTremoloMode;
            opl.HighVibratoMode = config->HighVibratoMode;
            opl.ScaleModulators = config->ScaleModulators;
            opl.LogarithmicVolumes = config->LogarithmicVolumes;
            opl.NumCards    = config->NumCards;
            opl.NumFourOps  = config->NumFourOps;
            cmf_percussion_mode = false;
            opl.Reset();
            trackStart = true;
            loopStart  = true;
            loopStart_passed = false;
            invalidLoop = false;
            loopStart_hit = false;
            bool is_GMF = false; // GMD/MUS files (ScummVM)
            bool is_MUS = false; // MUS/DMX files (Doom)
            bool is_IMF = false; // IMF
            bool is_CMF = false; // Creative Music format (CMF/CTMF)
            //std::vector<unsigned char> MUS_instrumentList;

            if(std::memcmp(HeaderBuf, "GMF\x1", 4) == 0)
            {
                // GMD/MUS files (ScummVM)
                fr.seek(7 - static_cast<long>(HeaderSize), SEEK_CUR);
                is_GMF = true;
            }
            else if(std::memcmp(HeaderBuf, "MUS\x1A", 4) == 0)
            {
                // MUS/DMX files (Doom)
                uint64_t start = ReadLEint(HeaderBuf + 6, 2);
                is_MUS = true;
                fr.seek(static_cast<long>(start), SEEK_SET);
            }
            else if(std::memcmp(HeaderBuf, "CTMF", 4) == 0)
            {
                opl.dynamic_instruments.clear();
                opl.dynamic_metainstruments.clear();
                // Creative Music Format (CMF).
                // When playing CTMF files, use the following commandline:
                // adlmidi song8.ctmf -p -v 1 1 0
                // i.e. enable percussion mode, deeper vibrato, and use only 1 card.
                is_CMF = true;
                //unsigned version   = ReadLEint(HeaderBuf+4, 2);
                uint64_t ins_start = ReadLEint(HeaderBuf + 6, 2);
                uint64_t mus_start = ReadLEint(HeaderBuf + 8, 2);
                //unsigned deltas    = ReadLEint(HeaderBuf+10, 2);
                uint64_t ticks     = ReadLEint(HeaderBuf + 12, 2);
                // Read title, author, remarks start offsets in file
                fr.read(HeaderBuf, 1, 6);
                //unsigned long notes_starts[3] = {ReadLEint(HeaderBuf+0,2),ReadLEint(HeaderBuf+0,4),ReadLEint(HeaderBuf+0,6)};
                fr.seek(16, SEEK_CUR); // Skip the channels-in-use table
                fr.read(HeaderBuf, 1, 4);
                uint64_t ins_count =  ReadLEint(HeaderBuf + 0, 2); //, basictempo = ReadLEint(HeaderBuf+2, 2);
                fr.seek(static_cast<long>(ins_start), SEEK_SET);

                //std::printf("%u instruments\n", ins_count);
                for(unsigned i = 0; i < ins_count; ++i)
                {
                    unsigned char InsData[16];
                    fr.read(InsData, 1, 16);
                    /*std::printf("Ins %3u: %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X\n",
                        i, InsData[0],InsData[1],InsData[2],InsData[3], InsData[4],InsData[5],InsData[6],InsData[7],
                           InsData[8],InsData[9],InsData[10],InsData[11], InsData[12],InsData[13],InsData[14],InsData[15]);*/
                    struct adldata    adl;
                    struct adlinsdata adlins;
                    adl.modulator_E862 =
                        ((static_cast<uint32_t>(InsData[8] & 0x07) << 24) & 0xFF000000) //WaveForm
                        | ((static_cast<uint32_t>(InsData[6]) << 16) & 0x00FF0000) //Sustain/Release
                        | ((static_cast<uint32_t>(InsData[4]) << 8) & 0x0000FF00) //Attack/Decay
                        | ((static_cast<uint32_t>(InsData[0]) << 0) & 0x000000FF); //MultKEVA
                    adl.carrier_E862 =
                        ((static_cast<uint32_t>(InsData[9] & 0x07) << 24) & 0xFF000000) //WaveForm
                        | ((static_cast<uint32_t>(InsData[7]) << 16) & 0x00FF0000) //Sustain/Release
                        | ((static_cast<uint32_t>(InsData[5]) << 8) & 0x0000FF00) //Attack/Decay
                        | ((static_cast<uint32_t>(InsData[1]) << 0) & 0x000000FF); //MultKEVA
                    adl.modulator_40 = InsData[2];
                    adl.carrier_40   = InsData[3];
                    adl.feedconn     = InsData[10] & 0x0F;
                    adl.finetune = 0;
                    adlins.adlno1 = static_cast<uint16_t>(opl.dynamic_instruments.size() | opl.DynamicInstrumentTag);
                    adlins.adlno2 = adlins.adlno1;
                    adlins.ms_sound_kon  = 1000;
                    adlins.ms_sound_koff = 500;
                    adlins.tone  = 0;
                    adlins.flags = 0;
                    adlins.fine_tune = 0.0;
                    opl.dynamic_metainstruments.push_back(adlins);
                    opl.dynamic_instruments.push_back(adl);
                }

                fr.seeku(mus_start, SEEK_SET);
                TrackCount = 1;
                DeltaTicks = ticks;
                opl.AdlBank    = ~0u; // Ignore AdlBank number, use dynamic banks instead
                //std::printf("CMF deltas %u ticks %u, basictempo = %u\n", deltas, ticks, basictempo);
                opl.LogarithmicVolumes = true;
                opl.AdlPercussionMode = true;
            }
            else
            {
                // Try parsing as an IMF file
                {
                    size_t end = static_cast<uint8_t>(HeaderBuf[0]) + 256 * static_cast<uint8_t>(HeaderBuf[1]);

                    if(!end || (end & 3))
                        goto not_imf;

                    size_t backup_pos = fr.tell();
                    int64_t sum1 = 0, sum2 = 0;
                    fr.seek(2, SEEK_SET);

                    for(unsigned n = 0; n < 42; ++n)
                    {
                        int64_t value1 = fr.getc();
                        value1 += fr.getc() << 8;
                        sum1 += value1;
                        int64_t value2 = fr.getc();
                        value2 += fr.getc() << 8;
                        sum2 += value2;
                    }

                    fr.seek(static_cast<long>(backup_pos), SEEK_SET);

                    if(sum1 > sum2)
                    {
                        is_IMF = true;
                        DeltaTicks = 1;
                    }
                }

                if(!is_IMF)
                {
not_imf:

                    if(std::memcmp(HeaderBuf, "MThd\0\0\0\6", 8) != 0)
                    {
InvFmt:
                        fr.close();
                        ADLMIDI_ErrorString = fr._fileName + ": Invalid format\n";
                        return false;
                    }

                    /*size_t  Fmt =*/ ReadBEint(HeaderBuf + 8,  2);
                    TrackCount = ReadBEint(HeaderBuf + 10, 2);
                    DeltaTicks = ReadBEint(HeaderBuf + 12, 2);
                }
            }

            TrackData.clear();
            TrackData.resize(TrackCount, std::vector<uint8_t>());
            CurrentPosition.track.clear();
            CurrentPosition.track.resize(TrackCount);
            InvDeltaTicks = fraction<long>(1, 1000000l * static_cast<long>(DeltaTicks));
            //Tempo       = 1000000l * InvDeltaTicks;
            Tempo         = fraction<long>(1,            static_cast<long>(DeltaTicks));
            static const unsigned char EndTag[4] = {0xFF, 0x2F, 0x00, 0x00};
            int totalGotten = 0;

            for(size_t tk = 0; tk < TrackCount; ++tk)
            {
                // Read track header
                size_t TrackLength;

                if(is_IMF)
                {
                    //std::fprintf(stderr, "Reading IMF file...\n");
                    size_t end = static_cast<uint8_t>(HeaderBuf[0]) + 256 * static_cast<uint8_t>(HeaderBuf[1]);
                    unsigned IMF_tempo = 1428;
                    static const unsigned char imf_tempo[] = {0xFF, 0x51, 0x4,
                                                              static_cast<uint8_t>(IMF_tempo >> 24),
                                                              static_cast<uint8_t>(IMF_tempo >> 16),
                                                              static_cast<uint8_t>(IMF_tempo >> 8),
                                                              static_cast<uint8_t>(IMF_tempo)
                                                             };
                    TrackData[tk].insert(TrackData[tk].end(), imf_tempo, imf_tempo + sizeof(imf_tempo));
                    TrackData[tk].push_back(0x00);
                    fr.seek(2, SEEK_SET);

                    while(fr.tell() < end && !fr.eof())
                    {
                        uint8_t special_event_buf[5];
                        special_event_buf[0] = 0xFF;
                        special_event_buf[1] = 0xE3;
                        special_event_buf[2] = 0x02;
                        special_event_buf[3] = static_cast<uint8_t>(fr.getc()); // port index
                        special_event_buf[4] = static_cast<uint8_t>(fr.getc()); // port value
                        uint32_t delay = static_cast<uint16_t>(fr.getc());
                        delay += 256 * static_cast<uint16_t>(fr.getc());
                        totalGotten += 4;
                        //if(special_event_buf[3] <= 8) continue;
                        //fprintf(stderr, "Put %02X <- %02X, plus %04X delay\n", special_event_buf[3],special_event_buf[4], delay);
                        TrackData[tk].insert(TrackData[tk].end(), special_event_buf, special_event_buf + 5);

                        //if(delay>>21) TrackData[tk].push_back( 0x80 | ((delay>>21) & 0x7F ) );
                        if(delay >> 14)
                            TrackData[tk].push_back(0x80 | ((delay >> 14) & 0x7F));

                        if(delay >> 7)
                            TrackData[tk].push_back(0x80 | ((delay >> 7) & 0x7F));

                        TrackData[tk].push_back(((delay >> 0) & 0x7F));
                    }

                    TrackData[tk].insert(TrackData[tk].end(), EndTag + 0, EndTag + 4);
                    CurrentPosition.track[tk].delay = 0;
                    CurrentPosition.began = true;
                    //std::fprintf(stderr, "Done reading IMF file\n");
                    opl.NumFourOps = 0; //Don't use 4-operator channels for IMF playing!
                }
                else
                {
                    if(is_GMF || is_CMF) // Take the rest of the file
                    {
                        size_t pos = fr.tell();
                        fr.seek(0, SEEK_END);
                        TrackLength = fr.tell() - pos;
                        fr.seek(static_cast<long>(pos), SEEK_SET);
                    }
                    else if(is_MUS) // Read TrackLength from file position 4
                    {
                        size_t pos = fr.tell();
                        fr.seek(4, SEEK_SET);
                        TrackLength = static_cast<size_t>(fr.getc());
                        TrackLength += static_cast<size_t>(fr.getc() << 8);
                        fr.seek(static_cast<long>(pos), SEEK_SET);
                    }
                    else
                    {
                        fsize = fr.read(HeaderBuf, 1, 8);

                        if(std::memcmp(HeaderBuf, "MTrk", 4) != 0)
                            goto InvFmt;

                        TrackLength = ReadBEint(HeaderBuf + 4, 4);
                    }

                    // Read track data
                    TrackData[tk].resize(TrackLength);
                    fsize = fr.read(&TrackData[tk][0], 1, TrackLength);
                    totalGotten += fsize;

                    if(is_GMF || is_MUS) // Note: CMF does include the track end tag.
                        TrackData[tk].insert(TrackData[tk].end(), EndTag + 0, EndTag + 4);

                    bool ok = false;
                    // Read next event time
                    uint64_t tkDelay = ReadVarLenEx(tk, ok);

                    if(ok)
                        CurrentPosition.track[tk].delay = static_cast<long>(tkDelay);
                    else
                    {
                        std::stringstream msg;
                        msg << fr._fileName << ": invalid variable length in the track " << tk << "! (error code " << tkDelay << ")";
                        ADLMIDI_ErrorString = msg.str();
                        return false;
                    }
                }
            }

            for(size_t tk = 0; tk < TrackCount; ++tk)
                totalGotten += TrackData[tk].size();

            if(totalGotten == 0)
            {
                ADLMIDI_ErrorString = fr._fileName + ": Empty track data";
                return false;
            }

            opl.Reset(); // Reset AdLib
            //opl.Reset(); // ...twice (just in case someone misprogrammed OPL3 previously)
            ch.clear();
            ch.resize(opl.NumChannels);
            return true;
        }

        /* Periodic tick handler.
         *   Input: s           = seconds since last call
         *   Input: granularity = don't expect intervals smaller than this, in seconds
         *   Output: desired number of seconds until next call
         */
        double Tick(double s, double granularity)
        {
            if(CurrentPosition.began) CurrentPosition.wait -= s;

            int AntiFreezeCounter = 10000;//Limit 10000 loops to avoid freezing

            while((CurrentPosition.wait <= granularity * 0.5) && (AntiFreezeCounter > 0))
            {
                //std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
                ProcessEvents();

                if(CurrentPosition.wait <= 0.0)
                    AntiFreezeCounter--;
            }

            if(AntiFreezeCounter <= 0)
                CurrentPosition.wait += 1.0;/* Add extra 1 second when over 10000 events

                                           with zero delay are been detected */

            for(uint16_t c = 0; c < opl.NumChannels; ++c)
                ch[c].AddAge(static_cast<int64_t>(s * 1000.0));

            UpdateVibrato(s);
            UpdateArpeggio(s);
            return CurrentPosition.wait;
        }

    private:
        enum { Upd_Patch  = 0x1,
               Upd_Pan    = 0x2,
               Upd_Volume = 0x4,
               Upd_Pitch  = 0x8,
               Upd_All    = Upd_Pan + Upd_Volume + Upd_Pitch,
               Upd_Off    = 0x20
             };

        void NoteUpdate
        (uint16_t MidCh,
         MIDIchannel::activenoteiterator i,
         unsigned props_mask,
         int32_t select_adlchn = -1)
        {
            MIDIchannel::NoteInfo &info = i->second;
            const int16_t tone    = info.tone;
            const uint8_t vol     = info.vol;
            //const int midiins = info.midiins;
            const uint32_t insmeta = info.insmeta;
            const adlinsdata &ains = opl.GetAdlMetaIns(insmeta);
            AdlChannel::Location my_loc;
            my_loc.MidCh = MidCh;
            my_loc.note  = i->first;

            for(std::map<uint16_t, uint16_t>::iterator
                jnext = info.phys.begin();
                jnext != info.phys.end();
               )
            {
                std::map<uint16_t, uint16_t>::iterator j(jnext++);
                uint16_t c   = j->first;
                uint16_t ins = j->second;

                if(select_adlchn >= 0 && c != select_adlchn) continue;

                if(props_mask & Upd_Patch)
                {
                    opl.Patch(c, ins);
                    AdlChannel::LocationData &d = ch[c].users[my_loc];
                    d.sustained = false; // inserts if necessary
                    d.vibdelay  = 0;
                    d.kon_time_until_neglible = ains.ms_sound_kon;
                    d.ins       = ins;
                }
            }

            for(std::map<unsigned short, unsigned short>::iterator
                jnext = info.phys.begin();
                jnext != info.phys.end();
               )
            {
                std::map<unsigned short, unsigned short>::iterator j(jnext++);
                uint16_t c   = j->first;
                uint16_t ins = j->second;

                if(select_adlchn >= 0 && c != select_adlchn) continue;

                if(props_mask & Upd_Off) // note off
                {
                    if(Ch[MidCh].sustain == 0)
                    {
                        AdlChannel::users_t::iterator k = ch[c].users.find(my_loc);

                        if(k != ch[c].users.end())
                            ch[c].users.erase(k);

                        //UI.IllustrateNote(c, tone, midiins, 0, 0.0);

                        if(ch[c].users.empty())
                        {
                            opl.NoteOff(c);
                            ch[c].koff_time_until_neglible =
                                ains.ms_sound_koff;
                        }
                    }
                    else
                    {
                        // Sustain: Forget about the note, but don't key it off.
                        //          Also will avoid overwriting it very soon.
                        AdlChannel::LocationData &d = ch[c].users[my_loc];
                        d.sustained = true; // note: not erased!
                        //UI.IllustrateNote(c, tone, midiins, -1, 0.0);
                    }

                    info.phys.erase(j);
                    continue;
                }

                if(props_mask & Upd_Pan)
                    opl.Pan(c, Ch[MidCh].panning);

                if(props_mask & Upd_Volume)
                {
                    uint32_t volume = vol * Ch[MidCh].volume * Ch[MidCh].expression;
                    /* If the channel has arpeggio, the effective volume of
                     * *this* instrument is actually lower due to timesharing.
                     * To compensate, add extra volume that corresponds to the
                     * time this note is *not* heard.
                     * Empirical tests however show that a full equal-proportion
                     * increment sounds wrong. Therefore, using the square root.
                     */
                    //volume = (int)(volume * std::sqrt( (double) ch[c].users.size() ));
                    opl.Touch(c, volume);
                }

                if(props_mask & Upd_Pitch)
                {
                    AdlChannel::LocationData &d = ch[c].users[my_loc];

                    // Don't bend a sustained note
                    if(!d.sustained)
                    {
                        double bend = Ch[MidCh].bend + opl.GetAdlIns(ins).finetune;
                        double phase = 0.0;

                        if((ains.flags & adlinsdata::Flag_Pseudo4op) && ins == ains.adlno2)
                        {
                            phase = ains.fine_tune;//0.125; // Detune the note slightly (this is what Doom does)
                        }

                        if(Ch[MidCh].vibrato && d.vibdelay >= Ch[MidCh].vibdelay)
                            bend += Ch[MidCh].vibrato * Ch[MidCh].vibdepth * std::sin(Ch[MidCh].vibpos);

#ifdef ADLMIDI_USE_DOSBOX_OPL
#define BEND_COEFFICIENT 172.00093
#else
#define BEND_COEFFICIENT 172.4387
#endif
                        opl.NoteOn(c, BEND_COEFFICIENT * std::exp(0.057762265 * (tone + bend + phase)));
#undef BEND_COEFFICIENT
                    }
                }
            }

            if(info.phys.empty())
                Ch[MidCh].activenotes.erase(i);
        }

        void ProcessEvents()
        {
            loopEnd = false;
            const size_t TrackCount = TrackData.size();
            const Position RowBeginPosition(CurrentPosition);

            for(size_t tk = 0; tk < TrackCount; ++tk)
            {
                if(CurrentPosition.track[tk].status >= 0
                   && CurrentPosition.track[tk].delay <= 0)
                {
                    // Handle event
                    HandleEvent(tk);

                    // Read next event time (unless the track just ended)
                    if(CurrentPosition.track[tk].ptr >= TrackData[tk].size())
                        CurrentPosition.track[tk].status = -1;

                    if(CurrentPosition.track[tk].status >= 0)
                        CurrentPosition.track[tk].delay += ReadVarLen(tk);
                }
            }

            // Find shortest delay from all track
            long shortest = -1;

            for(size_t tk = 0; tk < TrackCount; ++tk)
                if(CurrentPosition.track[tk].status >= 0
                   && (shortest == -1
                       || CurrentPosition.track[tk].delay < shortest))
                    shortest = CurrentPosition.track[tk].delay;

            //if(shortest > 0) UI.PrintLn("shortest: %ld", shortest);

            // Schedule the next playevent to be processed after that delay
            for(size_t tk = 0; tk < TrackCount; ++tk)
                CurrentPosition.track[tk].delay -= shortest;

            fraction<long> t = shortest * Tempo;

            if(CurrentPosition.began) CurrentPosition.wait += t.valuel();

            //if(shortest > 0) UI.PrintLn("Delay %ld (%g)", shortest, (double)t.valuel());

            /*
            if(CurrentPosition.track[0].ptr > 8119) loopEnd = true;
            // ^HACK: CHRONO TRIGGER LOOP
            */

            if(loopStart_hit && (loopStart || loopEnd)) //Avoid invalid loops
            {
                invalidLoop = true;
                loopStart = false;
                loopEnd = false;
                LoopBeginPosition = trackBeginPosition;
            }
            else
                loopStart_hit = false;

            if(loopStart)
            {
                if(trackStart)
                {
                    trackBeginPosition = RowBeginPosition;
                    trackStart = false;
                }

                LoopBeginPosition = RowBeginPosition;
                loopStart = false;
                loopStart_hit = true;
            }

            if(shortest < 0 || loopEnd)
            {
                // Loop if song end reached
                loopEnd         = false;
                CurrentPosition = LoopBeginPosition;
                shortest = 0;

                if(opl._parent->QuitWithoutLooping == 1)
                {
                    opl._parent->QuitFlag = 1;
                    //^ HACK: QUIT WITHOUT LOOPING
                }
            }
        }

        void HandleEvent(size_t tk)
        {
            unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];

            if(byte == 0xF7 || byte == 0xF0) // Ignore SysEx
            {
                uint64_t length = ReadVarLen(tk);
                //std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
                CurrentPosition.track[tk].ptr += length;
                //UI.PrintLn("SysEx %02X: %u bytes", byte, length/*, data.c_str()*/);
                return;
            }

            if(byte == 0xFF)
            {
                // Special event FF
                unsigned char evtype = TrackData[tk][CurrentPosition.track[tk].ptr++];
                unsigned long length = ReadVarLen(tk);
                std::string data(length ? (const char *) &TrackData[tk][CurrentPosition.track[tk].ptr] : 0, length);
                CurrentPosition.track[tk].ptr += length;

                if(evtype == 0x2F)
                {
                    CurrentPosition.track[tk].status = -1;
                    return;
                }

                if(evtype == 0x51)
                {
                    Tempo = InvDeltaTicks * fraction<long>((long) ReadBEint(data.data(), data.size()));
                    return;
                }

                if(evtype == 6)
                {
                    for(size_t i = 0; i < data.size(); i++)
                    {
                        if(data[i] <= 'Z' && data[i] >= 'A')
                            data[i] = data[i] - ('Z' - 'z');
                    }

                    if((data == "loopstart") && (!invalidLoop))
                    {
                        loopStart = true;
                        loopStart_passed = true;
                    }

                    if((data == "loopend") && (!invalidLoop))
                    {
                        if((loopStart_passed) && (!loopStart))
                            loopEnd = true;
                        else
                            invalidLoop = true;
                    }
                }

                if(evtype == 9)
                    current_device[tk] = ChooseDevice(data);

                //if(evtype >= 1 && evtype <= 6)
                //    UI.PrintLn("Meta %d: %s", evtype, data.c_str());

                if(evtype == 0xE3) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
                {
                    uint8_t i = static_cast<uint8_t>(data[0]), v = static_cast<uint8_t>(data[1]);

                    if((i & 0xF0) == 0xC0)
                        v |= 0x30;

                    //std::printf("OPL poke %02X, %02X\n", i, v);
                    //std::fflush(stdout);
                    opl.PokeN(0, i, v);
                }

                return;
            }

            // Any normal event (80..EF)
            if(byte < 0x80)
            {
                byte = static_cast<uint8_t>(CurrentPosition.track[tk].status | 0x80);
                CurrentPosition.track[tk].ptr--;
            }

            if(byte == 0xF3)
            {
                CurrentPosition.track[tk].ptr += 1;
                return;
            }

            if(byte == 0xF2)
            {
                CurrentPosition.track[tk].ptr += 2;
                return;
            }

            /*UI.PrintLn("@%X Track %u: %02X %02X",
                CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte,
                TrackData[tk][CurrentPosition.track[tk].ptr]);*/
            uint8_t  MidCh = byte & 0x0F, EvType = byte >> 4;
            MidCh += current_device[tk];
            CurrentPosition.track[tk].status = byte;

            switch(EvType)
            {
            case 0x8: // Note off
            case 0x9: // Note on
            {
                uint8_t note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                uint8_t vol  = TrackData[tk][CurrentPosition.track[tk].ptr++];
                //if(MidCh != 9) note -= 12; // HACK
                NoteOff(MidCh, note);

                // On Note on, Keyoff the note first, just in case keyoff
                // was omitted; this fixes Dance of sugar-plum fairy
                // by Microsoft. Now that we've done a Keyoff,
                // check if we still need to do a Keyon.
                // vol=0 and event 8x are both Keyoff-only.
                if(vol == 0 || EvType == 0x8) break;

                uint8_t midiins = Ch[MidCh].patch;

                if(MidCh % 16 == 9)
                    midiins = 128 + note; // Percussion instrument

                /*
                if(MidCh%16 == 9 || (midiins != 32 && midiins != 46 && midiins != 48 && midiins != 50))
                    break; // HACK
                if(midiins == 46) vol = (vol*7)/10;          // HACK
                if(midiins == 48 || midiins == 50) vol /= 4; // HACK
                */
                //if(midiins == 56) vol = vol*6/10; // HACK
                static std::set<uint32_t> bank_warnings;

                if(Ch[MidCh].bank_msb)
                {
                    uint32_t bankid = midiins + 256 * Ch[MidCh].bank_msb;
                    std::set<uint32_t>::iterator
                    i = bank_warnings.lower_bound(bankid);

                    if(i == bank_warnings.end() || *i != bankid)
                    {
                        ADLMIDI_ErrorString.clear();
                        std::stringstream s;
                        s << "[" << MidCh << "]Bank " << Ch[MidCh].bank_msb <<
                          " undefined, patch=" << ((midiins & 128) ? 'P' : 'M') << (midiins & 127);
                        ADLMIDI_ErrorString = s.str();
                        bank_warnings.insert(i, bankid);
                    }
                }

                if(Ch[MidCh].bank_lsb)
                {
                    unsigned bankid = Ch[MidCh].bank_lsb * 65536;
                    std::set<unsigned>::iterator
                    i = bank_warnings.lower_bound(bankid);

                    if(i == bank_warnings.end() || *i != bankid)
                    {
                        ADLMIDI_ErrorString.clear();
                        std::stringstream s;
                        s << "[" << MidCh << "]Bank lsb " << Ch[MidCh].bank_lsb << " undefined";
                        ADLMIDI_ErrorString = s.str();
                        bank_warnings.insert(i, bankid);
                    }
                }

                //int meta = banks[opl.AdlBank][midiins];
                const uint32_t      meta   = opl.GetAdlMetaNumber(midiins);
                const adlinsdata    &ains  = opl.GetAdlMetaIns(meta);
                int16_t tone = note;

                if(ains.tone)
                {
                    if(ains.tone < 20)
                        tone += ains.tone;
                    else if(ains.tone < 128)
                        tone = ains.tone;
                    else
                        tone -= ains.tone - 128;
                }

                uint16_t i[2] = { ains.adlno1, ains.adlno2 };
                bool pseudo_4op = ains.flags & adlinsdata::Flag_Pseudo4op;

                if((opl.AdlPercussionMode == 1) && PercussionMap[midiins & 0xFF]) i[1] = i[0];

                static std::set<uint8_t> missing_warnings;

                if(!missing_warnings.count(static_cast<uint8_t>(midiins)) && (ains.flags & adlinsdata::Flag_NoSound))
                {
                    //UI.PrintLn("[%i]Playing missing instrument %i", MidCh, midiins);
                    missing_warnings.insert(static_cast<uint8_t>(midiins));
                }

                // Allocate AdLib channel (the physical sound channel for the note)
                int32_t adlchannel[2] = { -1, -1 };

                for(uint32_t ccount = 0; ccount < 2; ++ccount)
                {
                    if(ccount == 1)
                    {
                        if(i[0] == i[1]) break; // No secondary channel

                        if(adlchannel[0] == -1)
                            break; // No secondary if primary failed
                    }

                    int32_t c = -1;
                    long bs = -0x7FFFFFFFl;

                    for(uint32_t a = 0; a < opl.NumChannels; ++a)
                    {
                        if(ccount == 1 && static_cast<int32_t>(a) == adlchannel[0]) continue;

                        // ^ Don't use the same channel for primary&secondary

                        if(i[0] == i[1] || pseudo_4op)
                        {
                            // Only use regular channels
                            uint8_t expected_mode = 0;

                            if(opl.AdlPercussionMode == 1)
                            {
                                if(cmf_percussion_mode)
                                    expected_mode = MidCh < 11 ? 0 : (3 + MidCh - 11); // CMF
                                else
                                    expected_mode = PercussionMap[midiins & 0xFF];
                            }

                            if(opl.four_op_category[a] != expected_mode)
                                continue;
                        }
                        else
                        {
                            if(ccount == 0)
                            {
                                // Only use four-op master channels
                                if(opl.four_op_category[a] != 1)
                                    continue;
                            }
                            else
                            {
                                // The secondary must be played on a specific channel.
                                if(a != static_cast<uint32_t>(adlchannel[0]) + 3)
                                    continue;
                            }
                        }

                        long s = CalculateAdlChannelGoodness(a, i[ccount], MidCh);

                        if(s > bs)
                        {
                            bs = s;    // Best candidate wins
                            c = static_cast<int32_t>(a);
                        }
                    }

                    if(c < 0)
                    {
                        //UI.PrintLn("ignored unplaceable note");
                        continue; // Could not play this note. Ignore it.
                    }

                    PrepareAdlChannelForNewNote(static_cast<size_t>(c), i[ccount]);
                    adlchannel[ccount] = c;
                }

                if(adlchannel[0] < 0 && adlchannel[1] < 0)
                {
                    // The note could not be played, at all.
                    break;
                }

                //UI.PrintLn("i1=%d:%d, i2=%d:%d", i[0],adlchannel[0], i[1],adlchannel[1]);
                // Allocate active note for MIDI channel
                std::pair<MIDIchannel::activenoteiterator, bool>
                ir = Ch[MidCh].activenotes.insert(
                         std::make_pair(note, MIDIchannel::NoteInfo()));
                ir.first->second.vol     = vol;
                ir.first->second.tone    = tone;
                ir.first->second.midiins = midiins;
                ir.first->second.insmeta = meta;

                for(unsigned ccount = 0; ccount < 2; ++ccount)
                {
                    int32_t c = adlchannel[ccount];

                    if(c < 0)
                        continue;

                    ir.first->second.phys[ static_cast<uint16_t>(adlchannel[ccount]) ] = i[ccount];
                }

                CurrentPosition.began  = true;
                NoteUpdate(MidCh, ir.first, Upd_All | Upd_Patch);
                break;
            }

            case 0xA: // Note touch
            {
                uint8_t note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                uint8_t vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                MIDIchannel::activenoteiterator
                i = Ch[MidCh].activenotes.find(note);

                if(i == Ch[MidCh].activenotes.end())
                {
                    // Ignore touch if note is not active
                    break;
                }

                i->second.vol = vol;
                NoteUpdate(MidCh, i, Upd_Volume);
                break;
            }

            case 0xB: // Controller change
            {
                uint8_t ctrlno = TrackData[tk][CurrentPosition.track[tk].ptr++];
                uint8_t value = TrackData[tk][CurrentPosition.track[tk].ptr++];

                switch(ctrlno)
                {
                case 1: // Adjust vibrato
                    //UI.PrintLn("%u:vibrato %d", MidCh,value);
                    Ch[MidCh].vibrato = value;
                    break;

                case 0: // Set bank msb (GM bank)
                    Ch[MidCh].bank_msb = value;
                    break;

                case 32: // Set bank lsb (XG bank)
                    Ch[MidCh].bank_lsb = value;
                    break;

                case 5: // Set portamento msb
                    Ch[MidCh].portamento = static_cast<uint16_t>((Ch[MidCh].portamento & 0x7F) | (value << 7));
                    //UpdatePortamento(MidCh);
                    break;

                case 37: // Set portamento lsb
                    Ch[MidCh].portamento = (Ch[MidCh].portamento & 0x3F80) | (value);
                    //UpdatePortamento(MidCh);
                    break;

                case 65: // Enable/disable portamento
                    // value >= 64 ? enabled : disabled
                    //UpdatePortamento(MidCh);
                    break;

                case 7: // Change volume
                    Ch[MidCh].volume = value;
                    NoteUpdate_All(MidCh, Upd_Volume);
                    break;

                case 64: // Enable/disable sustain
                    Ch[MidCh].sustain = value;

                    if(!value) KillSustainingNotes(MidCh);

                    break;

                case 11: // Change expression (another volume factor)
                    Ch[MidCh].expression = value;
                    NoteUpdate_All(MidCh, Upd_Volume);
                    break;

                case 10: // Change panning
                    Ch[MidCh].panning = 0x00;

                    if(value  < 64 + 32) Ch[MidCh].panning |= 0x10;

                    if(value >= 64 - 32) Ch[MidCh].panning |= 0x20;

                    NoteUpdate_All(MidCh, Upd_Pan);
                    break;

                case 121: // Reset all controllers
                    Ch[MidCh].bend       = 0;
                    Ch[MidCh].volume     = 100;
                    Ch[MidCh].expression = 100;
                    Ch[MidCh].sustain    = 0;
                    Ch[MidCh].vibrato    = 0;
                    Ch[MidCh].vibspeed   = 2 * 3.141592653 * 5.0;
                    Ch[MidCh].vibdepth   = 0.5 / 127;
                    Ch[MidCh].vibdelay   = 0;
                    Ch[MidCh].panning    = 0x30;
                    Ch[MidCh].portamento = 0;
                    //UpdatePortamento(MidCh);
                    NoteUpdate_All(MidCh, Upd_Pan + Upd_Volume + Upd_Pitch);
                    // Kill all sustained notes
                    KillSustainingNotes(MidCh);
                    break;

                case 123: // All notes off
                    NoteUpdate_All(MidCh, Upd_Off);
                    break;

                case 91:
                    break; // Reverb effect depth. We don't do per-channel reverb.

                case 92:
                    break; // Tremolo effect depth. We don't do...

                case 93:
                    break; // Chorus effect depth. We don't do.

                case 94:
                    break; // Celeste effect depth. We don't do.

                case 95:
                    break; // Phaser effect depth. We don't do.

                case 98:
                    Ch[MidCh].lastlrpn = value;
                    Ch[MidCh].nrpn = true;
                    break;

                case 99:
                    Ch[MidCh].lastmrpn = value;
                    Ch[MidCh].nrpn = true;
                    break;

                case 100:
                    Ch[MidCh].lastlrpn = value;
                    Ch[MidCh].nrpn = false;
                    break;

                case 101:
                    Ch[MidCh].lastmrpn = value;
                    Ch[MidCh].nrpn = false;
                    break;

                case 113:
                    break; // Related to pitch-bender, used by missimp.mid in Duke3D

                case  6:
                    SetRPN(MidCh, value, true);
                    break;

                case 38:
                    SetRPN(MidCh, value, false);
                    break;

                case 103:
                    cmf_percussion_mode = value;
                    break; // CMF (ctrl 0x67) rhythm mode

                case 111://LoopStart unofficial controller
                    if(!invalidLoop)
                    {
                        loopStart = true;
                        loopStart_passed = true;
                    }

                    break;

                default:
                    break;
                    //UI.PrintLn("Ctrl %d <- %d (ch %u)", ctrlno, value, MidCh);
                }

                break;
            }

            case 0xC: // Patch change
                Ch[MidCh].patch = TrackData[tk][CurrentPosition.track[tk].ptr++];
                break;

            case 0xD: // Channel after-touch
            {
                // TODO: Verify, is this correct action?
                uint8_t vol = TrackData[tk][CurrentPosition.track[tk].ptr++];

                for(MIDIchannel::activenoteiterator
                    i = Ch[MidCh].activenotes.begin();
                    i != Ch[MidCh].activenotes.end();
                    ++i)
                {
                    // Set this pressure to all active notes on the channel
                    i->second.vol = vol;
                }

                NoteUpdate_All(MidCh, Upd_Volume);
                break;
            }

            case 0xE: // Wheel/pitch bend
            {
                int a = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int b = TrackData[tk][CurrentPosition.track[tk].ptr++];
                Ch[MidCh].bend = (a + b * 128 - 8192) * Ch[MidCh].bendsense;
                NoteUpdate_All(MidCh, Upd_Pitch);
                break;
            }
            }
        }

        // Determine how good a candidate this adlchannel
        // would be for playing a note from this instrument.
        long CalculateAdlChannelGoodness
        (unsigned c, uint16_t ins, uint16_t /*MidCh*/) const
        {
            long s = -ch[c].koff_time_until_neglible;

            // Same midi-instrument = some stability
            //if(c == MidCh) s += 4;
            for(AdlChannel::users_t::const_iterator
                j = ch[c].users.begin();
                j != ch[c].users.end();
                ++j)
            {
                s -= 4000;

                if(!j->second.sustained)
                    s -= j->second.kon_time_until_neglible;
                else
                    s -= j->second.kon_time_until_neglible / 2;

                MIDIchannel::activenotemap_t::const_iterator
                k = Ch[j->first.MidCh].activenotes.find(j->first.note);

                if(k != Ch[j->first.MidCh].activenotes.end())
                {
                    // Same instrument = good
                    if(j->second.ins == ins)
                    {
                        s += 300;

                        // Arpeggio candidate = even better
                        if(j->second.vibdelay < 70
                           || j->second.kon_time_until_neglible > 20000)
                            s += 0;
                    }

                    // Percussion is inferior to melody
                    s += 50 * (k->second.midiins / 128);
                    /*
                    if(k->second.midiins >= 25
                    && k->second.midiins < 40
                    && j->second.ins != ins)
                    {
                        s -= 14000; // HACK: Don't clobber the bass or the guitar
                    }
                    */
                }

                // If there is another channel to which this note
                // can be evacuated to in the case of congestion,
                // increase the score slightly.
                unsigned n_evacuation_stations = 0;

                for(unsigned c2 = 0; c2 < opl.NumChannels; ++c2)
                {
                    if(c2 == c) continue;

                    if(opl.four_op_category[c2]
                       != opl.four_op_category[c]) continue;

                    for(AdlChannel::users_t::const_iterator
                        m = ch[c2].users.begin();
                        m != ch[c2].users.end();
                        ++m)
                    {
                        if(m->second.sustained)       continue;

                        if(m->second.vibdelay >= 200) continue;

                        if(m->second.ins != j->second.ins) continue;

                        n_evacuation_stations += 1;
                    }
                }

                s += n_evacuation_stations * 4;
            }

            return s;
        }

        // A new note will be played on this channel using this instrument.
        // Kill existing notes on this channel (or don't, if we do arpeggio)
        void PrepareAdlChannelForNewNote(size_t c, int ins)
        {
            if(ch[c].users.empty()) return; // Nothing to do

            //bool doing_arpeggio = false;
            for(AdlChannel::users_t::iterator
                jnext = ch[c].users.begin();
                jnext != ch[c].users.end();
               )
            {
                AdlChannel::users_t::iterator j(jnext++);

                if(!j->second.sustained)
                {
                    // Collision: Kill old note,
                    // UNLESS we're going to do arpeggio
                    MIDIchannel::activenoteiterator i
                    (Ch[j->first.MidCh].activenotes.find(j->first.note));

                    // Check if we can do arpeggio.
                    if((j->second.vibdelay < 70
                        || j->second.kon_time_until_neglible > 20000)
                       && j->second.ins == ins)
                    {
                        // Do arpeggio together with this note.
                        //doing_arpeggio = true;
                        continue;
                    }

                    KillOrEvacuate(c, j, i);
                    // ^ will also erase j from ch[c].users.
                }
            }

            // Kill all sustained notes on this channel
            // Don't keep them for arpeggio, because arpeggio requires
            // an intact "activenotes" record. This is a design flaw.
            KillSustainingNotes(-1, static_cast<int32_t>(c));

            // Keyoff the channel so that it can be retriggered,
            // unless the new note will be introduced as just an arpeggio.
            if(ch[c].users.empty())
                opl.NoteOff(c);
        }

        void KillOrEvacuate(
            size_t  from_channel,
            AdlChannel::users_t::iterator j,
            MIDIchannel::activenoteiterator i)
        {
            // Before killing the note, check if it can be
            // evacuated to another channel as an arpeggio
            // instrument. This helps if e.g. all channels
            // are full of strings and we want to do percussion.
            // FIXME: This does not care about four-op entanglements.
            for(uint32_t c = 0; c < opl.NumChannels; ++c)
            {
                uint16_t cs = static_cast<uint16_t>(c);

                if(c > std::numeric_limits<uint32_t>::max())
                    break;

                if(c == from_channel)
                    continue;

                if(opl.four_op_category[c]
                   != opl.four_op_category[from_channel]
                  ) continue;

                for(AdlChannel::users_t::iterator
                    m = ch[c].users.begin();
                    m != ch[c].users.end();
                    ++m)
                {
                    if(m->second.vibdelay >= 200
                       && m->second.kon_time_until_neglible < 10000) continue;

                    if(m->second.ins != j->second.ins) continue;

                    // the note can be moved here!
                    //                UI.IllustrateNote(
                    //                    from_channel,
                    //                    i->second.tone,
                    //                    i->second.midiins, 0, 0.0);
                    //                UI.IllustrateNote(
                    //                    c,
                    //                    i->second.tone,
                    //                    i->second.midiins,
                    //                    i->second.vol,
                    //                    0.0);
                    i->second.phys.erase(static_cast<uint16_t>(from_channel));
                    i->second.phys[cs] = j->second.ins;
                    ch[cs].users.insert(*j);
                    ch[from_channel].users.erase(j);
                    return;
                }
            }

            /*UI.PrintLn(
                "collision @%u: [%ld] <- ins[%3u]",
                c,
                //ch[c].midiins<128?'M':'P', ch[c].midiins&127,
                ch[c].age, //adlins[ch[c].insmeta].ms_sound_kon,
                ins
                );*/
            // Kill it
            NoteUpdate(j->first.MidCh,
                       i,
                       Upd_Off,
                       static_cast<int32_t>(from_channel));
        }

        void KillSustainingNotes(int32_t MidCh = -1, int32_t this_adlchn = -1)
        {
            uint32_t first = 0, last = opl.NumChannels;

            if(this_adlchn >= 0)
            {
                first = static_cast<uint32_t>(this_adlchn);
                last = first + 1;
            }

            for(unsigned c = first; c < last; ++c)
            {
                if(ch[c].users.empty()) continue; // Nothing to do

                for(AdlChannel::users_t::iterator
                    jnext = ch[c].users.begin();
                    jnext != ch[c].users.end();
                   )
                {
                    AdlChannel::users_t::iterator j(jnext++);

                    if((MidCh < 0 || j->first.MidCh == MidCh)
                       && j->second.sustained)
                    {
                        //int midiins = '?';
                        //UI.IllustrateNote(c, j->first.note, midiins, 0, 0.0);
                        ch[c].users.erase(j);
                    }
                }

                // Keyoff the channel, if there are no users left.
                if(ch[c].users.empty())
                    opl.NoteOff(c);
            }
        }

        void SetRPN(unsigned MidCh, unsigned value, bool MSB)
        {
            bool nrpn = Ch[MidCh].nrpn;
            unsigned addr = Ch[MidCh].lastmrpn * 0x100 + Ch[MidCh].lastlrpn;

            switch(addr + nrpn * 0x10000 + MSB * 0x20000)
            {
            case 0x0000 + 0*0x10000 + 1*0x20000: // Pitch-bender sensitivity
                Ch[MidCh].bendsense = value / 8192.0;
                break;

            case 0x0108 + 1*0x10000 + 1*0x20000: // Vibrato speed
                if(value == 64)
                    Ch[MidCh].vibspeed = 1.0;
                else if(value < 100)
                    Ch[MidCh].vibspeed = 1.0 / (1.6e-2 * (value ? value : 1));
                else
                    Ch[MidCh].vibspeed = 1.0 / (0.051153846 * value - 3.4965385);

                Ch[MidCh].vibspeed *= 2 * 3.141592653 * 5.0;
                break;

            case 0x0109 + 1*0x10000 + 1*0x20000: // Vibrato depth
                Ch[MidCh].vibdepth = ((value - 64) * 0.15) * 0.01;
                break;

            case 0x010A + 1*0x10000 + 1*0x20000: // Vibrato delay in millisecons
                Ch[MidCh].vibdelay =
                    value ? long(0.2092 * std::exp(0.0795 * value)) : 0.0;
                break;

            default:/* UI.PrintLn("%s %04X <- %d (%cSB) (ch %u)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB], MidCh);*/
                break;
            }
        }

        //    void UpdatePortamento(unsigned MidCh)
        //    {
        //        // mt = 2^(portamento/2048) * (1.0 / 5000.0)
        //        /*
        //        double mt = std::exp(0.00033845077 * Ch[MidCh].portamento);
        //        NoteUpdate_All(MidCh, Upd_Pitch);
        //        */
        //        //UI.PrintLn("Portamento %u: %u (unimplemented)", MidCh, Ch[MidCh].portamento);
        //    }

        void NoteUpdate_All(uint16_t MidCh, unsigned props_mask)
        {
            for(MIDIchannel::activenoteiterator
                i = Ch[MidCh].activenotes.begin();
                i != Ch[MidCh].activenotes.end();
               )
            {
                MIDIchannel::activenoteiterator j(i++);
                NoteUpdate(MidCh, j, props_mask);
            }
        }

        void NoteOff(uint16_t MidCh, uint8_t note)
        {
            MIDIchannel::activenoteiterator
            i = Ch[MidCh].activenotes.find(note);

            if(i != Ch[MidCh].activenotes.end())
                NoteUpdate(MidCh, i, Upd_Off);
        }

        void UpdateVibrato(double amount)
        {
            for(size_t a = 0, b = Ch.size(); a < b; ++a)
                if(Ch[a].vibrato && !Ch[a].activenotes.empty())
                {
                    NoteUpdate_All(static_cast<uint16_t>(a), Upd_Pitch);
                    Ch[a].vibpos += amount * Ch[a].vibspeed;
                }
                else
                    Ch[a].vibpos = 0.0;
        }

        void UpdateArpeggio(double /*amount*/) // amount = amount of time passed
        {
            // If there is an adlib channel that has multiple notes
            // simulated on the same channel, arpeggio them.
#if 0
            const unsigned desired_arpeggio_rate = 40; // Hz (upper limit)
#if 1
            static unsigned cache = 0;
            amount = amount; // Ignore amount. Assume we get a constant rate.
            cache += MaxSamplesAtTime * desired_arpeggio_rate;

            if(cache < PCM_RATE) return;

            cache %= PCM_RATE;
#else
            static double arpeggio_cache = 0;
            arpeggio_cache += amount * desired_arpeggio_rate;

            if(arpeggio_cache < 1.0) return;

            arpeggio_cache = 0.0;
#endif
#endif
            static unsigned arpeggio_counter = 0;
            ++arpeggio_counter;

            for(uint32_t c = 0; c < opl.NumChannels; ++c)
            {
retry_arpeggio:

                if(c > std::numeric_limits<int32_t>::max())
                    break;

                size_t n_users = ch[c].users.size();

                if(n_users > 1)
                {
                    AdlChannel::users_t::const_iterator i = ch[c].users.begin();
                    size_t rate_reduction = 3;

                    if(n_users >= 3) rate_reduction = 2;

                    if(n_users >= 4) rate_reduction = 1;

                    std::advance(i, (arpeggio_counter / rate_reduction) % n_users);

                    if(i->second.sustained == false)
                    {
                        if(i->second.kon_time_until_neglible <= 0l)
                        {
                            NoteUpdate(
                                i->first.MidCh,
                                Ch[ i->first.MidCh ].activenotes.find(i->first.note),
                                Upd_Off,
                                static_cast<int32_t>(c));
                            goto retry_arpeggio;
                        }

                        NoteUpdate(
                            i->first.MidCh,
                            Ch[ i->first.MidCh ].activenotes.find(i->first.note),
                            Upd_Pitch | Upd_Volume | Upd_Pan,
                            static_cast<int32_t>(c));
                    }
                }
            }
        }

    public:
        uint64_t ChooseDevice(const std::string &name)
        {
            std::map<std::string, uint64_t>::iterator i = devices.find(name);

            if(i != devices.end())
                return i->second;

            size_t n = devices.size() * 16;
            devices.insert(std::make_pair(name, n));
            Ch.resize(n + 16);
            return n;
        }
};

#ifdef ADLMIDI_buildAsApp
static std::deque<short> AudioBuffer;
static MutexType AudioBuffer_lock;

static void SDL_AudioCallbackX(void *, Uint8 *stream, int len)
{
    SDL_LockAudio();
    short *target = (short *) stream;
    AudioBuffer_lock.Lock();
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len / 2; // number of shorts

    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();

    for(unsigned a = 0; a < ate; ++a)
        target[a] = AudioBuffer[a];

    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}
#endif

struct FourChars
{
    char ret[4];

    FourChars(const char *s)
    {
        for(unsigned c = 0; c < 4; ++c)
            ret[c] = s[c];
    }
    FourChars(unsigned w) // Little-endian
    {
        for(unsigned c = 0; c < 4; ++c)
            ret[c] = static_cast<int8_t>((w >>(c * 8)) & 0xFF);
    }
};

int adlRefreshNumCards(ADL_MIDIPlayer *device)
{
    unsigned n_fourop[2] = {0, 0}, n_total[2] = {0, 0};

    for(unsigned a = 0; a < 256; ++a)
    {
        unsigned insno = banks[device->AdlBank][a];

        if(insno == 198) continue;

        ++n_total[a / 128];

        if(adlins[insno].adlno1 != adlins[insno].adlno2)
            ++n_fourop[a / 128];
    }

    device->NumFourOps =
        (n_fourop[0] >= n_total[0] * 7 / 8) ? device->NumCards * 6
        : (n_fourop[0] < n_total[0] * 1 / 8) ? 0
        : (device->NumCards == 1 ? 1 : device->NumCards * 4);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.NumFourOps = device->NumFourOps;

    if(n_fourop[0] >= n_total[0] * 15 / 16 && device->NumFourOps == 0)
    {
        ADLMIDI_ErrorString = "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
                              "       The results (silence + much cpu load) would be probably\n"
                              "       not what you want, therefore ignoring the request.\n";
        return -1;
    }

    return 0;
}

/*---------------------------EXPORTS---------------------------*/

ADLMIDI_EXPORT struct ADL_MIDIPlayer *adl_init(long sample_rate)
{
    ADL_MIDIPlayer *_device;
    _device = (ADL_MIDIPlayer *)malloc(sizeof(ADL_MIDIPlayer));
    _device->PCM_RATE = static_cast<unsigned long>(sample_rate);
    _device->AdlBank    = 0;
    _device->NumFourOps = 7;
    _device->NumCards   = 2;
    _device->HighTremoloMode   = 0;
    _device->HighVibratoMode   = 0;
    _device->AdlPercussionMode = 0;
    _device->LogarithmicVolumes = 0;
    _device->QuitFlag = 0;
    _device->SkipForward = 0;
    _device->QuitWithoutLooping = 0;
    _device->ScaleModulators = 0;
    _device->delay = 0.0;
    _device->carry = 0.0;
    _device->mindelay = 1.0 / (double)_device->PCM_RATE;
    _device->maxdelay = 512.0 / (double)_device->PCM_RATE;
    _device->stored_samples = 0;
    _device->backup_samples_size = 0;
    MIDIplay *player = new MIDIplay;
    _device->adl_midiPlayer = player;
    player->config = _device;
    player->opl._parent = _device;
    player->opl.NumCards = _device->NumCards;
    player->opl.AdlBank = _device->AdlBank;
    player->opl.NumFourOps = _device->NumFourOps;
    player->opl.LogarithmicVolumes = (bool)_device->LogarithmicVolumes;
    player->opl.HighTremoloMode = (bool)_device->HighTremoloMode;
    player->opl.HighVibratoMode = (bool)_device->HighVibratoMode;
    player->opl.AdlPercussionMode = (bool)_device->AdlPercussionMode;
    player->opl.ScaleModulators = (bool)_device->ScaleModulators;
    player->ChooseDevice("none");
    adlRefreshNumCards(_device);
    return _device;
}

ADLMIDI_EXPORT int adl_setNumCards(ADL_MIDIPlayer *device, int numCards)
{
    if(device == NULL) return -2;

    device->NumCards = static_cast<unsigned int>(numCards);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.NumCards = device->NumCards;

    if(device->NumCards < 1 || device->NumCards > MaxCards)
    {
        std::stringstream s;
        s << "number of cards may only be 1.." << MaxCards << ".\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }

    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_setBank(ADL_MIDIPlayer *device, int bank)
{
    const uint32_t NumBanks = static_cast<uint32_t>(maxAdlBanks());
    int32_t bankno = bank;

    if(bankno < 0)
        bankno = 0;

    device->AdlBank = static_cast<uint32_t>(bankno);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.AdlBank = device->AdlBank;

    if(device->AdlBank >= NumBanks)
    {
        std::stringstream s;
        s << "bank number may only be 0.." << (NumBanks - 1) << ".\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }

    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_getBanksCount()
{
    return maxAdlBanks();
}

ADLMIDI_EXPORT const char *const *adl_getBankNames()
{
    return banknames;
}

ADLMIDI_EXPORT int adl_setNumFourOpsChn(ADL_MIDIPlayer *device, int ops4)
{
    device->NumFourOps = static_cast<unsigned int>(ops4);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.NumFourOps = device->NumFourOps;

    if(device->NumFourOps > 6 * device->NumCards)
    {
        std::stringstream s;
        s << "number of four-op channels may only be 0.." << (6 * (device->NumCards)) << " when " << device->NumCards << " OPL3 cards are used.\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }

    return adlRefreshNumCards(device);
}


ADLMIDI_EXPORT void adl_setPercMode(ADL_MIDIPlayer *device, int percmod)
{
    if(!device) return;

    device->AdlPercussionMode = static_cast<unsigned int>(percmod);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.AdlPercussionMode = (bool)device->AdlPercussionMode;
}

ADLMIDI_EXPORT void adl_setHVibrato(ADL_MIDIPlayer *device, int hvibro)
{
    if(!device) return;

    device->HighVibratoMode = static_cast<unsigned int>(hvibro);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.HighVibratoMode = (bool)device->HighVibratoMode;
}

ADLMIDI_EXPORT void adl_setHTremolo(ADL_MIDIPlayer *device, int htremo)
{
    if(!device) return;

    device->HighTremoloMode = static_cast<unsigned int>(htremo);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.HighTremoloMode = (bool)device->HighTremoloMode;
}

ADLMIDI_EXPORT void adl_setScaleModulators(ADL_MIDIPlayer *device, int smod)
{
    if(!device) return;

    device->ScaleModulators = static_cast<unsigned int>(smod);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.ScaleModulators = (bool)device->ScaleModulators;
}

ADLMIDI_EXPORT void adl_setLoopEnabled(ADL_MIDIPlayer *device, int loopEn)
{
    if(!device) return;

    device->QuitWithoutLooping = (loopEn == 0);
}

ADLMIDI_EXPORT void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol)
{
    if(!device) return;

    device->LogarithmicVolumes = static_cast<unsigned int>(logvol);
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.LogarithmicVolumes = (bool)device->LogarithmicVolumes;
}

ADLMIDI_EXPORT int adl_openFile(ADL_MIDIPlayer *device, char *filePath)
{
    ADLMIDI_ErrorString.clear();

    if(device && device->adl_midiPlayer)
    {
        device->stored_samples = 0;
        device->backup_samples_size = 0;

        if(!reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->LoadMIDI(filePath))
        {
            if(ADLMIDI_ErrorString.empty())
                ADLMIDI_ErrorString = "ADL MIDI: Can't load file";

            return -1;
        }
        else return 0;
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openData(ADL_MIDIPlayer *device, void *mem, long size)
{
    ADLMIDI_ErrorString.clear();

    if(device && device->adl_midiPlayer)
    {
        device->stored_samples = 0;
        device->backup_samples_size = 0;

        if(!reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->LoadMIDI(mem, static_cast<size_t>(size)))
        {
            if(ADLMIDI_ErrorString.empty())
                ADLMIDI_ErrorString = "ADL MIDI: Can't load data from memory";

            return -1;
        }
        else return 0;
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}


ADLMIDI_EXPORT const char *adl_errorString()
{
    return ADLMIDI_ErrorString.c_str();
}

ADLMIDI_EXPORT const char *adl_getMusicTitle(ADL_MIDIPlayer *device)
{
    if(!device) return "";

    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musTitle.c_str();
}

ADLMIDI_EXPORT void adl_close(ADL_MIDIPlayer *device)
{
    if(device->adl_midiPlayer)
        delete reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);

    device->adl_midiPlayer = NULL;
    free(device);
    device = NULL;
}

ADLMIDI_EXPORT void adl_reset(ADL_MIDIPlayer *device)
{
    if(!device) return;

    device->stored_samples = 0;
    device->backup_samples_size = 0;
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.Reset();
}

#ifdef ADLMIDI_USE_DOSBOX_OPL
inline static void SendStereoAudio(ADL_MIDIPlayer *device,
                                   int      &samples_requested,
                                   ssize_t  &in_size,
                                   int      *_in,
                                   ssize_t  out_pos,
                                   short    *_out)
{
    if(!in_size) return;

    device->stored_samples = 0;
    ssize_t out;
    ssize_t offset;
    ssize_t pos = static_cast<ssize_t>(out_pos);

    for(ssize_t p = 0; p < in_size; ++p)
    {
        for(ssize_t w = 0; w < 2; ++w)
        {
            out    = _in[p * 2 + w];
            offset = pos + p * 2 + w;

            if(offset < samples_requested)
                _out[offset] = static_cast<short>(out);
            else
            {
                device->backup_samples[device->backup_samples_size] = static_cast<short>(out);
                device->backup_samples_size++;
                device->stored_samples++;
            }
        }
    }
}
#else
inline static void SendStereoAudio(ADL_MIDIPlayer *device,
                                   int      &samples_requested,
                                   ssize_t  &in_size,
                                   short    *_in,
                                   ssize_t   out_pos,
                                   short    *_out)
{
    if(!in_size)
        return;

    device->stored_samples = 0;
    size_t offset       = static_cast<size_t>(out_pos);
    size_t inSamples    = static_cast<size_t>(in_size * 2);
    size_t maxSamples   = static_cast<size_t>(samples_requested) - offset;
    size_t toCopy       = std::min(maxSamples, inSamples);
    memcpy(_out + out_pos, _in, toCopy * sizeof(short));

    if(maxSamples < inSamples)
    {
        size_t appendSize = inSamples - maxSamples;
        memcpy(device->backup_samples + device->backup_samples_size,
               maxSamples + _in, appendSize * sizeof(short));
        device->backup_samples_size += appendSize;
        device->stored_samples += appendSize;
    }
}
#endif


ADLMIDI_EXPORT int adl_play(ADL_MIDIPlayer *device, int sampleCount, short *out)
{
    if(!device) return 0;

    sampleCount -= sampleCount % 2; //Avoid even sample requests

    if(sampleCount < 0) return 0;

    if(device->QuitFlag) return 0;

    ssize_t gotten_len = 0;
    ssize_t n_periodCountStereo = 512;
    ssize_t n_periodCountPhys = n_periodCountStereo * 2;
    int left = sampleCount;

    while(left > 0)
    {
        if(device->backup_samples_size > 0)
        {
            //Send reserved samples if exist
            ssize_t ate = 0;

            while((ate < device->backup_samples_size) && (ate < left))
            {
                out[ate] = device->backup_samples[ate];
                ate++;
            }

            left -= ate;
            gotten_len += ate;

            if(ate < device->backup_samples_size)
            {
                for(int j = 0;
                    j < ate;
                    j++)
                    device->backup_samples[(ate - 1) - j] = device->backup_samples[(device->backup_samples_size - 1) - j];
            }

            device->backup_samples_size -= ate;
        }
        else
        {
            const double eat_delay = device->delay < device->maxdelay ? device->delay : device->maxdelay;
            device->delay -= eat_delay;
            device->carry += device->PCM_RATE * eat_delay;
            n_periodCountStereo = static_cast<ssize_t>(device->carry);
            n_periodCountPhys = n_periodCountStereo * 2;
            device->carry -= n_periodCountStereo;

            if(device->SkipForward > 0)
                device->SkipForward -= 1;
            else
            {
#ifdef ADLMIDI_USE_DOSBOX_OPL
                std::vector<int> out_buf; //int buf[n_samples * 2];
#else
                std::vector<int16_t> out_buf;
#endif
                out_buf.resize(1024 /*n_samples * 2*/);
                ssize_t in_generatedStereo = (n_periodCountStereo > 512) ? 512 : n_periodCountStereo;
                ssize_t in_generatedPhys = in_generatedStereo * 2;
                //fill buffer with zeros
                size_t in_countStereoU = static_cast<size_t>(in_generatedStereo * 2);

                if(device->NumCards == 1)
                {
#ifdef ADLMIDI_USE_DOSBOX_OPL
                    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.cards[0].GenerateArr(out_buf.data(), &in_generatedStereo);
                    in_generatedPhys = in_generatedStereo * 2;
#else
                    OPL3_GenerateStream(&(reinterpret_cast<MIDIplay *>(device->adl_midiPlayer))->opl.cards[0], out_buf.data(), static_cast<Bit32u>(in_generatedStereo));
#endif
                    /* Process it */
                    SendStereoAudio(device, sampleCount, in_generatedStereo, out_buf.data(), gotten_len, out);
                }
                else if(n_periodCountStereo > 0)
                {
#ifdef ADLMIDI_USE_DOSBOX_OPL
                    std::vector<int32_t> in_mixBuffer;
                    in_mixBuffer.resize(1024); //n_samples * 2
                    ssize_t in_generatedStereo = n_periodCountStereo;
#endif
                    memset(out_buf.data(), 0, in_countStereoU * sizeof(short));

                    /* Generate data from every chip and mix result */
                    for(unsigned card = 0; card < device->NumCards; ++card)
                    {
#ifdef ADLMIDI_USE_DOSBOX_OPL
                        reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->opl.cards[card].GenerateArr(in_mixBuffer.data(), &in_generatedStereo);
                        in_generatedPhys = in_generatedStereo * 2;
                        size_t in_samplesCount = static_cast<size_t>(in_generatedPhys);

                        for(size_t a = 0; a < in_samplesCount; ++a)
                            out_buf[a] += in_mixBuffer[a];

#else
                        OPL3_GenerateStreamMix(&(reinterpret_cast<MIDIplay *>(device->adl_midiPlayer))->opl.cards[card], out_buf.data(), static_cast<Bit32u>(in_generatedStereo));
#endif
                    }

                    /* Process it */
                    SendStereoAudio(device, sampleCount, in_generatedStereo, out_buf.data(), gotten_len, out);
                }

                left -= in_generatedPhys;
                gotten_len += (in_generatedPhys) - device->stored_samples;
            }

            device->delay = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->Tick(eat_delay, device->mindelay);
        }
    }

    return static_cast<int>(gotten_len);
}


#ifdef ADLMIDI_buildAsApp

int main(int argc, char **argv)
{
    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
        );

        for(unsigned a = 0; a < sizeof(banknames) / sizeof(*banknames); ++a)
            std::printf("%10s%2u = %s\n",
                        a ? "" : "Banks:",
                        a,
                        banknames[a]);

        std::printf(
            "     Use banks 2-5 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            "     The fourth parameter can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
        );
        return 0;
    }

    //const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    const double AudioBufferLength = 0.08;
    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.1;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.
    SDL_AudioSpec spec;
    SDL_AudioSpec obtained;
    spec.freq     = 44100;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = spec.freq * AudioBufferLength;
    spec.callback = SDL_AudioCallbackX;

    // Set up SDL
    if(SDL_OpenAudio(&spec, &obtained) < 0)
    {
        std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        //return 1;
    }

    if(spec.samples != obtained.samples)
        std::fprintf(stderr, "Wanted (samples=%u,rate=%u,channels=%u); obtained (samples=%u,rate=%u,channels=%u)\n",
                     spec.samples,    spec.freq,    spec.channels,
                     obtained.samples, obtained.freq, obtained.channels);

    ADL_MIDIPlayer *myDevice;
    myDevice = adl_init(44100);

    if(myDevice == NULL)
    {
        std::fprintf(stderr, "Failed to init MIDI device!\n");
        return 1;
    }

    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            adl_setPercMode(myDevice, 1);
        else if(!std::strcmp("-v", argv[2]))
            adl_setHVibrato(myDevice, 1);
        else if(!std::strcmp("-t", argv[2]))
            adl_setHTremolo(myDevice, 1);
        else if(!std::strcmp("-nl", argv[2]))
            adl_setLoopEnabled(myDevice, 0);
        else if(!std::strcmp("-s", argv[2]))
            adl_setScaleModulators(myDevice, 1);
        else break;

        std::copy(argv + (had_option ? 4 : 3), argv + argc,
                  argv + 2);
        argc -= (had_option ? 2 : 1);
    }

    if(argc >= 3)
    {
        int bankno = std::atoi(argv[2]);

        if(adl_setBank(myDevice, bankno) != 0)
        {
            std::fprintf(stderr, "%s", adl_errorString());
            return 0;
        }
    }

    if(argc >= 4)
    {
        if(adl_setNumCards(myDevice, std::atoi(argv[3])) != 0)
        {
            std::fprintf(stderr, "%s\n", adl_errorString());
            return 0;
        }
    }

    if(argc >= 5)
    {
        if(adl_setNumFourOpsChn(myDevice, std::atoi(argv[4])) != 0)
        {
            std::fprintf(stderr, "%s\n", adl_errorString());
            return 0;
        }
    }

    if(adl_openFile(myDevice, argv[1]) != 0)
    {
        std::fprintf(stderr, "%s\n", adl_errorString());
        return 2;
    }

    SDL_PauseAudio(0);

    while(1)
    {
        int buff[4096];
        unsigned long gotten = adl_play(myDevice, 4096, buff);

        if(gotten <= 0) break;

        AudioBuffer_lock.Lock();
        size_t pos = AudioBuffer.size();
        AudioBuffer.resize(pos + gotten);

        for(unsigned long p = 0; p < gotten; ++p)
            AudioBuffer[pos + p] = buff[p];

        AudioBuffer_lock.Unlock();
        const SDL_AudioSpec &spec_ = obtained;

        while(AudioBuffer.size() > spec_.samples + (spec_.freq * 2) * OurHeadRoomLength)
            SDL_Delay(1);
    }

    adl_close(myDevice);
    SDL_CloseAudio();
    return 0;
}
#endif
