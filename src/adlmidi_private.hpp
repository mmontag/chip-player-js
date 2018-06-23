/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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
#   if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#       define ADLMIDI_EXPORT __declspec(dllexport)
#   elif defined (LIBADLMIDI_VISIBILITY)
#       define ADLMIDI_EXPORT __attribute__((visibility ("default")))
#   else
#       define ADLMIDI_EXPORT
#   endif
#endif

// Require declarations of unstable API for extern "C"
#define ADLMIDI_UNSTABLE_API

#ifdef _WIN32
#define NOMINMAX 1
#endif

#if defined(_WIN32) && !defined(__WATCOMC__)
#   undef NO_OLDNAMES
#       include <stdint.h>
#   ifdef _MSC_VER
#       ifdef _WIN64
typedef __int64 ssize_t;
#       else
typedef __int32 ssize_t;
#       endif
#       define NOMINMAX 1 //Don't override std::min and std::max
#   else
#       ifdef _WIN64
typedef int64_t ssize_t;
#       else
typedef int32_t ssize_t;
#       endif
#   endif
#   include <windows.h>
#endif

#if defined(__DJGPP__) || (defined(__WATCOMC__) && (defined(__DOS__) || defined(__DOS4G__) || defined(__DOS4GNZ__)))
#define ADLMIDI_HW_OPL
#include <conio.h>
#ifdef __DJGPP__
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <dos.h>
#endif

#endif

#include <vector>
#include <list>
#include <string>
//#ifdef __WATCOMC__
//#include <myset.h> //TODO: Implemnet a workaround for OpenWatcom to fix a crash while using those containers
//#include <mymap.h>
//#else
#include <map>
#include <set>
//#endif
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
#if defined(__WATCOMC__)
#include <math.h> // round, sqrt
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits> // numeric_limit

#ifndef _WIN32
#include <errno.h>
#endif

#include <deque>
#include <algorithm>

/*
 * Workaround for some compilers are has no those macros in their headers!
 */
#ifndef INT8_MIN
#define INT8_MIN    (-0x7f - 1)
#endif
#ifndef INT16_MIN
#define INT16_MIN   (-0x7fff - 1)
#endif
#ifndef INT32_MIN
#define INT32_MIN   (-0x7fffffff - 1)
#endif
#ifndef INT8_MAX
#define INT8_MAX    0x7f
#endif
#ifndef INT16_MAX
#define INT16_MAX   0x7fff
#endif
#ifndef INT32_MAX
#define INT32_MAX   0x7fffffff
#endif

#include "file_reader.hpp"

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
// Rename class to avoid ABI collisions
#define BW_MidiSequencer AdlMidiSequencer
#include "midi_sequencer.hpp"
typedef BW_MidiSequencer MidiSequencer;
#endif//ADLMIDI_DISABLE_MIDI_SEQUENCER

#ifndef ADLMIDI_HW_OPL
#include "chips/opl_chip_base.h"
#endif

#include "adldata.hh"
#include "adlmidi.h"    //Main API
#ifndef ADLMIDI_DISABLE_CPP_EXTRAS
#include "adlmidi.hpp"  //Extra C++ API
#endif
#include "adlmidi_ptr.hpp"
#include "adlmidi_bankmap.h"

#define ADL_UNUSED(x) (void)x

#define OPL_PANNING_LEFT    0x10
#define OPL_PANNING_RIGHT   0x20
#define OPL_PANNING_BOTH    0x30

extern std::string ADLMIDI_ErrorString;

/*
  Sample conversions to various formats
*/
template <class Real>
inline Real adl_cvtReal(int32_t x)
{
    return x * ((Real)1 / INT16_MAX);
}

inline int32_t adl_cvtS16(int32_t x)
{
    x = (x < INT16_MIN) ? (INT16_MIN) : x;
    x = (x > INT16_MAX) ? (INT16_MAX) : x;
    return x;
}

