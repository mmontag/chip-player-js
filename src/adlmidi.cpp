/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <deque>
#include <algorithm>

#include "fraction.h"
#include "dbopl.h"

#include "adldata.hh"
#include "adlmidi.h"

#ifdef ADLMIDI_buildAsApp
#include <SDL2/SDL.h>
#endif

class MIDIplay;

static const unsigned MaxCards = 100;
static std::string ADLMIDI_ErrorString;

static const unsigned short Operators[23*2] =
    {0x000,0x003,0x001,0x004,0x002,0x005, // operators  0, 3,  1, 4,  2, 5
     0x008,0x00B,0x009,0x00C,0x00A,0x00D, // operators  6, 9,  7,10,  8,11
     0x010,0x013,0x011,0x014,0x012,0x015, // operators 12,15, 13,16, 14,17
     0x100,0x103,0x101,0x104,0x102,0x105, // operators 18,21, 19,22, 20,23
     0x108,0x10B,0x109,0x10C,0x10A,0x10D, // operators 24,27, 25,28, 26,29
     0x110,0x113,0x111,0x114,0x112,0x115, // operators 30,33, 31,34, 32,35
     0x010,0x013,   // operators 12,15
     0x014,0xFFF,   // operator 16
     0x012,0xFFF,   // operator 14
     0x015,0xFFF,   // operator 17
     0x011,0xFFF }; // operator 13

static const unsigned short Channels[23] =
    {0x000,0x001,0x002, 0x003,0x004,0x005, 0x006,0x007,0x008, // 0..8
     0x100,0x101,0x102, 0x103,0x104,0x105, 0x106,0x107,0x108, // 9..17 (secondary set)
     0x006,0x007,0x008,0xFFF,0xFFF }; // <- hw percussions, 0xFFF = no support for pitch/pan

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
    unsigned NumChannels;
    ADL_MIDIPlayer* _parent;

    std::vector<DBOPL::Handler> cards;
private:
    std::vector<unsigned short> ins; // index to adl[], cached, needed by Touch()
    std::vector<unsigned char> pit;  // value poked to B0, cached, needed by NoteOff)(
    std::vector<unsigned char> regBD;
