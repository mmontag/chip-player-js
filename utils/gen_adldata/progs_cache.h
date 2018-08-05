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

struct insdata
{
    uint8_t         data[11];
    int8_t          finetune;
    bool            diff;
    bool operator==(const insdata &b) const
    {
        return (std::memcmp(data, b.data, 11) == 0) && (finetune == b.finetune) && (diff == b.diff);
    }
    bool operator!=(const insdata &b) const
    {
        return !operator==(b);
    }
    bool operator<(const insdata &b) const
    {
        int c = std::memcmp(data, b.data, 11);
        if(c != 0) return c < 0;
        if(finetune != b.finetune) return finetune < b.finetune;
        if(diff != b.diff) return (diff) == (!b.diff);
        return 0;
    }
    bool operator>(const insdata &b) const
    {
        return !operator<(b) && operator!=(b);
    }
};

inline bool equal_approx(double const a, double const b)
{
    double const epsilon(std::numeric_limits<double>::epsilon() * 100);
    double const scale(1.0);
    return std::fabs(a - b) < epsilon * (scale + (std::max)(std::fabs(a), std::fabs(b)));
}

struct ins
{
    enum { Flag_Pseudo4op = 0x01, Flag_NoSound = 0x02, Flag_Real4op = 0x04 };
    size_t insno1, insno2;
    unsigned char notenum;
    bool pseudo4op;
    bool real4op;
    double voice2_fine_tune;
    int8_t midi_velocity_offset;

    bool operator==(const ins &b) const
    {
        return notenum == b.notenum
               && insno1 == b.insno1
               && insno2 == b.insno2
               && pseudo4op == b.pseudo4op
               && real4op == b.real4op
               && equal_approx(voice2_fine_tune, b.voice2_fine_tune)
               && midi_velocity_offset == b.midi_velocity_offset;
    }
    bool operator< (const ins &b) const
    {
        if(insno1 != b.insno1) return insno1 < b.insno1;
        if(insno2 != b.insno2) return insno2 < b.insno2;
        if(notenum != b.notenum) return notenum < b.notenum;
        if(pseudo4op != b.pseudo4op) return pseudo4op < b.pseudo4op;
        if(real4op != b.real4op) return real4op < b.real4op;
        if(!equal_approx(voice2_fine_tune, b.voice2_fine_tune)) return voice2_fine_tune < b.voice2_fine_tune;
        if(midi_velocity_offset != b.midi_velocity_offset) return midi_velocity_offset < b.midi_velocity_offset;
        return 0;
    }
    bool operator!=(const ins &b) const
    {
        return !operator==(b);
    }
};

enum VolumesModels
{
    VOLUME_Generic,
    VOLUME_CMF,
    VOLUME_DMX,
    VOLUME_APOGEE,
    VOLUME_9X
};

struct AdlBankSetup
{
    int     volumeModel;
    bool    deepTremolo;
    bool    deepVibrato;
    bool    adLibPercussions;
    bool    scaleModulators;
};

typedef std::map<insdata, std::pair<size_t, std::set<std::string> > > InstrumentDataTab;
extern InstrumentDataTab insdatatab;

typedef std::map<ins, std::pair<size_t, std::set<std::string> > > InstrumentsData;
extern InstrumentsData instab;

typedef std::map<size_t, std::map<size_t, size_t> > InstProgsData;
extern InstProgsData progs;

typedef std::map<size_t, AdlBankSetup> BankSetupData;
extern BankSetupData banksetup;

extern std::vector<std::string> banknames;

//static std::map<unsigned, std::map<unsigned, unsigned> > Correlate;
//extern unsigned maxvalues[30];

void SetBank(size_t bank, unsigned patch, size_t insno);
void SetBankSetup(size_t bank, const AdlBankSetup &setup);

/* 2op voice instrument */
size_t InsertIns(const insdata &id, ins &in,
                 const std::string &name, const std::string &name2);

/* 4op voice instrument or double-voice 2-op instrument */
size_t InsertIns(const insdata &id, const insdata &id2, ins &in,
                 const std::string &name, const std::string &name2,
                 bool oneVoice = false);

size_t InsertNoSoundIns();
insdata MakeNoSoundIns();

#endif // PROGS_H
