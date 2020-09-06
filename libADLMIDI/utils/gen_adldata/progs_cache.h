#pragma once
#ifndef PROGS_H
#define PROGS_H

#include <map>
#include <set>
#include <utility>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <cassert>

struct InstBuffer_t
{
    uint8_t op1_amvib;      // 0
    uint8_t op2_amvib;      // 1
    uint8_t op1_atdec;      // 2
    uint8_t op2_atdec;      // 3
    uint8_t op1_susrel;     // 4
    uint8_t op2_susrel;     // 5
    uint8_t op1_wave;       // 6
    uint8_t op2_wave;       // 7
    uint8_t op1_ksltl;      // 8
    uint8_t op2_ksltl;      // 9
    uint8_t fbconn;         // 10

    bool operator==(const InstBuffer_t &b) const
    {
        return std::memcmp(this, &b, sizeof(InstBuffer_t)) == 0;
    }

    bool operator!=(const InstBuffer_t &b) const
    {
        return !operator==(b);
    }

    bool operator<(const InstBuffer_t &b) const
    {
        int c = std::memcmp(this, &b, 11);
        if(c != 0) return c < 0;
        return 0;
    }

    bool operator>(const InstBuffer_t &b) const
    {
        return !operator<(b) && operator!=(b);
    }
};

union InstBuffer
{
    InstBuffer_t d;
    uint8_t data[11];
};

inline bool equal_approx(double const a, double const b)
{
    int_fast64_t ai = static_cast<int_fast64_t>(a * 1000000.0);
    int_fast64_t bi = static_cast<int_fast64_t>(b * 1000000.0);
    return ai == bi;
}

enum VolumesModels
{
    VOLUME_Generic,
    VOLUME_CMF,
    VOLUME_DMX,
    VOLUME_APOGEE,
    VOLUME_9X
};

InstBuffer MakeNoSoundIns1();









struct BanksDump
{
    struct BankEntry
    {
        uint_fast32_t bankId = 0;
        std::string bankTitle = "Untitled";

        /* Global OPL flags */
        typedef enum WOPLFileFlags
        {
            /* Enable Deep-Tremolo flag */
            WOPL_FLAG_DEEP_TREMOLO = 0x01,
            /* Enable Deep-Vibrato flag */
            WOPL_FLAG_DEEP_VIBRATO = 0x02
        } WOPLFileFlags;

        /* Volume scaling model implemented in the libADLMIDI */
        typedef enum WOPL_VolumeModel
        {
            WOPL_VM_Generic = 0,
            WOPL_VM_Native,
            WOPL_VM_DMX,
            WOPL_VM_Apogee,
            WOPL_VM_Win9x,
            WOPL_VM_DMX_Fixed,
            WOPL_VM_Apogee_Fixed,
            WOPL_VM_AIL,
            WOPL_VM_Win9x_GeneralFM,
            WOPL_VM_HMI
        } WOPL_VolumeModel;

        /**
         * @brief Suggested bank setup in dependence from a driver that does use of this
         */
        enum BankSetup
        {
            SETUP_Generic = 0x0300,
            SETUP_Win9X   = 0x0304, // SB16
            SETUP_Win9XGF = 0x0308, // GeneralFM
            SETUP_DMX     = 0x0002,
            SETUP_Apogee  = 0x0003,
            SETUP_AIL     = 0x0307,
            SETUP_IBK     = 0x0301,
            SETUP_IMF     = 0x0200,
            SETUP_CMF     = 0x0201,
            SETUP_HMI     = 0x0309
        };

        uint_fast16_t       bankSetup = SETUP_Generic; // 0xAABB, AA - OPL flags, BB - Volume model
        std::vector<size_t> melodic;
        std::vector<size_t> percussion;

        explicit BankEntry() = default;

        BankEntry(const BankEntry &o)
        {
            bankId = o.bankId;
            bankTitle = o.bankTitle;
            bankSetup = o.bankSetup;
            melodic = o.melodic;
            percussion = o.percussion;
        }

        BankEntry(const BankEntry &&o)
        {
            bankId = std::move(o.bankId);
            bankTitle = std::move(o.bankTitle);
            bankSetup = std::move(o.bankSetup);
            melodic = std::move(o.melodic);
            percussion = std::move(o.percussion);
        }
    };

    struct MidiBank
    {
        uint_fast32_t midiBankId = 0;
        uint_fast8_t  msb = 0, lsb = 0;
        int_fast32_t  instruments[128];

        MidiBank()
        {
            for(size_t i = 0; i < 128; i++)
                instruments[i] = -1;
        }

        MidiBank(const MidiBank &o)
        {
            midiBankId = o.midiBankId;
            msb = o.msb;
            lsb = o.lsb;
            std::memcpy(instruments, o.instruments, sizeof(int_fast32_t) * 128);
        }

        bool operator==(const MidiBank &o)
        {
            if(msb != o.msb)
                return false;
            if(lsb != o.lsb)
                return false;
            if(std::memcmp(instruments, o.instruments, sizeof(int_fast32_t) * 128) != 0)
                return false;
            return true;
        }
        bool operator!=(const MidiBank &o)
        {
            return !operator==(o);
        }
    };