public:
    unsigned int NumCards;
    unsigned int AdlBank;
    unsigned int NumFourOps;
    bool HighTremoloMode;
    bool HighVibratoMode;
    bool AdlPercussionMode;
    bool ScaleModulators;
    std::vector<char> four_op_category; // 1 = quad-master, 2 = quad-slave, 0 = regular
                                        // 3 = percussion BassDrum
                                        // 4 = percussion Snare
                                        // 5 = percussion Tom
                                        // 6 = percussion Crash cymbal
                                        // 7 = percussion Hihat
                                        // 8 = percussion slave

    void Poke(unsigned card, unsigned index, unsigned value)
    {
        cards[card].WriteReg(index, value);
    }
    void NoteOff(unsigned c)
    {
        unsigned card = c/23, cc = c%23;
        if(cc >= 18)
        {
            regBD[card] &= ~(0x10 >> (cc-18));
            Poke(card, 0xBD, regBD[card]);
            return;
        }
        Poke(card, 0xB0 + Channels[cc], pit[c] & 0xDF);
    }
    void NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
    {
        unsigned card = c/23, cc = c%23;
        unsigned x = 0x2000;
        if(hertz < 0 || hertz > 131071) // Avoid infinite loop
            return;
        while(hertz >= 1023.5) { hertz /= 2.0; x += 0x400; } // Calculate octave
        x += (int)(hertz + 0.5);
        unsigned chn = Channels[cc];
        if(cc >= 18)
        {
            regBD[card] |= (0x10 >> (cc-18));
            Poke(card, 0x0BD, regBD[card]);
            x &= ~0x2000;
            //x |= 0x800; // for test
        }
        if(chn != 0xFFF)
        {
            Poke(card, 0xA0 + chn, x & 0xFF);
            Poke(card, 0xB0 + chn, pit[c] = x >> 8);
        }
    }
    void Touch_Real(unsigned c, unsigned volume)
    {
        if(volume > 63) volume = 63;
        unsigned card = c/23, cc = c%23;
        unsigned i = ins[c], o1 = Operators[cc*2], o2 = Operators[cc*2+1];
        unsigned x = adl[i].modulator_40, y = adl[i].carrier_40;
        bool do_modulator;
        bool do_carrier;

        unsigned mode = 1; // 2-op AM
        if(four_op_category[c] == 0 || four_op_category[c] == 3)
        {
            mode = adl[i].feedconn & 1; // 2-op FM or 2-op AM
        }
        else if(four_op_category[c] == 1 || four_op_category[c] == 2)
        {
            unsigned i0, i1;
            if ( four_op_category[c] == 1 )
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
            mode += (adl[i0].feedconn & 1) + (adl[i1].feedconn & 1) * 2;
        }
        static const bool do_ops[10][2] =
          { { false, true },  /* 2 op FM */
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

        do_modulator = (ScaleModulators==1) ? true : do_ops[ mode ][ 0 ];
        do_carrier   = (ScaleModulators==1) ? true : do_ops[ mode ][ 1 ];

        Poke(card, 0x40+o1, do_modulator ? (x|63) - volume + volume*(x&63)/63 : x);
        if(o2 != 0xFFF)
        Poke(card, 0x40+o2, do_carrier   ? (y|63) - volume + volume*(y&63)/63 : y);
        // Correct formula (ST3, AdPlug):
        //   63-((63-(instrvol))/63)*chanvol
        // Reduces to (tested identical):
        //   63 - chanvol + chanvol*instrvol/63
        // Also (slower, floats):
        //   63 + chanvol * (instrvol / 63.0 - 1)
    }
    void Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        Touch_Real(c, volume>8725  ? std::log(volume)*11.541561 + (0.5 - 104.22845) : 0);
        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
    void Patch(unsigned c, unsigned i)
    {
        unsigned card = c/23, cc = c%23;
        static const unsigned char data[4] = {0x20,0x60,0x80,0xE0};
        ins[c] = i;
        unsigned o1 = Operators[cc*2+0], o2 = Operators[cc*2+1];
        unsigned x = adl[i].modulator_E862, y = adl[i].carrier_E862;
        for(unsigned a=0; a<4; ++a)
        {
            Poke(card, data[a]+o1, x&0xFF); x>>=8;
            if(o2 != 0xFFF)
            Poke(card, data[a]+o2, y&0xFF); y>>=8;
        }
    }
    void Pan(unsigned c, unsigned value)
    {
        unsigned card = c/23, cc = c%23;
        if(Channels[cc] != 0xFFF)
            Poke(card, 0xC0 + Channels[cc], adl[ins[c]].feedconn | value);
    }
    void Silence() // Silence all OPL channels.
    {
        for(unsigned c=0; c<NumChannels; ++c) { NoteOff(c); Touch_Real(c,0); }
    }
    void Reset()
    {
        cards.resize(NumCards);

        NumChannels = NumCards * 23;
        ins.resize(NumChannels,     189);
        pit.resize(NumChannels,       0);
        regBD.resize(NumCards);
        four_op_category.resize(NumChannels);
        for(unsigned p=0, a=0; a<NumCards; ++a)
        {
            for(unsigned b=0; b<18; ++b) four_op_category[p++] = 0;
            for(unsigned b=0; b< 5; ++b) four_op_category[p++] = 8;
        }

        static const short data[] =
        { 0x004,96, 0x004,128,        // Pulse timer
          0x105, 0, 0x105,1, 0x105,0, // Pulse OPL3 enable
          0x001,32, 0x105,1           // Enable wave, OPL3 extensions
        };
        unsigned fours = NumFourOps;
        for(unsigned card=0; card<NumCards; ++card)
        {
            cards[card].Init(_parent->PCM_RATE);

            for(unsigned a=0; a< 18; ++a) Poke(card, 0xB0+Channels[a], 0x00);
            for(unsigned a=0; a< sizeof(data)/sizeof(*data); a+=2)
                Poke(card, data[a], data[a+1]);
            Poke(card, 0x0BD, regBD[card] = (HighTremoloMode*0x80
                                           + HighVibratoMode*0x40
                                           + AdlPercussionMode*0x20) );
            unsigned fours_this_card = std::min(fours, 6u);
            Poke(card, 0x104, (1 << fours_this_card) - 1);
            //fprintf(stderr, "Card %u: %u four-ops.\n", card, fours_this_card);
            fours -= fours_this_card;
        }

        // Mark all channels that are reserved for four-operator function
        if(AdlPercussionMode==1)
            for(unsigned a=0; a<NumCards; ++a)
            {
                for(unsigned b=0; b<5; ++b) four_op_category[a*23 + 18 + b] = b+3;
                for(unsigned b=0; b<3; ++b) four_op_category[a*23 + 6  + b] = 8;
            }

        unsigned nextfour = 0;
        for(unsigned a=0; a<NumFourOps; ++a)
        {
            four_op_category[nextfour  ] = 1;
            four_op_category[nextfour+3] = 2;
            switch(a % 6)
            {
                case 0: case 1: nextfour += 1; break;
                case 2:         nextfour += 9-2; break;
                case 3: case 4: nextfour += 1; break;
                case 5:         nextfour += 23-9-2; break;
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

static const char MIDIsymbols[256+1] =
"PPPPPPhcckmvmxbd"  // Ins  0-15
"oooooahoGGGGGGGG"  // Ins 16-31
"BBBBBBBBVVVVVHHM"  // Ins 32-47
"SSSSOOOcTTTTTTTT"  // Ins 48-63
"XXXXTTTFFFFFFFFF"  // Ins 64-79
"LLLLLLLLpppppppp"  // Ins 80-95
"XXXXXXXXGGGGGTSS"  // Ins 96-111
"bbbbMMMcGXXXXXXX"  // Ins 112-127
"????????????????"  // Prc 0-15
"????????????????"  // Prc 16-31
"???DDshMhhhCCCbM"  // Prc 32-47
"CBDMMDDDMMDDDDDD"  // Prc 48-63
"DDDDDDDDDDDDDDDD"  // Prc 64-79
"DD??????????????"  // Prc 80-95
"????????????????"  // Prc 96-111
"????????????????"; // Prc 112-127

static const char PercussionMap[256] =
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
        double wait;
        struct TrackInfo
        {
            size_t ptr;
            long   delay;
            int    status;

            TrackInfo(): ptr(0), delay(0), status(0) { }
        };
        std::vector<TrackInfo> track;

        Position(): began(false), wait(0.0), track() { }
    } CurrentPosition, LoopBeginPosition;

    std::map<std::string, unsigned> devices;
    std::map<unsigned/*track*/, unsigned/*channel begin index*/> current_device;

    // Persistent settings for each MIDI channel
    struct MIDIchannel
    {
        unsigned short portamento;
        unsigned char bank_lsb, bank_msb;
        unsigned char patch;
        unsigned char volume, expression;
        unsigned char panning, vibrato, sustain;
        double bend, bendsense;
        double vibpos, vibspeed, vibdepth;
        long   vibdelay;
        unsigned char lastlrpn,lastmrpn; bool nrpn;
        struct NoteInfo
        {
            // Current pressure
            unsigned char  vol;
            // Tone selected on noteon:
            short tone;
            // Patch selected on noteon; index to banks[AdlBank][]
            unsigned char midiins;
            // Index to physical adlib data structure, adlins[]
            unsigned short insmeta;
            // List of adlib channels it is currently occupying.
            std::map<unsigned short/*adlchn*/,
                     unsigned short/*ins, inde to adl[]*/
                    > phys;
        };
        typedef std::map<unsigned char,NoteInfo> activenotemap_t;
        typedef activenotemap_t::iterator activenoteiterator;
        activenotemap_t activenotes;

        MIDIchannel()
            : portamento(0),
              bank_lsb(0), bank_msb(0), patch(0),
              volume(100),expression(100),
              panning(0x30), vibrato(0), sustain(0),
              bend(0.0), bendsense(2 / 8192.0),
              vibpos(0), vibspeed(2*3.141592653*5.0),
              vibdepth(0.5/127), vibdelay(0),
              lastlrpn(0),lastmrpn(0),nrpn(false),
              activenotes() { }
    };
    std::vector<MIDIchannel> Ch;

    // Additional information about AdLib channels
    struct AdlChannel
    {
        // For collisions
        struct Location
        {
            unsigned short MidCh;
            unsigned char  note;
            bool operator==(const Location&b) const
                { return MidCh==b.MidCh && note==b.note; }
            bool operator< (const Location&b) const
                { return MidCh<b.MidCh || (MidCh==b.MidCh&& note<b.note); }
        };
        struct LocationData
        {
            bool sustained;
            unsigned short ins;  // a copy of that in phys[]
            long kon_time_until_neglible;
            long vibdelay;
        };
        typedef std::map<Location, LocationData> users_t;
        users_t users;

        // If the channel is keyoff'd
        long koff_time_until_neglible;
        // For channel allocation:
        AdlChannel(): users(), koff_time_until_neglible(0) { }

        void AddAge(long ms)
        {
            if(users.empty())
                koff_time_until_neglible =
                    std::max(koff_time_until_neglible-ms, -0x1FFFFFFFl);
            else
            {
                koff_time_until_neglible = 0;
                for(users_t::iterator i = users.begin(); i != users.end(); ++i)
                {
                    i->second.kon_time_until_neglible =
                    std::max(i->second.kon_time_until_neglible-ms, -0x1FFFFFFFl);
                    i->second.vibdelay += ms;
                }
            }
        }
    };
    std::vector<AdlChannel> ch;

    std::vector< std::vector<unsigned char> > TrackData;
public:
    std::string musTitle;
    fraction<long> InvDeltaTicks, Tempo;
    bool loopStart, loopEnd;
    OPL3 opl;
public:
    static unsigned long ReadBEInt(const void* buffer, unsigned nbytes)
    {
        unsigned long result=0;
        const unsigned char* data = (const unsigned char*) buffer;
        for(unsigned n=0; n<nbytes; ++n)
            result = (result << 8) + data[n];
        return result;
    }
    unsigned long ReadVarLen(unsigned tk)
    {
        unsigned long result = 0;
        for(;;)
        {
            unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
            result = (result << 7) + (byte & 0x7F);
            if(!(byte & 0x80)) break;
        }
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
            SET=0,
            CUR=1,
            END=2
        };

        fileReader()
        {
            fp=NULL;
            mp=NULL;
            mp_size=0;
            mp_tell=0;
        }
        ~fileReader()
        {
            close();
        }

        void openFile(const char *path)
        {
            fp = std::fopen(path, "rb");
            _fileName=path;
            mp=NULL;
            mp_size=0;
            mp_tell=0;
        }

        void openData(void* mem, unsigned long lenght)
        {
            fp=NULL;
            mp=mem;
            mp_size=lenght;
            mp_tell=0;
        }

        void seek(long pos, int rel_to)
        {
            if(fp)
                std::fseek(fp, pos, rel_to);
            else
            {
                switch(rel_to)
                {
                    case SET: mp_tell=pos;
                    case END: mp_tell=mp_size-pos;
                    case CUR: mp_tell+=pos;
                }
            }
        }

        long read(void *buf, long num, long size)
        {
            if(fp)
                return std::fread(buf, num, size, fp);
            else
            {
                int pos=0;
                while( (pos < (size*num)) &&(mp_tell<mp_size))
                {
                    ((char*)buf)[pos]=((char*)mp)[mp_tell];
                    mp_tell++;
                    pos++;
                }
                return pos;
            }
        }

        char getc()
        {
            if(fp)
                return std::getc(fp);
            else
            {
                if(mp_tell>=mp_size) mp_tell=mp_size-1;
                char x=((char*)mp)[mp_tell];mp_tell++;
                return x;
            }
        }

        unsigned long tell()
        {
            if(fp)
                return std::ftell(fp);
            else
                return mp_tell;
        }

        void close()
        {
            if(fp) std::fclose(fp);
            fp=NULL;
            mp=NULL;
            mp_size=0;
            mp_tell=0;
        }

        bool isValid()
        {
            return (fp)||(mp);
        }

        bool eof()
        {
            return mp_tell>=mp_size;
        }
        std::string _fileName;
        std::FILE*    fp;
        void*         mp;
        unsigned long mp_size;
        unsigned long mp_tell;
    };

    bool LoadMIDI(const std::string& filename)
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
        int fsize;
        qqq(fsize);
        //std::FILE* fr = std::fopen(filename.c_str(), "rb");
        if(!fr.isValid()) { return false; }
        char HeaderBuf[4+4+2+2+2]="";
    riffskip:;
        fsize=fr.read(HeaderBuf, 1, 4+4+2+2+2);
        if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
            { fr.seek(6, SEEK_CUR); goto riffskip; }
        size_t DeltaTicks=192, TrackCount=1;

        bool is_GMF = false, is_MUS = false, is_IMF = false;
        //std::vector<unsigned char> MUS_instrumentList;

        if(std::memcmp(HeaderBuf, "GMF\1", 4) == 0)
        {
            // GMD/MUS files (ScummVM)
            fr.seek(7-(4+4+2+2+2), SEEK_CUR);
            is_GMF = true;
        }
        else if(std::memcmp(HeaderBuf, "MUS\1x1A", 4) == 0)
        {
            // MUS/DMX files (Doom)
            fr.seek(8-(4+4+2+2+2), SEEK_CUR);
            is_MUS = true;
            unsigned start = fr.getc(); start += (fr.getc() << 8);
            fr.seek(-8+start, SEEK_CUR);
        }
        else
        {
            // Try parsing as an IMF file
           {
            unsigned end = (unsigned char)HeaderBuf[0] + 256*(unsigned char)HeaderBuf[1];
            if(!end || (end & 3)) goto not_imf;

            long backup_pos = fr.tell();
            unsigned sum1 = 0, sum2 = 0;
            fr.seek(2, SEEK_SET);
            for(unsigned n=0; n<42; ++n)
            {
                unsigned value1 = fr.getc(); value1 += fr.getc() << 8; sum1 += value1;
                unsigned value2 = fr.getc(); value2 += fr.getc() << 8; sum2 += value2;
            }
            fr.seek(backup_pos, SEEK_SET);
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
                { InvFmt:
                    fr.close();
                    ADLMIDI_ErrorString=fr._fileName+": Invalid format\n";
                    return false;
                }
                /*size_t  Fmt =*/ ReadBEInt(HeaderBuf+8,  2);
                TrackCount = ReadBEInt(HeaderBuf+10, 2);
                DeltaTicks = ReadBEInt(HeaderBuf+12, 2);
            }
        }
        TrackData.resize(TrackCount);
        CurrentPosition.track.resize(TrackCount);
        InvDeltaTicks = fraction<long>(1, 1000000l * DeltaTicks);
        //Tempo       = 1000000l * InvDeltaTicks;
        Tempo         = fraction<long>(1,            DeltaTicks);

        static const unsigned char EndTag[4] = {0xFF,0x2F,0x00,0x00};

        for(size_t tk = 0; tk < TrackCount; ++tk)
        {
            // Read track header
            size_t TrackLength;
            if(is_IMF)
            {
                //std::fprintf(stderr, "Reading IMF file...\n");
                long end = (unsigned char)HeaderBuf[0] + 256*(unsigned char)HeaderBuf[1];

                unsigned IMF_tempo = 1428;
                static const unsigned char imf_tempo[] = {0xFF,0x51,0x4,
                    (unsigned char)(IMF_tempo>>24),
                    (unsigned char)(IMF_tempo>>16),
                    (unsigned char)(IMF_tempo>>8),
                    (unsigned char)(IMF_tempo)};
                TrackData[tk].insert(TrackData[tk].end(), imf_tempo, imf_tempo + sizeof(imf_tempo));
                TrackData[tk].push_back(0x00);

                fr.seek(2, SEEK_SET);
                while(fr.tell() < (unsigned)end)
                {
                    unsigned char special_event_buf[5];
                    special_event_buf[0] = 0xFF;
                    special_event_buf[1] = 0xE3;
                    special_event_buf[2] = 0x02;
                    special_event_buf[3] = fr.getc(); // port index
                    special_event_buf[4] = fr.getc(); // port value
                    unsigned delay = fr.getc(); delay += 256 * fr.getc();

                    //if(special_event_buf[3] <= 8) continue;

                    //fprintf(stderr, "Put %02X <- %02X, plus %04X delay\n", special_event_buf[3],special_event_buf[4], delay);

                    TrackData[tk].insert(TrackData[tk].end(), special_event_buf, special_event_buf+5);
                    //if(delay>>21) TrackData[tk].push_back( 0x80 | ((delay>>21) & 0x7F ) );
                    if(delay>>14) TrackData[tk].push_back( 0x80 | ((delay>>14) & 0x7F ) );
                    if(delay>> 7) TrackData[tk].push_back( 0x80 | ((delay>> 7) & 0x7F ) );
                    TrackData[tk].push_back( ((delay>>0) & 0x7F ) );
                }
                TrackData[tk].insert(TrackData[tk].end(), EndTag+0, EndTag+4);
                CurrentPosition.track[tk].delay = 0;
                CurrentPosition.began = true;
                //std::fprintf(stderr, "Done reading IMF file\n");
            }
            else
            {
                if(is_GMF)
                {
                    long pos = fr.tell();
                    fr.seek(0, SEEK_END);
                    TrackLength = fr.tell() - pos;
                    fr.seek(pos, SEEK_SET);
                }
                else if(is_MUS)
                {
                    long pos = fr.tell();
                    fr.seek(4, SEEK_SET);
                    TrackLength = fr.getc(); TrackLength += (fr.getc() << 8);
                    fr.seek(pos, SEEK_SET);
                }
                else
                {
                    fsize=fr.read(HeaderBuf, 1, 8);
                    if(std::memcmp(HeaderBuf, "MTrk", 4) != 0) goto InvFmt;
                    TrackLength = ReadBEInt(HeaderBuf+4, 4);
                }
                // Read track data
                TrackData[tk].resize(TrackLength);
                fsize=fr.read(&TrackData[tk][0], 1, TrackLength);
                if(is_GMF || is_MUS)
                {
                    TrackData[tk].insert(TrackData[tk].end(), EndTag+0, EndTag+4);
                }
                // Read next event time
                CurrentPosition.track[tk].delay = ReadVarLen(tk);
            }
        }
        loopStart = true;

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
        while(CurrentPosition.wait <= granularity * 0.5)
        {
            //std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
            ProcessEvents();
        }

        for(unsigned c = 0; c < opl.NumChannels; ++c)
            ch[c].AddAge(s * 1000);

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
           Upd_Off    = 0x20 };

    void NoteUpdate
        (unsigned MidCh,
         MIDIchannel::activenoteiterator i,
         unsigned props_mask,
         int select_adlchn = -1)
    {
        MIDIchannel::NoteInfo& info = i->second;
        const int tone    = info.tone;
        const int vol     = info.vol;
        //const int midiins = info.midiins;
        const int insmeta = info.insmeta;

        AdlChannel::Location my_loc;
        my_loc.MidCh = MidCh;
        my_loc.note  = i->first;

        for(std::map<unsigned short,unsigned short>::iterator
            jnext = info.phys.begin();
            jnext != info.phys.end();
           )
        {
            std::map<unsigned short,unsigned short>::iterator j(jnext++);
            int c   = j->first;
            int ins = j->second;
            if(select_adlchn >= 0 && c != select_adlchn) continue;

            if(props_mask & Upd_Patch)
            {
                opl.Patch(c, ins);
                AdlChannel::LocationData& d = ch[c].users[my_loc];
                d.sustained = false; // inserts if necessary
                d.vibdelay  = 0;
                d.kon_time_until_neglible = adlins[insmeta].ms_sound_kon;
                d.ins       = ins;
            }
        }
        for(std::map<unsigned short,unsigned short>::iterator
            jnext = info.phys.begin();
            jnext != info.phys.end();
           )
        {
            std::map<unsigned short,unsigned short>::iterator j(jnext++);
            int c   = j->first;
            int ins = j->second;
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
                            adlins[insmeta].ms_sound_koff;
                    }
                }
                else
                {
                    // Sustain: Forget about the note, but don't key it off.
                    //          Also will avoid overwriting it very soon.
                    AdlChannel::LocationData& d = ch[c].users[my_loc];
                    d.sustained = true; // note: not erased!
                    //UI.IllustrateNote(c, tone, midiins, -1, 0.0);
                }
                info.phys.erase(j);
                continue;
            }
            if(props_mask & Upd_Pan)
            {
                opl.Pan(c, Ch[MidCh].panning);
            }
            if(props_mask & Upd_Volume)
            {
                int volume = vol * Ch[MidCh].volume * Ch[MidCh].expression;
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
                AdlChannel::LocationData& d = ch[c].users[my_loc];
                // Don't bend a sustained note
                if(!d.sustained)
                {
                    double bend = Ch[MidCh].bend + adl[ins].finetune;
                    double phase = 0.0;

                    if((adlins[insmeta].flags & adlinsdata::Flag_Pseudo4op) && ins == adlins[insmeta].adlno2)
                    {
                        phase = 0.125; // Detune the note slightly (this is what Doom does)
                    }

                    if(Ch[MidCh].vibrato && d.vibdelay >= Ch[MidCh].vibdelay)
                        bend += Ch[MidCh].vibrato * Ch[MidCh].vibdepth * std::sin(Ch[MidCh].vibpos);
                    opl.NoteOn(c, 172.00093 * std::exp(0.057762265 * (tone + bend + phase)));
                    //UI.IllustrateNote(c, tone, midiins, vol, Ch[MidCh].bend);
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
        const Position RowBeginPosition ( CurrentPosition );
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
        for(size_t tk=0; tk<TrackCount; ++tk)
            if(CurrentPosition.track[tk].status >= 0
            && (shortest == -1
               || CurrentPosition.track[tk].delay < shortest))
            {
                shortest = CurrentPosition.track[tk].delay;
            }
        //if(shortest > 0) UI.PrintLn("shortest: %ld", shortest);

        // Schedule the next playevent to be processed after that delay
        for(size_t tk=0; tk<TrackCount; ++tk)
            CurrentPosition.track[tk].delay -= shortest;

        fraction<long> t = shortest * Tempo;
        if(CurrentPosition.began) CurrentPosition.wait += t.valuel();

        //if(shortest > 0) UI.PrintLn("Delay %ld (%g)", shortest, (double)t.valuel());

        /*
        if(CurrentPosition.track[0].ptr > 8119) loopEnd = true;
        // ^HACK: CHRONO TRIGGER LOOP
        */

        if(loopStart)
        {
            LoopBeginPosition = RowBeginPosition;
            loopStart = false;
        }
        if(shortest < 0 || loopEnd)
        {
            // Loop if song end reached
            loopEnd         = false;
            CurrentPosition = LoopBeginPosition;
            shortest        = 0;
            if(opl._parent->QuitWithoutLooping==1)
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
            unsigned length = ReadVarLen(tk);
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
            std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
            CurrentPosition.track[tk].ptr += length;
            if(evtype == 0x2F) { CurrentPosition.track[tk].status = -1; return; }
            if(evtype == 0x51) { Tempo = InvDeltaTicks * fraction<long>( (long) ReadBEInt(data.data(), data.size())); return; }
            if(evtype == 6 && data == "loopStart") loopStart = true;
            if(evtype == 6 && data == "loopEnd"  ) loopEnd   = true;
            if(evtype == 9) current_device[tk] = ChooseDevice(data);
//            if(evtype >= 1 && evtype <= 6)
//                UI.PrintLn("Meta %d: %s", evtype, data.c_str());

            if(evtype == 0xE3) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
            {
                unsigned char i = data[0], v = data[1];
                if( (i&0xF0) == 0xC0 ) v |= 0x30;
                //fprintf(stderr, "OPL poke %02X, %02X\n", i,v);
                opl.Poke(0, i,v);
            }
            return;
        }
        // Any normal event (80..EF)
        if(byte < 0x80)
          { byte = CurrentPosition.track[tk].status | 0x80;
            CurrentPosition.track[tk].ptr--; }
        if(byte == 0xF3) { CurrentPosition.track[tk].ptr += 1; return; }
        if(byte == 0xF2) { CurrentPosition.track[tk].ptr += 2; return; }
        /*UI.PrintLn("@%X Track %u: %02X %02X",
            CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte,
            TrackData[tk][CurrentPosition.track[tk].ptr]);*/
        unsigned MidCh = byte & 0x0F, EvType = byte >> 4;
        MidCh += current_device[tk];

        CurrentPosition.track[tk].status = byte;
        switch(EvType)
        {
            case 0x8: // Note off
            case 0x9: // Note on
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                //if(MidCh != 9) note -= 12; // HACK
                NoteOff(MidCh, note);
                // On Note on, Keyoff the note first, just in case keyoff
                // was omitted; this fixes Dance of sugar-plum fairy
                // by Microsoft. Now that we've done a Keyoff,
                // check if we still need to do a Keyon.
                // vol=0 and event 8x are both Keyoff-only.
                if(vol == 0 || EvType == 0x8) break;

                unsigned midiins = Ch[MidCh].patch;
                if(MidCh%16 == 9) midiins = 128 + note; // Percussion instrument

                /*
                if(MidCh%16 == 9 || (midiins != 32 && midiins != 46 && midiins != 48 && midiins != 50))
                    break; // HACK
                if(midiins == 46) vol = (vol*7)/10;          // HACK
                if(midiins == 48 || midiins == 50) vol /= 4; // HACK
                */
                //if(midiins == 56) vol = vol*6/10; // HACK

                static std::set<unsigned> bank_warnings;
                if(Ch[MidCh].bank_msb)
                {
                    unsigned bankid = midiins + 256*Ch[MidCh].bank_msb;
                    std::set<unsigned>::iterator
                        i = bank_warnings.lower_bound(bankid);
                    if(i == bank_warnings.end() || *i != bankid)
                    {
                        ADLMIDI_ErrorString.clear();
                        std::stringstream s;
                        s<<"[" << MidCh <<"]Bank " << Ch[MidCh].bank_msb <<
                           " undefined, patch="<< ((midiins&128) ? 'P':'M') << (midiins&127);
                        ADLMIDI_ErrorString=s.str();
                        bank_warnings.insert(i, bankid);
                    }
                }
                if(Ch[MidCh].bank_lsb)
                {
                    unsigned bankid = Ch[MidCh].bank_lsb*65536;
                    std::set<unsigned>::iterator
                        i = bank_warnings.lower_bound(bankid);
                    if(i == bank_warnings.end() || *i != bankid)
                    {
                        ADLMIDI_ErrorString.clear();
                        std::stringstream s;
                        s<<"["<<MidCh<< "]Bank lsb " << Ch[MidCh].bank_lsb <<" undefined";
                        ADLMIDI_ErrorString=s.str();
                        bank_warnings.insert(i, bankid);
                    }
                }

                int meta = banks[opl.AdlBank][midiins];
                int tone = note;
                if(adlins[meta].tone)
                {
                    if(adlins[meta].tone < 20)
                        tone += adlins[meta].tone;
                    else if(adlins[meta].tone < 128)
                        tone = adlins[meta].tone;
                    else
                        tone -= adlins[meta].tone-128;
                }
                int i[2] = { adlins[meta].adlno1, adlins[meta].adlno2 };
                bool pseudo_4op = adlins[meta].flags & adlinsdata::Flag_Pseudo4op;

                if((opl.AdlPercussionMode==1) && PercussionMap[midiins & 0xFF]) i[1] = i[0];

                static std::set<unsigned char> missing_warnings;
                if(!missing_warnings.count(midiins) && (adlins[meta].flags & adlinsdata::Flag_NoSound))
                {
                    //UI.PrintLn("[%i]Playing missing instrument %i", MidCh, midiins);
                    missing_warnings.insert(midiins);
                }

                // Allocate AdLib channel (the physical sound channel for the note)
                int adlchannel[2] = { -1, -1 };
                for(unsigned ccount = 0; ccount < 2; ++ccount)
                {
                    if(ccount == 1)
                    {
                        if(i[0] == i[1]) break; // No secondary channel
                        if(adlchannel[0] == -1) break; // No secondary if primary failed
                    }

                    int c = -1;
                    long bs = -0x7FFFFFFFl;
                    for(int a = 0; a < (int)opl.NumChannels; ++a)
                    {
                        if(ccount == 1 && a == adlchannel[0]) continue;
                        // ^ Don't use the same channel for primary&secondary

                        if(i[0] == i[1] || pseudo_4op)
                        {
                            // Only use regular channels
                            int expected_mode = 0;
                            if(opl.AdlPercussionMode==1)
                                expected_mode = PercussionMap[midiins & 0xFF];
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
                                if(a != adlchannel[0] + 3)
                                    continue;
                            }
                        }

                        long s = CalculateAdlChannelGoodness(a, i[ccount], MidCh);
                        if(s > bs) { bs=s; c = a; } // Best candidate wins
                    }

                    if(c < 0)
                    {
                        //UI.PrintLn("ignored unplaceable note");
                        continue; // Could not play this note. Ignore it.
                    }
                    PrepareAdlChannelForNewNote(c, i[ccount]);
                    adlchannel[ccount] = c;
                }
                if(adlchannel[0] < 0 && adlchannel[1] < 0)
                {
                    // The note could not be played, at all.
                    break;
                }
                //UI.PrintLn("i1=%d:%d, i2=%d:%d", i[0],adlchannel[0], i[1],adlchannel[1]);

                // Allocate active note for MIDI channel
                std::pair<MIDIchannel::activenoteiterator,bool>
                    ir = Ch[MidCh].activenotes.insert(
                        std::make_pair(note, MIDIchannel::NoteInfo()));
                ir.first->second.vol     = vol;
                ir.first->second.tone    = tone;
                ir.first->second.midiins = midiins;
                ir.first->second.insmeta = meta;
                for(unsigned ccount=0; ccount<2; ++ccount)
                {
                    int c = adlchannel[ccount];
                    if(c < 0) continue;
                    ir.first->second.phys[ adlchannel[ccount] ] = i[ccount];
                }
                CurrentPosition.began  = true;
                NoteUpdate(MidCh, ir.first, Upd_All | Upd_Patch);
                break;
            }
            case 0xA: // Note touch
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
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
                int ctrlno = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  value = TrackData[tk][CurrentPosition.track[tk].ptr++];
                switch(ctrlno)
                {
                    case 1: // Adjust vibrato
                        //UI.PrintLn("%u:vibrato %d", MidCh,value);
                        Ch[MidCh].vibrato = value; break;
                    case 0: // Set bank msb (GM bank)
                        Ch[MidCh].bank_msb = value;
                        break;
                    case 32: // Set bank lsb (XG bank)
                        Ch[MidCh].bank_lsb = value;
                        break;
                    case 5: // Set portamento msb
                        Ch[MidCh].portamento = (Ch[MidCh].portamento & 0x7F) | (value<<7);
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
                        //NoteUpdate_All(MidCh, Upd_Volume);
                        break;
                    case 64: // Enable/disable sustain
                        Ch[MidCh].sustain = value;
                        if(!value) KillSustainingNotes( MidCh );
                        break;
                    case 11: // Change expression (another volume factor)
                        Ch[MidCh].expression = value;
                        NoteUpdate_All(MidCh, Upd_Volume);
                        break;
                    case 10: // Change panning
                        Ch[MidCh].panning = 0x00;
                        if(value  < 64+32) Ch[MidCh].panning |= 0x10;
                        if(value >= 64-32) Ch[MidCh].panning |= 0x20;
                        NoteUpdate_All(MidCh, Upd_Pan);
                        break;
                    case 121: // Reset all controllers
                        Ch[MidCh].bend       = 0;
                        Ch[MidCh].volume     = 100;
                        Ch[MidCh].expression = 100;
                        Ch[MidCh].sustain    = 0;
                        Ch[MidCh].vibrato    = 0;
                        Ch[MidCh].vibspeed   = 2*3.141592653*5.0;
                        Ch[MidCh].vibdepth   = 0.5/127;
                        Ch[MidCh].vibdelay   = 0;
                        Ch[MidCh].panning    = 0x30;
                        Ch[MidCh].portamento = 0;
                        //UpdatePortamento(MidCh);
                        NoteUpdate_All(MidCh, Upd_Pan+Upd_Volume+Upd_Pitch);
                        // Kill all sustained notes
                        KillSustainingNotes(MidCh);
                        break;
                    case 123: // All notes off
                        NoteUpdate_All(MidCh, Upd_Off);
                        break;
                    case 91: break; // Reverb effect depth. We don't do per-channel reverb.
                    case 92: break; // Tremolo effect depth. We don't do...
                    case 93: break; // Chorus effect depth. We don't do.
                    case 94: break; // Celeste effect depth. We don't do.
                    case 95: break; // Phaser effect depth. We don't do.
                    case 98: Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=true; break;
                    case 99: Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=true; break;
                    case 100:Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=false; break;
                    case 101:Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=false; break;
                    case 113: break; // Related to pitch-bender, used by missimp.mid in Duke3D
                    case  6: SetRPN(MidCh, value, true); break;
                    case 38: SetRPN(MidCh, value, false); break;
                    default: break;
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
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
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
                Ch[MidCh].bend = (a + b*128 - 8192) * Ch[MidCh].bendsense;
                NoteUpdate_All(MidCh, Upd_Pitch);
                break;
            }
        }
    }

    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    long CalculateAdlChannelGoodness
        (unsigned c, unsigned ins, unsigned /*MidCh*/) const
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
    void PrepareAdlChannelForNewNote(int c, int ins)
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
                ( Ch[j->first.MidCh].activenotes.find( j->first.note ) );

                // Check if we can do arpeggio.
                if((j->second.vibdelay < 70
                 || j->second.kon_time_until_neglible > 20000)
                && j->second.ins == ins)
                {
                    // Do arpeggio together with this note.
                    //doing_arpeggio = true;
                    continue;
                }

                KillOrEvacuate(c,j,i);
                // ^ will also erase j from ch[c].users.
            }
        }

        // Kill all sustained notes on this channel
        // Don't keep them for arpeggio, because arpeggio requires
        // an intact "activenotes" record. This is a design flaw.
        KillSustainingNotes(-1, c);

        // Keyoff the channel so that it can be retriggered,
        // unless the new note will be introduced as just an arpeggio.
        if(ch[c].users.empty())
            opl.NoteOff(c);
    }

    void KillOrEvacuate(
        unsigned from_channel,
        AdlChannel::users_t::iterator j,
        MIDIchannel::activenoteiterator i)
    {
        // Before killing the note, check if it can be
        // evacuated to another channel as an arpeggio
        // instrument. This helps if e.g. all channels
        // are full of strings and we want to do percussion.
        // FIXME: This does not care about four-op entanglements.
        for(unsigned c = 0; c < opl.NumChannels; ++c)
        {
            if(c == from_channel) continue;
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

                i->second.phys.erase(from_channel);
                i->second.phys[c] = j->second.ins;
                ch[c].users.insert( *j );
                ch[from_channel].users.erase( j );
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
                   from_channel);
    }

    void KillSustainingNotes(int MidCh = -1, int this_adlchn = -1)
    {
        unsigned first=0, last=opl.NumChannels;
        if(this_adlchn >= 0) { first=this_adlchn; last=first+1; }
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
        unsigned addr = Ch[MidCh].lastmrpn*0x100 + Ch[MidCh].lastlrpn;
        switch(addr + nrpn*0x10000 + MSB*0x20000)
        {
            case 0x0000 + 0*0x10000 + 1*0x20000: // Pitch-bender sensitivity
                Ch[MidCh].bendsense = value/8192.0;
                break;
            case 0x0108 + 1*0x10000 + 1*0x20000: // Vibrato speed
                if(value == 64)
                    Ch[MidCh].vibspeed = 1.0;
                else if(value < 100)
                    Ch[MidCh].vibspeed = 1.0/(1.6e-2*(value?value:1));
                else
                    Ch[MidCh].vibspeed = 1.0/(0.051153846*value-3.4965385);
                Ch[MidCh].vibspeed *= 2*3.141592653*5.0;
                break;
            case 0x0109 + 1*0x10000 + 1*0x20000: // Vibrato depth
                Ch[MidCh].vibdepth = ((value-64)*0.15)*0.01;
                break;
            case 0x010A + 1*0x10000 + 1*0x20000: // Vibrato delay in millisecons
                Ch[MidCh].vibdelay =
                    value ? long(0.2092 * std::exp(0.0795 * value)) : 0.0;
                break;

            default:/* UI.PrintLn("%s %04X <- %d (%cSB) (ch %u)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB], MidCh);*/ break;
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

    void NoteUpdate_All(unsigned MidCh, unsigned props_mask)
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

    void NoteOff(unsigned MidCh, int note)
    {
        MIDIchannel::activenoteiterator
            i = Ch[MidCh].activenotes.find(note);
        if(i != Ch[MidCh].activenotes.end())
        {
            NoteUpdate(MidCh, i, Upd_Off);
        }
    }

    void UpdateVibrato(double amount)
    {
        for(unsigned a=0, b=Ch.size(); a<b; ++a)
            if(Ch[a].vibrato && !Ch[a].activenotes.empty())
            {
                NoteUpdate_All(a, Upd_Pitch);
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
        static unsigned cache=0;
        amount=amount; // Ignore amount. Assume we get a constant rate.
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

        for(unsigned c = 0; c < opl.NumChannels; ++c)
        {
        retry_arpeggio:;
            size_t n_users = ch[c].users.size();
            /*if(true)
            {
                UI.GotoXY(64,c+1); UI.Color(2);
                std::fprintf(stderr, "%7ld/%7ld,%3u\r",
                    ch[c].keyoff,
                    (unsigned) n_users);
                UI.x = 0;
            }*/
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
                            Ch[ i->first.MidCh ].activenotes.find( i->first.note ),
                            Upd_Off,
                            c);
                        goto retry_arpeggio;
                    }
                    NoteUpdate(
                        i->first.MidCh,
                        Ch[ i->first.MidCh ].activenotes.find( i->first.note ),
                        Upd_Pitch | Upd_Volume | Upd_Pan,
                        c);
                }
            }
        }
    }