inline int32_t adl_cvtS8(int32_t x)
{
    return adl_cvtS16(x) / 256;
}
inline int32_t adl_cvtS24(int32_t x)
{
    return adl_cvtS16(x) * 256;
}
inline int32_t adl_cvtS32(int32_t x)
{
    return adl_cvtS16(x) * 65536;
}
inline int32_t adl_cvtU16(int32_t x)
{
    return adl_cvtS16(x) - INT16_MIN;
}
inline int32_t adl_cvtU8(int32_t x)
{
    return (adl_cvtS16(x) / 256) - INT8_MIN;
}
inline int32_t adl_cvtU24(int32_t x)
{
    enum { int24_min = -(1 << 23) };
    return adl_cvtS24(x) - int24_min;
}
inline int32_t adl_cvtU32(int32_t x)
{
    // unsigned operation because overflow on signed integers is undefined
    return (uint32_t)adl_cvtS32(x) - (uint32_t)INT32_MIN;
}

struct ADL_MIDIPlayer;
class OPL3
{
public:
    friend class MIDIplay;
    friend class AdlInstrumentTester;
    uint32_t NumChannels;
    char ____padding[4];
#ifndef ADLMIDI_HW_OPL
    std::vector<AdlMIDI_SPtr<OPLChipBase > > cardsOP2;
#endif
private:
    std::vector<adldata>    ins;  // patch data, cached, needed by Touch()
    std::vector<uint8_t>    pit;  // value poked to B0, cached, needed by NoteOff)(
    std::vector<uint8_t>    regBD;

    friend int adlRefreshNumCards(ADL_MIDIPlayer *device);
public:
    struct Bank
    {
        adlinsdata2 ins[128];
    };
    typedef BasicBankMap<Bank> BankMap;
    BankMap dynamic_banks;
    AdlBankSetup dynamic_bank_setup;
public:
    void    setEmbeddedBank(unsigned int bank);
    static const adlinsdata2 emptyInstrument;
    enum { PercussionTag = 1 << 15 };

    //! Total number of running concurrent emulated chips
    unsigned int NumCards;
    //! Currently running embedded bank number. "~0" means usign of the custom bank.
    unsigned int AdlBank;
    //! Total number of needed four-operator channels in all running chips
    unsigned int NumFourOps;
    //! Turn global Deep Tremolo mode on
    bool HighTremoloMode;
    //! Turn global Deep Vibrato mode on
    bool HighVibratoMode;
    //! Use AdLib percussion mode
    bool AdlPercussionMode;
    //! Carriers-only are scaled by default by volume level. This flag will tell to scale modulators too.
    bool ScaleModulators;
    //! Run emulator at PCM rate if that possible. Reduces sounding accuracy, but decreases CPU usage on lower rates.
    bool runAtPcmRate;
    // ! Required to play CMF files. Can be turned on by using of "CMF" volume model
    //bool LogarithmicVolumes; //[REPLACED WITH "m_volumeScale == VOLUME_NATIVE", DEPRECATED!!!]
    // ! Required to play EA-MUS files [REPLACED WITH "m_musicMode", DEPRECATED!!!]
    //bool CartoonersVolumes;
    enum MusicMode
    {
        MODE_MIDI,
        MODE_IMF,
        MODE_CMF,
        MODE_RSXX
    } m_musicMode;
    //! Just a padding. Reserved.
    char ___padding2[3];
    //! Volume models enum
    enum VolumesScale
    {
        VOLUME_Generic,
        VOLUME_NATIVE,
        VOLUME_DMX,
        VOLUME_APOGEE,
        VOLUME_9X
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

    void Poke(size_t card, uint16_t index, uint8_t value);

    void NoteOff(size_t c);
    void NoteOn(unsigned c, double hertz);
    void Touch_Real(unsigned c, unsigned volume, uint8_t brightness = 127);
    //void Touch(unsigned c, unsigned volume)

    void Patch(uint16_t c, const adldata &adli);
    void Pan(unsigned c, unsigned value);
    void Silence();
    void updateFlags();
    void updateDeepFlags();
    void ChangeVolumeRangesModel(ADLMIDI_VolumeModels volumeModel);
    #ifndef ADLMIDI_HW_OPL
    void ClearChips();
    #endif
    void Reset(int emulator, unsigned long PCM_RATE, void *audioTickHandler);
};


/**
 * @brief Hooks of the internal events
 */
struct MIDIEventHooks
{
    MIDIEventHooks() :
        onNote(NULL),
        onNote_userData(NULL),
        onDebugMessage(NULL),
        onDebugMessage_userData(NULL)
    {}