    struct InstrumentEntry
    {
        uint_fast32_t instId = 0;
        std::vector<std::string> instMetas;

        typedef enum WOPL_InstrumentFlags
        {
            /* Is two-operator single-voice instrument (no flags) */
            WOPL_Ins_2op        = 0x00,
            /* Is true four-operator instrument */
            WOPL_Ins_4op        = 0x01,
            /* Is pseudo four-operator (two 2-operator voices) instrument */
            WOPL_Ins_Pseudo4op  = 0x02,
            /* Is a blank instrument entry */
            WOPL_Ins_IsBlank    = 0x04,

            /* RythmMode flags mask */
            WOPL_RhythmModeMask  = 0x38,

            /* Mask of the flags range */
            WOPL_Ins_ALL_MASK   = 0x07
        } WOPL_InstrumentFlags;

        typedef enum WOPL_RhythmMode
        {
            /* RythmMode: BassDrum */
            WOPL_RM_BassDrum  = 0x08,
            /* RythmMode: Snare */
            WOPL_RM_Snare     = 0x10,
            /* RythmMode: TomTom */
            WOPL_RM_TomTom    = 0x18,
            /* RythmMode: Cymbell */
            WOPL_RM_Cymbal   = 0x20,
            /* RythmMode: HiHat */
            WOPL_RM_HiHat     = 0x28
        } WOPL_RhythmMode;

        int_fast8_t   noteOffset1 = 0;
        int_fast8_t   noteOffset2 = 0;
        int_fast8_t   midiVelocityOffset = 0;
        uint_fast8_t  percussionKeyNumber = 0;
        uint_fast32_t instFlags = 0;
        int_fast8_t   secondVoiceDetune = 0;
        /*
            2op: modulator1, carrier1, feedback1
            2vo: modulator1, carrier1, modulator2, carrier2, feedback(1+2)
            4op: modulator1, carrier1, modulator2, carrier2, feedback1
        */
        //! Contains FeedBack-Connection for both operators 0xBBAA - AA - first, BB - second
        uint_fast16_t fbConn = 0;
        int_fast64_t delay_on_ms = 0;
        int_fast64_t delay_off_ms = 0;
        int_fast32_t ops[5] = {-1, -1, -1, -1, -1};

        void setFbConn(uint_fast16_t fbConn1, uint_fast16_t fbConn2 = 0x00);

        bool operator==(const InstrumentEntry &o)
        {
            return (
                (noteOffset1 == o.noteOffset1) &&
                (noteOffset2 == o.noteOffset2) &&
                (midiVelocityOffset == o.midiVelocityOffset) &&
                (percussionKeyNumber == o.percussionKeyNumber) &&
                (instFlags == o.instFlags) &&
                (secondVoiceDetune == o.secondVoiceDetune) &&
                (fbConn == o.fbConn) &&
                (delay_on_ms == o.delay_on_ms) &&
                (delay_off_ms == o.delay_off_ms) &&
                (std::memcmp(ops, o.ops, sizeof(int_fast32_t) * 5) == 0)
            );
        }
        bool operator!=(const InstrumentEntry &o)
        {
            return !operator==(o);
        }
    };

    struct Operator
    {
        uint_fast32_t opId = 0;
        uint_fast32_t d_E862 = 0;
        uint_fast32_t d_40 = 0;
        explicit Operator() {}
        Operator(const Operator &o)
        {
            opId = o.opId;
            d_E862 = o.d_E862;
            d_40 = o.d_40;
        }
        bool operator==(const Operator &o)
        {
            return ((d_E862 == o.d_E862) && (d_40 == o.d_40));
        }
        bool operator!=(const Operator &o)
        {
            return !operator==(o);
        }
    };

    std::vector<BankEntry>       banks;
    std::vector<MidiBank>        midiBanks;
    std::vector<InstrumentEntry> instruments;
    std::vector<Operator>        operators;

    static void toOps(const InstBuffer_t &inData, Operator *outData, size_t begin = 0);
    //! WIP
    static bool isSilent(const BanksDump &db, const BanksDump::InstrumentEntry &ins, bool moreInfo = false);
    static bool isSilent(const Operator *ops, uint_fast16_t fbConn, size_t countOps = 2, bool pseudo4op = false, bool moreInfo = false);

    size_t initBank(size_t bankId, const std::string &title, uint_fast16_t bankSetup);
    void addMidiBank(size_t bankId, bool percussion, MidiBank b);
    void addInstrument(MidiBank &bank, size_t patchId, InstrumentEntry e, Operator *ops, const std::string &meta = std::string());
    void exportBanks(const std::string &outPath, const std::string &headerName = "adlmidi_db.h");
};


namespace BankFormats
{

bool LoadMiles(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadBisqwit(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadBNK(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix, bool is_fat, bool percussive);
bool LoadBNK2(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix,
                     const std::string &melo_filter,
                     const std::string &perc_filter);
bool LoadEA(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadIBK(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix, bool percussive, bool noRhythmMode = false);
bool LoadJunglevision(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadDoom(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadTMB(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix);
bool LoadWopl(BanksDump &db, const char *fn, unsigned bank, const std::string bankTitle, const char *prefix);

}

#endif // PROGS_H