public:
    unsigned ChooseDevice(const std::string& name)
    {
        std::map<std::string, unsigned>::iterator i = devices.find(name);
        if(i != devices.end()) return i->second;
        size_t n = devices.size() * 16;
        devices.insert( std::make_pair(name, n) );
        Ch.resize(n+16);
        return n;
    }
};

#ifdef ADLMIDI_buildAsApp
static std::deque<short> AudioBuffer;
static MutexType AudioBuffer_lock;

static void SDL_AudioCallbackX(void*, Uint8* stream, int len)
{
    SDL_LockAudio();
    short* target = (short*) stream;
    AudioBuffer_lock.Lock();
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len/2; // number of shorts
    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();
    for(unsigned a=0; a<ate; ++a)
    {
        target[a] = AudioBuffer[a];
    }
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}
#endif

struct FourChars
{
    char ret[4];

    FourChars(const char* s)
    {
        for(unsigned c=0; c<4; ++c) ret[c] = s[c];
    }
    FourChars(unsigned w) // Little-endian
    {
        for(unsigned c=0; c<4; ++c) ret[c] = (w >> (c*8)) & 0xFF;
    }
};

int adlRefreshNumCards(ADL_MIDIPlayer* device)
{
    unsigned n_fourop[2] = {0,0}, n_total[2] = {0,0};
    for(unsigned a=0; a<256; ++a)
    {
        unsigned insno = banks[device->AdlBank][a];
        if(insno == 198) continue;
        ++n_total[a/128];
        if(adlins[insno].adlno1 != adlins[insno].adlno2)
            ++n_fourop[a/128];
    }

    device->NumFourOps =
        (n_fourop[0] >= n_total[0]*7/8) ? device->NumCards * 6
      : (n_fourop[0] < n_total[0]*1/8) ? 0
      : (device->NumCards==1 ? 1 : device->NumCards*4);

    ((MIDIplay*)device->adl_midiPlayer)->opl.NumFourOps=device->NumFourOps;

    if(n_fourop[0] >= n_total[0]*15/16 && device->NumFourOps == 0)
    {
        ADLMIDI_ErrorString = "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
            "       The results (silence + much cpu load) would be probably\n"
            "       not what you want, therefore ignoring the request.\n";
        return -1;
    }
    return 0;
}