    //! Note on/off hooks
    typedef void (*NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);
    NoteHook     onNote;
    void         *onNote_userData;

    //! Library internal debug messages
    typedef void (*DebugMessageHook)(void *userdata, const char *fmt, ...);
    DebugMessageHook onDebugMessage;
    void *onDebugMessage_userData;
};


class MIDIplay
{
    friend void adl_reset(struct ADL_MIDIPlayer*);
public:
    explicit MIDIplay(unsigned long sampleRate = 22050);

    ~MIDIplay()
    {}

    void applySetup();

    /**********************Internal structures and classes**********************/

    // Persistent settings for each MIDI channel
    struct MIDIchannel
    {
        uint8_t bank_lsb, bank_msb;
        uint8_t patch;
        uint8_t volume, expression;
        uint8_t panning, vibrato, aftertouch;
        uint16_t portamento;
        bool sustain;
        bool softPedal;
        bool portamentoEnable;
        int8_t portamentoSource;  // note number or -1
        double portamentoRate;
        //! Per note Aftertouch values
        uint8_t noteAftertouch[128];
        //! Is note aftertouch has any non-zero value
        bool    noteAfterTouchInUse;
        char ____padding[6];
        int bend;
        double bendsense;
        int bendsense_lsb, bendsense_msb;
        double  vibpos, vibspeed, vibdepth;
        int64_t vibdelay;
        uint8_t lastlrpn, lastmrpn;
        bool nrpn;
        uint8_t brightness;
        bool is_xg_percussion;
        struct NoteInfo
        {
            uint8_t note;
            bool active;
            // Current pressure
            uint8_t vol;
            // Note vibrato (a part of Note Aftertouch feature)
            uint8_t vibrato;
            // Tone selected on noteon:
            int16_t noteTone;
            // Current tone (!= noteTone if gliding note)
            double currentTone;
            // Gliding rate
            double glideRate;
            // Patch selected on noteon; index to bank.ins[]
            size_t  midiins;
            // Patch selected
            const adlinsdata2 *ains;
            enum
            {
                MaxNumPhysChans = 2,
                MaxNumPhysItemCount = MaxNumPhysChans,
            };
            struct Phys
            {
                //! Destination chip channel
                uint16_t chip_chan;
                //! ins, inde to adl[]
                adldata ains;
                //! Is this voice must be detunable?
                bool    pseudo4op;

                void assign(const Phys &oth)
                {
                    ains = oth.ains;
                    pseudo4op = oth.pseudo4op;
                }
                bool operator==(const Phys &oth) const
                {
                    return (ains == oth.ains) && (pseudo4op == oth.pseudo4op);
                }
                bool operator!=(const Phys &oth) const
                {
                    return !operator==(oth);
                }
            };
            //! List of OPL3 channels it is currently occupying.
            Phys chip_channels[MaxNumPhysItemCount];
            //! Count of used channels.
            unsigned chip_channels_count;
            //
            Phys *phys_find(unsigned chip_chan)
            {
                Phys *ph = NULL;
                for(unsigned i = 0; i < chip_channels_count && !ph; ++i)
                    if(chip_channels[i].chip_chan == chip_chan)
                        ph = &chip_channels[i];
                return ph;
            }
            Phys *phys_find_or_create(uint16_t chip_chan)
            {
                Phys *ph = phys_find(chip_chan);
                if(!ph) {
                    if(chip_channels_count < MaxNumPhysItemCount) {
                        ph = &chip_channels[chip_channels_count++];
                        ph->chip_chan = chip_chan;
                    }
                }
                return ph;
            }
            Phys *phys_ensure_find_or_create(uint16_t chip_chan)
            {
                Phys *ph = phys_find_or_create(chip_chan);
                assert(ph);
                return ph;
            }
            void phys_erase_at(const Phys *ph)
            {
                intptr_t pos = ph - chip_channels;
                assert(pos < static_cast<intptr_t>(chip_channels_count));
                for(intptr_t i = pos + 1; i < static_cast<intptr_t>(chip_channels_count); ++i)
                    chip_channels[i - 1] = chip_channels[i];
                --chip_channels_count;
            }
            void phys_erase(unsigned chip_chan)
            {
                Phys *ph = phys_find(chip_chan);
                if(ph)
                    phys_erase_at(ph);
            }
        };
        char ____padding2[5];
        unsigned gliding_note_count;
        NoteInfo activenotes[128];

