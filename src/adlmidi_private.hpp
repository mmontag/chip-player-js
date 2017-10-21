/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2017 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef ADLMIDI_PRIVATE_HPP
#define ADLMIDI_PRIVATE_HPP

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

    #ifdef _MSC_VER
        #ifdef _WIN64
            typedef __int64 ssize_t;
        #else
            typedef __int32 ssize_t;
        #endif
        #define NOMINMAX //Don't override std::min and std::max
    #endif
    #include <windows.h>
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

#ifndef _WIN32
#include <errno.h>
#endif

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

#define ADL_UNUSED(x) (void)x

extern std::string ADLMIDI_ErrorString;

/*
    Smart pointer for C heaps, created with malloc() call.
    FAQ: Why not std::shared_ptr? Because of Android NDK now doesn't supports it
*/
template<class PTR>
class AdlMIDI_CPtr
{
    PTR* m_p;
public:
    AdlMIDI_CPtr() : m_p(NULL) {}
    ~AdlMIDI_CPtr()
    {
        reset(NULL);
    }

    void reset(PTR *p = NULL)
    {
        if(m_p)
            free(m_p);
        m_p = p;
    }

    PTR* get() { return m_p;}
    PTR& operator*() { return *m_p; }
    PTR* operator->() { return m_p; }
};

class MIDIplay;
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

    friend int adlRefreshNumCards(ADL_MIDIPlayer *device);
    std::vector<adlinsdata> dynamic_metainstruments; // Replaces adlins[] when CMF file
    std::vector<adldata>    dynamic_instruments;     // Replaces adl[]    when CMF file
    const unsigned  DynamicInstrumentTag /* = 0x8000u*/,
                    DynamicMetaInstrumentTag /* = 0x4000000u*/;
    const adlinsdata    &GetAdlMetaIns(unsigned n);
    unsigned            GetAdlMetaNumber(unsigned midiins);
    const adldata       &GetAdlIns(unsigned short insno);

public:
    unsigned int NumCards;
    unsigned int AdlBank;
    unsigned int NumFourOps;
    bool HighTremoloMode;
    bool HighVibratoMode;
    bool AdlPercussionMode;
    bool ScaleModulators;

    bool LogarithmicVolumes;
    char ___padding2[3];
    enum VolumesScale
    {
        VOLUME_Generic,
        VOLUME_CMF,
        VOLUME_DMX,
        VOLUME_APOGEE,
        VOLUME_9X,
    } m_volumeScale;

    OPL3();
    char ____padding3[8];
    std::vector<char> four_op_category; // 1 = quad-master, 2 = quad-slave, 0 = regular
    // 3 = percussion BassDrum
    // 4 = percussion Snare
    // 5 = percussion Tom
    // 6 = percussion Crash cymbal
    // 7 = percussion Hihat
    // 8 = percussion slave

    void Poke(size_t card, uint32_t index, uint32_t value);
    void PokeN(size_t card, uint16_t index, uint8_t value);
    void NoteOff(size_t c);
    void NoteOn(unsigned c, double hertz);
    void Touch_Real(unsigned c, unsigned volume);
    //void Touch(unsigned c, unsigned volume)

    void Patch(uint16_t c, uint16_t i);
    void Pan(unsigned c, unsigned value);
    void Silence();
    void updateFlags();
    void ChangeVolumeRangesModel(ADLMIDI_VolumeModels volumeModel);
    void Reset();
};


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
            uint64_t delay;
            int    status;
            char padding2[4];
            TrackInfo(): ptr(0), delay(0), status(0) {}
        };
        std::vector<TrackInfo> track;

        Position(): began(false), wait(0.0l), track() { }
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
              volume(100), expression(127),
              panning(0x30), vibrato(0), sustain(0),
              bend(0.0), bendsense(2 / 8192.0),
              vibpos(0), vibspeed(2 * 3.141592653 * 5.0),
              vibdepth(0.5 / 127), vibdelay(0),
              lastlrpn(0), lastmrpn(0), nrpn(false),
              activenotes() { }
    };
    std::vector<MIDIchannel> Ch;
    bool cmf_percussion_mode;

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
        void AddAge(int64_t ms);
    };
public:
    char ____padding[7];
private:
    std::vector<AdlChannel> ch;
    std::vector<std::vector<uint8_t> > TrackData;