struct Mixx
{
    static int requested_len;  //size of target buffer
    static int stored_samples; //num of collected samples
    static int backup_samples[1024]; //Backup sample storage.
    static int backup_samples_size; //Backup sample storage.
    //If requested number of samples less than 512 bytes, backup will be stored

    static int *_len;
    static int *_out;
    static void SendStereoAudio(unsigned long count, int* samples)
    {
        if(!count) return;
        stored_samples=0;
        size_t pos=(*_len);
        for(unsigned long p = 0; p < count; ++p)
            for(unsigned w=0; w<2; ++w)
            {
                int out=samples[p*2+w];
                int offset=pos+p*2+w;
                if(offset<requested_len)
                    _out[offset] = out;
                else
                {
                    backup_samples[backup_samples_size]=out;
                    backup_samples_size++; stored_samples++;
                }
            }
    }

};

int  Mixx::requested_len=0;
int  Mixx::stored_samples=0;
int  Mixx::backup_samples[1024];
int  Mixx::backup_samples_size=0;
int *Mixx::_len=NULL;
int *Mixx::_out=NULL;



/*---------------------------EXPORTS---------------------------*/

ADLMIDI_EXPORT struct ADL_MIDIPlayer* adl_init(long sample_rate)
{
    ADL_MIDIPlayer* _device;
    _device = (ADL_MIDIPlayer*)malloc(sizeof(ADL_MIDIPlayer));
    _device->PCM_RATE = sample_rate;
    _device->AdlBank    = 0;
    _device->NumFourOps = 7;
    _device->NumCards   = 2;
    _device->HighTremoloMode   = 0;
    _device->HighVibratoMode   = 0;
    _device->AdlPercussionMode = 0;
    _device->QuitFlag = 0;
    _device->SkipForward = 0;
    _device->QuitWithoutLooping = 0;
    _device->ScaleModulators = 0;
    _device->delay=0.0;
    _device->carry=0.0;
    _device->mindelay = 1.0 / (double)_device->PCM_RATE;
    _device->maxdelay = 512.0 / (double)_device->PCM_RATE;
    _device->adl_midiPlayer = (void*)new MIDIplay;
    ((MIDIplay*)_device->adl_midiPlayer)->opl._parent=_device;
    ((MIDIplay*)_device->adl_midiPlayer)->opl.NumCards=_device->NumCards;
    ((MIDIplay*)_device->adl_midiPlayer)->opl.AdlBank=_device->AdlBank;
    ((MIDIplay*)_device->adl_midiPlayer)->opl.NumFourOps=_device->NumFourOps;
    ((MIDIplay*)_device->adl_midiPlayer)->opl.HighTremoloMode=(bool)_device->HighTremoloMode;
    ((MIDIplay*)_device->adl_midiPlayer)->opl.HighVibratoMode=(bool)_device->HighVibratoMode;
    ((MIDIplay*)_device->adl_midiPlayer)->opl.AdlPercussionMode=(bool)_device->AdlPercussionMode;
    ((MIDIplay*)_device->adl_midiPlayer)->opl.ScaleModulators=(bool)_device->ScaleModulators;
    ((MIDIplay*)(_device->adl_midiPlayer))->ChooseDevice("");
    adlRefreshNumCards(_device);
    return _device;
}