        struct activenoteiterator
        {
            explicit activenoteiterator(NoteInfo *info = 0)
                : ptr(info) {}
            activenoteiterator &operator++()
            {
                if(ptr->note == 127)
                    ptr = 0;
                else
                    for(++ptr; ptr && !ptr->active;)
                        ptr = (ptr->note == 127) ? 0 : (ptr + 1);
                return *this;
            }
            activenoteiterator operator++(int)
            {
                activenoteiterator pos = *this;
                ++*this;
                return pos;
            }
            NoteInfo &operator*() const
                { return *ptr; }
            NoteInfo *operator->() const
                { return ptr; }
            bool operator==(activenoteiterator other) const
                { return ptr == other.ptr; }
            bool operator!=(activenoteiterator other) const
                { return ptr != other.ptr; }
            operator NoteInfo *() const
                { return ptr; }
        private:
            NoteInfo *ptr;
        };

        activenoteiterator activenotes_begin()
        {
            activenoteiterator it(activenotes);
            return (it->active) ? it : ++it;
        }

        activenoteiterator activenotes_find(uint8_t note)
        {
            assert(note < 128);
            return activenoteiterator(
                activenotes[note].active ? &activenotes[note] : 0);
        }

        activenoteiterator activenotes_ensure_find(uint8_t note)
        {
            activenoteiterator it = activenotes_find(note);
            assert(it);
            return it;
        }

        std::pair<activenoteiterator, bool> activenotes_insert(uint8_t note)
        {
            assert(note < 128);
            NoteInfo &info = activenotes[note];
            bool inserted = !info.active;
            if(inserted) info.active = true;
            return std::pair<activenoteiterator, bool>(activenoteiterator(&info), inserted);
        }

        void activenotes_erase(activenoteiterator pos)
        {
            if(pos)
                pos->active = false;
        }

        bool activenotes_empty()
        {
            return !activenotes_begin();
        }

        void activenotes_clear()
        {
            for(uint8_t i = 0; i < 128; ++i) {
                activenotes[i].note = i;
                activenotes[i].active = false;
            }
        }

        void reset()
        {
            resetAllControllers();
            patch = 0;
            vibpos = 0;
            bank_lsb = 0;
            bank_msb = 0;
            lastlrpn = 0;
            lastmrpn = 0;
            nrpn = false;
            is_xg_percussion = false;
        }
        void resetAllControllers()
        {
            bend = 0;
            bendsense_msb = 2;
            bendsense_lsb = 0;
            updateBendSensitivity();
            volume  = 100;
            expression = 127;
            sustain = false;
            softPedal = false;
            vibrato = 0;
            aftertouch = 0;
            std::memset(noteAftertouch, 0, 128);
            noteAfterTouchInUse = false;
            vibspeed = 2 * 3.141592653 * 5.0;
            vibdepth = 0.5 / 127;
            vibdelay = 0;
            panning = OPL_PANNING_BOTH;
            portamento = 0;
            portamentoEnable = false;
            portamentoSource = -1;
            portamentoRate = HUGE_VAL;
            brightness = 127;
        }
        bool hasVibrato()
        {
            return (vibrato > 0) || (aftertouch > 0) || noteAfterTouchInUse;
        }
        void updateBendSensitivity()
        {
            int cent = bendsense_msb * 128 + bendsense_lsb;
            bendsense = cent * (1.0 / (128 * 8192));
        }
        MIDIchannel()
        {
            activenotes_clear();
            gliding_note_count = 0;
            reset();
        }
    };