public:
    MIDIplay();
    ~MIDIplay()
    {}

    ADL_MIDIPlayer *config;
    std::string musTitle;
    fraction<uint64_t> InvDeltaTicks, Tempo;
    bool    trackStart,
            atEnd,
            loopStart,
            loopEnd,
            loopStart_passed /*Tells that "loopStart" already passed*/,
            invalidLoop /*Loop points are invalid (loopStart after loopEnd or loopStart and loopEnd are on same place)*/,
            loopStart_hit /*loopStart entry was hited in previous tick*/;
    char ____padding2[2];
    OPL3 opl;
public:
    static uint64_t ReadBEint(const void *buffer, size_t nbytes);
    static uint64_t ReadLEint(const void *buffer, size_t nbytes);

    uint64_t ReadVarLen(size_t tk);
    uint64_t ReadVarLenEx(size_t tk, bool &ok);

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
            #ifndef _WIN32
            fp = std::fopen(path, "rb");
            #else
            wchar_t widePath[MAX_PATH];
            int size = MultiByteToWideChar(CP_UTF8, 0, path, (int)std::strlen(path), widePath, MAX_PATH);
            widePath[size] = '\0';
            fp = _wfopen(widePath, L"rb");
            #endif
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

        inline void seeku(uint64_t pos, int rel_to)
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

    bool LoadBank(const std::string &filename);
    bool LoadBank(void *data, unsigned long size);
    bool LoadBank(fileReader &fr);

    bool LoadMIDI(const std::string &filename);
    bool LoadMIDI(void *data, unsigned long size);
    bool LoadMIDI(fileReader &fr);

    /* Periodic tick handler.
     *   Input: s           = seconds since last call
     *   Input: granularity = don't expect intervals smaller than this, in seconds
     *   Output: desired number of seconds until next call
     */
    double Tick(double s, double granularity);

    void    rewind();

    /* RealTime event triggers */
    void realTime_ResetState();

    bool realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void realTime_NoteOff(uint8_t channel, uint8_t note);

    void realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal);
    void realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal);

    void realTime_Controller(uint8_t channel, uint8_t type, uint8_t value);

    void realTime_PatchChange(uint8_t channel, uint8_t patch);

    void realTime_PitchBend(uint8_t channel, uint16_t pitch);
    void realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb);

    void realTime_BankChangeLSB(uint8_t channel, uint8_t lsb);
    void realTime_BankChangeMSB(uint8_t channel, uint8_t msb);
    void realTime_BankChange(uint8_t channel, uint16_t bank);

private:
    enum
    {
        Upd_Patch  = 0x1,
        Upd_Pan    = 0x2,
        Upd_Volume = 0x4,
        Upd_Pitch  = 0x8,
        Upd_All    = Upd_Pan + Upd_Volume + Upd_Pitch,
        Upd_Off    = 0x20
    };

    void NoteUpdate(uint16_t MidCh,
                    MIDIchannel::activenoteiterator i,
                    unsigned props_mask,
                    int32_t select_adlchn = -1);
    bool ProcessEvents();
    void HandleEvent(size_t tk);

    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    long CalculateAdlChannelGoodness(unsigned c, uint16_t ins, uint16_t /*MidCh*/) const;

    // A new note will be played on this channel using this instrument.
    // Kill existing notes on this channel (or don't, if we do arpeggio)
    void PrepareAdlChannelForNewNote(size_t c, int ins);

    void KillOrEvacuate(
        size_t  from_channel,
        AdlChannel::users_t::iterator j,
        MIDIchannel::activenoteiterator i);
    void KillSustainingNotes(int32_t MidCh = -1, int32_t this_adlchn = -1);
    void SetRPN(unsigned MidCh, unsigned value, bool MSB);
    //void UpdatePortamento(unsigned MidCh)
    void NoteUpdate_All(uint16_t MidCh, unsigned props_mask);
    void NoteOff(uint16_t MidCh, uint8_t note);
    void UpdateVibrato(double amount);
    void UpdateArpeggio(double /*amount*/);

public:
    uint64_t ChooseDevice(const std::string &name);
};

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


extern int adlRefreshNumCards(ADL_MIDIPlayer *device);


#endif // ADLMIDI_PRIVATE_HPP
