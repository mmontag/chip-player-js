#ifndef LOAD_OP2_H
#define LOAD_OP2_H

#include "../progs_cache.h"
#ifndef COMMON_H
#include "common.h"
#endif

#ifndef _MSC_VER
#define PACKED_STRUCT __attribute__((packed))
#else
#define PACKED_STRUCT
#endif

#ifdef _MSC_VER
#pragma pack(push,1)
#endif

struct Doom_OPL2instrument
{
    uint8_t trem_vibr_1;    /* OP 1: tremolo/vibrato/sustain/KSR/multi */
    uint8_t att_dec_1;      /* OP 1: attack rate/decay rate */
    uint8_t sust_rel_1;     /* OP 1: sustain level/release rate */
    uint8_t wave_1;         /* OP 1: waveform select */
    uint8_t scale_1;        /* OP 1: key scale level */
    uint8_t level_1;        /* OP 1: output level */
    uint8_t feedback;       /* feedback/AM-FM (both operators) */
    uint8_t trem_vibr_2;    /* OP 2: tremolo/vibrato/sustain/KSR/multi */
    uint8_t att_dec_2;      /* OP 2: attack rate/decay rate */
    uint8_t sust_rel_2;     /* OP 2: sustain level/release rate */
    uint8_t wave_2;         /* OP 2: waveform select */
    uint8_t scale_2;        /* OP 2: key scale level */
    uint8_t level_2;        /* OP 2: output level */
    uint8_t unused;
    uint8_t basenote[2];    /* base note offset */
} PACKED_STRUCT;

struct Doom_opl_instr
{
    uint8_t             flags[2];
#define FL_FIXED_PITCH  0x0001          // note has fixed pitch (drum note)
#define FL_VIB_DELAY    0x0002          // vib_delay (used in instrument #65 only)
#define FL_DOUBLE_VOICE 0x0004          // use two voices instead of one

    uint8_t             finetune;
    uint8_t             note;
    struct Doom_OPL2instrument patchdata[2];
} PACKED_STRUCT;

#ifdef _MSC_VER
#pragma pack(pop)
#endif


bool BankFormats::LoadDoom(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix)
{
#ifdef HARD_BANKS
    writeIni("OP2", fn, prefix, bank, INI_Both);
#endif
    FILE *fp = std::fopen(fn, "rb");
    if(!fp)
        return false;
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(size_t(std::ftell(fp)));
    std::rewind(fp);

    if(std::fread(&data[0], 1, data.size(), fp) != data.size())
    {
        std::fclose(fp);
        return false;
    }
    std::fclose(fp);

    size_t bankDb = db.initBank(bank, bankTitle, BanksDump::BankEntry::SETUP_DMX);
    BanksDump::MidiBank bnkMelodique;
    BanksDump::MidiBank bnkPercussion;

    for(unsigned a = 0; a < 175; ++a)
    {
        const size_t offset1 = 0x18A4 + a * 32;
        const size_t offset2 = 8      + a * 36;
        bool isDrum = a >= 128;
        BanksDump::MidiBank &bnk = isDrum ? bnkPercussion : bnkMelodique;
        BanksDump::InstrumentEntry inst;
        BanksDump::Operator ops[5];

        std::string name;
        for(unsigned p = 0; p < 32; ++p)
        {
            if(data[offset1] != '\0')
                name += char(data[offset1 + p]);
        }

        //printf("%3d %3d %3d %8s: ", a,b,c, name.c_str());
        int gmno = int(a < 128 ? a : ((a | 128) + 35));
        size_t patchId = isDrum ? ((a - 128) + 35) : a;

        char name2[512];
        snprintf(name2, 512, "%s%c%u", prefix, (gmno < 128 ? 'M' : 'P'), gmno & 127);

        Doom_opl_instr &ins = *(Doom_opl_instr *) &data[offset2];
        uint16_t flags = toSint16LE(ins.flags);

        InstBuffer tmp[2] = {MakeNoSoundIns1(), MakeNoSoundIns1()};
        int16_t noteOffset[2];

        for(size_t index = 0; index < 2; ++index)
        {
            const Doom_OPL2instrument &src = ins.patchdata[index];
            tmp[index].d.op1_amvib = src.trem_vibr_1;
            tmp[index].d.op2_amvib = src.trem_vibr_2;
            tmp[index].d.op1_atdec = src.att_dec_1;
            tmp[index].d.op2_atdec = src.att_dec_2;
            tmp[index].d.op1_susrel = src.sust_rel_1;
            tmp[index].d.op2_susrel = src.sust_rel_2;
            tmp[index].d.op1_wave = src.wave_1;
            tmp[index].d.op2_wave = src.wave_2;
            tmp[index].d.op1_ksltl = src.scale_1 | src.level_1;
            tmp[index].d.op2_ksltl = src.scale_2 | src.level_2;
            tmp[index].d.fbconn = src.feedback;
            if(isDrum)
                noteOffset[index] = 12;
            else
                noteOffset[index] = toSint16LE(src.basenote) + 12;
            inst.fbConn |= (uint_fast16_t(src.feedback) << (index == 1 ? 8 : 0));
            db.toOps(tmp[index].d, ops, index * 2);
        }

        uint8_t notenum = ins.note;
        if(isDrum)
            notenum = (flags & FL_FIXED_PITCH) ? ins.note : 60;

        while(notenum && notenum < 20)
        {
            notenum += 12;
            noteOffset[0] -= 12;
            noteOffset[1] -= 12;
        }

        inst.noteOffset1 = noteOffset[0];
        inst.noteOffset2 = noteOffset[1];

        if((flags & FL_DOUBLE_VOICE) != 0)
            inst.instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_4op | BanksDump::InstrumentEntry::WOPL_Ins_Pseudo4op;
        inst.percussionKeyNumber = notenum;
        inst.secondVoiceDetune = static_cast<char>(static_cast<int>(ins.finetune) - 128);

        db.addInstrument(bnk, patchId, inst, ops, fn);

        /*const Doom_OPL2instrument& A = ins.patchdata[0];
        const Doom_OPL2instrument& B = ins.patchdata[1];
        printf(
            "flags:%04X fine:%02X note:%02X\n"
            "{t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X f:%02X\n"
            " t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X ?:%02X base:%d}\n"
            "{t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X f:%02X\n"
            " t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X ?:%02X base:%d} ",
            ins.flags, ins.finetune, ins.note,
            A.trem_vibr_1, A.att_dec_1, A.sust_rel_1,
            A.wave_1, A.scale_1, A.level_1, A.feedback,
            A.trem_vibr_2, A.att_dec_2, A.sust_rel_2,
            A.wave_2, A.scale_2, A.level_2, A.unused, A.basenote,
            B.trem_vibr_1, B.att_dec_1, B.sust_rel_1,
            B.wave_1, B.scale_1, B.level_1, B.feedback,
            B.trem_vibr_2, B.att_dec_2, B.sust_rel_2,
            B.wave_2, B.scale_2, B.level_2, B.unused, B.basenote);
        printf(" %s VS %s\n", name.c_str(), MidiInsName[a]);
        printf("------------------------------------------------------------\n");*/
    }

    db.addMidiBank(bankDb, false, bnkMelodique);
    db.addMidiBank(bankDb, true, bnkPercussion);

    return true;
}

#endif // LOAD_OP2_H