    // Additional information about OPL3 channels
    struct AdlChannel
    {
        struct Location
        {
            uint16_t    MidCh;
            uint8_t     note;
            bool operator==(const Location &l) const
                { return MidCh == l.MidCh && note == l.note; }
            bool operator!=(const Location &l) const
                { return !operator==(l); }
        };
        struct LocationData
        {
            LocationData *prev, *next;
            Location loc;
            enum {
                Sustain_None        = 0x00,
                Sustain_Pedal       = 0x01,
                Sustain_Sostenuto   = 0x02,
                Sustain_ANY         = Sustain_Pedal | Sustain_Sostenuto,
            };
            uint8_t sustained;
            char ____padding[6];
            MIDIchannel::NoteInfo::Phys ins;  // a copy of that in phys[]
            //! Has fixed sustain, don't iterate "on" timeout
            bool    fixed_sustain;
            //! Timeout until note will be allowed to be killed by channel manager while it is on
            int64_t kon_time_until_neglible;
            int64_t vibdelay;
        };

        // If the channel is keyoff'd
        int64_t koff_time_until_neglible;

        enum { users_max = 128 };
        LocationData *users_first, *users_free_cells;
        LocationData users_cells[users_max];
        unsigned users_size;

        bool users_empty() const;
        LocationData *users_find(Location loc);
        LocationData *users_allocate();
        LocationData *users_find_or_create(Location loc);
        LocationData *users_insert(const LocationData &x);
        void users_erase(LocationData *user);
        void users_clear();
        void users_assign(const LocationData *users, size_t count);

        // For channel allocation:
        AdlChannel(): koff_time_until_neglible(0)
        {
            users_clear();
        }

        AdlChannel(const AdlChannel &oth): koff_time_until_neglible(oth.koff_time_until_neglible)
        {
            if(oth.users_first)
            {
                users_first = NULL;
                users_assign(oth.users_first, oth.users_size);
            }
            else
                users_clear();
        }

        AdlChannel &operator=(const AdlChannel &oth)
        {
            koff_time_until_neglible = oth.koff_time_until_neglible;
            users_assign(oth.users_first, oth.users_size);
            return *this;
        }

        void AddAge(int64_t ms);
    };

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief MIDI files player sequencer
     */
    MidiSequencer m_sequencer;

    /**
     * @brief Interface between MIDI sequencer and this library
     */
    BW_MidiRtInterface m_sequencerInterface;

    /**
     * @brief Initialize MIDI sequencer interface
     */
    void initSequencerInterface();
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

    struct Setup
    {
        int          emulator;
        bool         runAtPcmRate;
        unsigned int AdlBank;
        unsigned int NumFourOps;
        unsigned int NumCards;
        int     HighTremoloMode;
        int     HighVibratoMode;
        int     AdlPercussionMode;
        bool    LogarithmicVolumes;
        int     VolumeModel;
        //unsigned int SkipForward;
        int     ScaleModulators;
        bool    fullRangeBrightnessCC74;

        double delay;
        double carry;

        /* The lag between visual content and audio content equals */
        /* the sum of these two buffers. */
        double mindelay;
        double maxdelay;

        /* For internal usage */
        ssize_t tick_skip_samples_delay; /* Skip tick processing after samples count. */
        /* For internal usage */

        unsigned long PCM_RATE;
    };

    struct MIDI_MarkerEntry
    {
        std::string     label;
        double          pos_time;
        uint64_t        pos_ticks;
    };

    std::vector<MIDIchannel> Ch;
    bool cmf_percussion_mode;
    uint8_t m_masterVolume;
    uint8_t m_sysExDeviceId;

    enum SynthMode
    {
        Mode_GM  = 0x00,
        Mode_GS  = 0x01,
        Mode_XG  = 0x02,
        Mode_GM2 = 0x04,
    };
    uint32_t m_synthMode;

    MIDIEventHooks hooks;

private:
    std::map<std::string, uint64_t> devices;
    std::map<uint64_t /*track*/, uint64_t /*channel begin index*/> current_device;