ADLMIDI_EXPORT int adl_setNumCards(ADL_MIDIPlayer *device, int numCards)
{
    if(device==NULL) return -2;
    device->NumCards = numCards;
    ((MIDIplay*)device->adl_midiPlayer)->opl.NumCards=device->NumCards;
    if(device->NumCards < 1 || device->NumCards > MaxCards)
    {
        std::stringstream s;
        s<<"number of cards may only be 1.."<< MaxCards<<".\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }
    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_setBank(ADL_MIDIPlayer *device, int bank)
{
    const unsigned NumBanks = sizeof(banknames)/sizeof(*banknames);
    int bankno = bank;
    if(bankno < 0)
        bankno = 0;
    device->AdlBank = bankno;
    ((MIDIplay*)device->adl_midiPlayer)->opl.AdlBank=device->AdlBank;
    if(device->AdlBank >= NumBanks)
    {
        std::stringstream s;
        s<<"bank number may only be 0.."<< (NumBanks-1)<<".\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }
    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_setNumFourOpsChn(ADL_MIDIPlayer *device, int ops4)
{
    device->NumFourOps = ops4;
    ((MIDIplay*)device->adl_midiPlayer)->opl.NumFourOps=device->NumFourOps;
    if(device->NumFourOps > 6 * device->NumCards)
    {
        std::stringstream s;
        s<<"number of four-op channels may only be 0.."<<(6*(device->NumCards))<<" when "<<device->NumCards<<" OPL3 cards are used.\n";
        ADLMIDI_ErrorString = s.str();
        return -1;
    }
    return adlRefreshNumCards(device);
}


ADLMIDI_EXPORT void adl_setPercMode(ADL_MIDIPlayer *device, int percmod)
{
    if(!device) return;
    device->AdlPercussionMode=percmod;
    ((MIDIplay*)device->adl_midiPlayer)->opl.AdlPercussionMode=(bool)device->AdlPercussionMode;
}

ADLMIDI_EXPORT void adl_setHVibrato(ADL_MIDIPlayer *device, int hvibro)
{
    if(!device) return;
    device->HighVibratoMode=hvibro;
    ((MIDIplay*)device->adl_midiPlayer)->opl.HighVibratoMode=(bool)device->HighVibratoMode;
}

ADLMIDI_EXPORT void adl_setHTremolo(ADL_MIDIPlayer *device, int htremo)
{
    if(!device) return;
    device->HighTremoloMode=htremo;
    ((MIDIplay*)device->adl_midiPlayer)->opl.HighTremoloMode=(bool)device->HighTremoloMode;
}

ADLMIDI_EXPORT void adl_setScaleModulators(ADL_MIDIPlayer *device, int smod)
{
    if(!device) return;
    device->ScaleModulators=smod;
    ((MIDIplay*)device->adl_midiPlayer)->opl.ScaleModulators=(bool)device->ScaleModulators;
}

ADLMIDI_EXPORT void adl_setLoopEnabled(ADL_MIDIPlayer *device, int loopEn)
{
    if(!device) return;
    device->QuitWithoutLooping=(int)(!(bool)loopEn);
}

ADLMIDI_EXPORT int adl_openFile(ADL_MIDIPlayer *device, char *filePath)
{
    Mixx::backup_samples_size=0;
    if(device && device->adl_midiPlayer)
    {
        if(!((MIDIplay *)device->adl_midiPlayer)->LoadMIDI(filePath))
        {
            ADLMIDI_ErrorString="ADL MIDI: Can't load file";
            return -1;
        } else return 0;
    }
    ADLMIDI_ErrorString="Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openData(ADL_MIDIPlayer* device, void *mem, long size)
{
    Mixx::backup_samples_size=0;
    if(device && device->adl_midiPlayer)
    {
        if(!((MIDIplay *)device->adl_midiPlayer)->LoadMIDI(mem, size))
        {
            ADLMIDI_ErrorString="ADL MIDI: Can't load data from memory";
            return -1;
        } else return 0;
    }
    ADLMIDI_ErrorString="Can't load file: ADL MIDI is not initialized";
    return -1;
}


ADLMIDI_EXPORT const char *adl_errorString()
{
    return ADLMIDI_ErrorString.c_str();
}

ADLMIDI_EXPORT const char *adl_getMusicTitle(ADL_MIDIPlayer *device)
{
    if(!device) return "";
    return ((MIDIplay*)(device->adl_midiPlayer))->musTitle.c_str();
}

ADLMIDI_EXPORT void adl_close(ADL_MIDIPlayer *device)
{
    Mixx::backup_samples_size=0;
    if(device->adl_midiPlayer) delete ((MIDIplay*)(device->adl_midiPlayer));
    device->adl_midiPlayer = NULL;
    free(device);
    device=NULL;
}

ADLMIDI_EXPORT void adl_reset(ADL_MIDIPlayer *device)
{
    if(!device) return;
    Mixx::backup_samples_size=0;
    ((MIDIplay*)device->adl_midiPlayer)->opl.Reset();
}

ADLMIDI_EXPORT int adl_play(ADL_MIDIPlayer*device, int sampleCount, int *out)
{
    if(!device) return 0;
    if(sampleCount<0) return 0;
    if(device->QuitFlag) return 0;

    int gotten_len=0;
    Mixx::requested_len=sampleCount;
    Mixx::_len=&gotten_len;
    Mixx::_out=out;

    unsigned long n_samples=512;
    unsigned long n_samples_2=n_samples*2;
    while(sampleCount>0)
    {
        if(Mixx::backup_samples_size>0)
        { //Send reserved samples if exist
            int ate=0;
            while((ate<Mixx::backup_samples_size) && (ate<sampleCount))
            {
                out[ate]=Mixx::backup_samples[ate]; ate++;
            }
            sampleCount-=ate;
            gotten_len+=ate;
            if(ate<Mixx::backup_samples_size)
            {
                for(int j=0;
                        j<ate;
                        j++)
                    Mixx::backup_samples[(ate-1)-j]=Mixx::backup_samples[(Mixx::backup_samples_size-1)-j];
            }
            Mixx::backup_samples_size-=ate;
        } else {
            const double eat_delay = device->delay < device->maxdelay ? device->delay : device->maxdelay;
            device->delay -= eat_delay;

            device->carry += device->PCM_RATE * eat_delay;
            n_samples = (unsigned) device->carry;
            device->carry -= n_samples;

            if(device->SkipForward > 0)
                device->SkipForward -= 1;
            else
            {
                sampleCount-=n_samples_2;
                if(device->NumCards == 1)
                {
                    ((MIDIplay*)(device->adl_midiPlayer))->opl.cards[0].Generate(0, Mixx::SendStereoAudio, n_samples);
                }
                else if(n_samples > 0)
                {
                    /* Mix together the audio from different cards */
                    static std::vector<int> sample_buf;
                    sample_buf.clear();
                    sample_buf.resize(n_samples*2);
                    struct Mix
                    {
                        static void AddStereoAudio(unsigned long count, int* samples)
                        {
                            for(unsigned long a=0; a<count*2; ++a)
                                sample_buf[a] += samples[a];
                        }
                    };
                    for(unsigned card = 0; card < device->NumCards; ++card)
                    {
                        ((MIDIplay*)(device->adl_midiPlayer))->opl.cards[card].Generate(
                            0,
                            Mix::AddStereoAudio,
                            n_samples);
                    }
                    /* Process it */
                    Mixx::SendStereoAudio(n_samples, &sample_buf[0]);
                }
                gotten_len += (n_samples*2)-Mixx::stored_samples;
            }
            device->delay = ((MIDIplay*)device->adl_midiPlayer)->Tick(eat_delay, device->mindelay);
        }
    }
    return gotten_len;
}




#ifdef ADLMIDI_buildAsApp

#undef main
int main(int argc, char** argv)
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
        for(unsigned a=0; a<sizeof(banknames)/sizeof(*banknames); ++a)
            std::printf("%10s%2u = %s\n",
                a?"":"Banks:",
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
            obtained.samples,obtained.freq,obtained.channels);

    ADL_MIDIPlayer* myDevice;
    myDevice = adl_init(44100);
    if(myDevice==NULL)
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
                  argv+2);
        argc -= (had_option ? 2 : 1);
    }

    if(argc >= 3)
    {
        int bankno = std::atoi(argv[2]);
        if(adl_setBank(myDevice, bankno)!=0)
        {
            std::fprintf(stderr,"%s", adl_errorString());
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
        if(adl_setNumFourOpsChn(myDevice, std::atoi(argv[4]))!=0)
        {
            std::fprintf(stderr, "%s\n", adl_errorString());
            return 0;
        }
    }

    if(adl_openFile(myDevice, argv[1])!=0)
    {
        std::fprintf(stderr, "%s\n", adl_errorString());
        return 2;
    }

    SDL_PauseAudio(0);

    while(1)
    {
        int buff[4096];
        unsigned long gotten=adl_play(myDevice, 4096, buff);
        if(gotten<=0) break;

        AudioBuffer_lock.Lock();
            size_t pos = AudioBuffer.size();
            AudioBuffer.resize(pos + gotten);
            for(unsigned long p = 0; p < gotten; ++p)
               AudioBuffer[pos+p] = buff[p];
        AudioBuffer_lock.Unlock();

        const SDL_AudioSpec& spec_ = obtained;
        while(AudioBuffer.size() > spec_.samples + (spec_.freq*2) * OurHeadRoomLength)
        {
            SDL_Delay(1);
        }
    }

    adl_close(myDevice);
    SDL_CloseAudio();
    return 0;
}
#endif