    //Padding to fix CLanc code model's warning
    char ____padding[7];

    std::vector<AdlChannel> ch;
    //! Counter of arpeggio processing
    size_t m_arpeggioCounter;

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    //! Audio tick counter
    uint32_t m_audioTickCounter;
#endif

    //! Local error string
    std::string errorStringOut;

    //! Missing instruments catches
    std::set<uint8_t> caugh_missing_instruments;
    //! Missing melodic banks catches
    std::set<uint16_t> caugh_missing_banks_melodic;
    //! Missing percussion banks catches
    std::set<uint16_t> caugh_missing_banks_percussion;

public:

    const std::string &getErrorString();
    void setErrorString(const std::string &err);

    OPL3 opl;

    int32_t outBuf[1024];

    Setup m_setup;

    bool LoadBank(const std::string &filename);
    bool LoadBank(const void *data, size_t size);
    bool LoadBank(FileAndMemReader &fr);

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief MIDI file loading pre-process
     * @return true on success, false on failure
     */
    bool LoadMIDI_pre();

    /**
     * @brief MIDI file loading post-process
     * @return true on success, false on failure
     */
    bool LoadMIDI_post();

    /**
     * @brief Load music file from a file
     * @param filename Path to music file
     * @return true on success, false on failure
     */

    bool LoadMIDI(const std::string &filename);

    /**
     * @brief Load music file from the memory block
     * @param data pointer to the memory block
     * @param size size of memory block
     * @return true on success, false on failure
     */
    bool LoadMIDI(const void *data, size_t size);

    /**
     * @brief Periodic tick handler.
     * @param s seconds since last call
     * @param granularity don't expect intervals smaller than this, in seconds
     * @return desired number of seconds until next call
     */
    double Tick(double s, double granularity);
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

    /**
     * @brief Process extra iterators like vibrato or arpeggio
     * @param s seconds since last call
     */
    void   TickIterators(double s);


    /* RealTime event triggers */
    /**
     * @brief Reset state of all channels
     */
    void realTime_ResetState();

    /**
     * @brief Note On event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     * @param velocity Velocity level (from 0 to 127)
     * @return true if Note On event was accepted
     */
    bool realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);

    /**
     * @brief Note Off event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     */
    void realTime_NoteOff(uint8_t channel, uint8_t note);

    /**
     * @brief Note aftertouch event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     * @param atVal After-Touch level (from 0 to 127)
     */
    void realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal);

    /**
     * @brief Channel aftertouch event
     * @param channel MIDI channel
     * @param atVal After-Touch level (from 0 to 127)
     */
    void realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal);

    /**
     * @brief Controller Change event
     * @param channel MIDI channel
     * @param type Type of controller
     * @param value Value of the controller (from 0 to 127)
     */
    void realTime_Controller(uint8_t channel, uint8_t type, uint8_t value);

    /**
     * @brief Patch change
     * @param channel MIDI channel
     * @param patch Patch Number (from 0 to 127)
     */
    void realTime_PatchChange(uint8_t channel, uint8_t patch);

    /**
     * @brief Pitch bend change
     * @param channel MIDI channel
     * @param pitch Concoctated raw pitch value
     */
    void realTime_PitchBend(uint8_t channel, uint16_t pitch);

    /**
     * @brief Pitch bend change
     * @param channel MIDI channel
     * @param msb MSB of pitch value
     * @param lsb LSB of pitch value
     */
    void realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb);

    /**
     * @brief LSB Bank Change CC
     * @param channel MIDI channel
     * @param lsb LSB value of bank number
     */
    void realTime_BankChangeLSB(uint8_t channel, uint8_t lsb);

    /**
     * @brief MSB Bank Change CC
     * @param channel MIDI channel
     * @param lsb MSB value of bank number
     */
    void realTime_BankChangeMSB(uint8_t channel, uint8_t msb);

    /**
     * @brief Bank Change (united value)
     * @param channel MIDI channel
     * @param bank Bank number value
     */
    void realTime_BankChange(uint8_t channel, uint16_t bank);

    /**
     * @brief Sets the Device identifier
     * @param id 7-bit Device identifier
     */
    void setDeviceId(uint8_t id);

    /**
     * @brief System Exclusive message
     * @param msg Raw SysEx Message
     * @param size Length of SysEx message
     * @return true if message was passed successfully. False on any errors
     */
    bool realTime_SysEx(const uint8_t *msg, size_t size);

    /**
     * @brief Turn off all notes and mute the sound of releasing notes
     */
    void realTime_panic();

    /**
     * @brief Device switch (to extend 16-channels limit of MIDI standard)
     * @param track MIDI track index
     * @param data Device name
     * @param length Length of device name string
     */
    void realTime_deviceSwitch(size_t track, const char *data, size_t length);

    /**
     * @brief Currently selected device index
     * @param track MIDI track index
     * @return Multiple 16 value
     */
    uint64_t realTime_currentDevice(size_t track);

    /**
     * @brief Send raw OPL chip command
     * @param reg OPL Register
     * @param value Value to write
     */
    void realTime_rawOPL(uint8_t reg, uint8_t value);

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    // Audio rate tick handler
    void AudioTick(uint32_t chipId, uint32_t rate);
#endif

private:
    enum
    {
        Manufacturer_Roland               = 0x41,
        Manufacturer_Yamaha               = 0x43,
        Manufacturer_UniversalNonRealtime = 0x7E,
        Manufacturer_UniversalRealtime    = 0x7F
    };
    enum
    {
        RolandMode_Request = 0x11,
        RolandMode_Send    = 0x12
    };
    enum
    {
        RolandModel_GS   = 0x42,
        RolandModel_SC55 = 0x45,
        YamahaModel_XG   = 0x4C
    };

    bool doUniversalSysEx(unsigned dev, bool realtime, const uint8_t *data, size_t size);
    bool doRolandSysEx(unsigned dev, const uint8_t *data, size_t size);
    bool doYamahaSysEx(unsigned dev, const uint8_t *data, size_t size);

private:
    enum
    {
        Upd_Patch  = 0x1,
        Upd_Pan    = 0x2,
        Upd_Volume = 0x4,
        Upd_Pitch  = 0x8,
        Upd_All    = Upd_Pan + Upd_Volume + Upd_Pitch,
        Upd_Off    = 0x20,
        Upd_Mute   = 0x40,
        Upd_OffMute = Upd_Off + Upd_Mute
    };

    void NoteUpdate(uint16_t MidCh,
                    MIDIchannel::activenoteiterator i,
                    unsigned props_mask,
                    int32_t select_adlchn = -1);

    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    int64_t CalculateAdlChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins, uint16_t /*MidCh*/) const;

    // A new note will be played on this channel using this instrument.
    // Kill existing notes on this channel (or don't, if we do arpeggio)
    void PrepareAdlChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins);

    void KillOrEvacuate(
        size_t  from_channel,
        AdlChannel::LocationData *j,
        MIDIchannel::activenoteiterator i);
    void Panic();
    void KillSustainingNotes(int32_t MidCh = -1,
                             int32_t this_adlchn = -1,
                             uint8_t sustain_type = AdlChannel::LocationData::Sustain_ANY);
    void MarkSostenutoNotes(int32_t MidCh = -1);
    void SetRPN(unsigned MidCh, unsigned value, bool MSB);
    void UpdatePortamento(unsigned MidCh);
    void NoteUpdate_All(uint16_t MidCh, unsigned props_mask);
    void NoteOff(uint16_t MidCh, uint8_t note);

    void UpdateVibrato(double amount);
    void UpdateArpeggio(double /*amount*/);
    void UpdateGlide(double amount);

public:
    uint64_t ChooseDevice(const std::string &name);
};

// I think, this is useless inside of Library
/*
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
*/

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
extern void adl_audioTickHandler(void *instance, uint32_t chipId, uint32_t rate);
#endif
extern int adlRefreshNumCards(ADL_MIDIPlayer *device);


#endif // ADLMIDI_PRIVATE_HPP
