#ifndef LOAD_OP2_H
#define LOAD_OP2_H

#include "../progs_cache.h"

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
    unsigned char trem_vibr_1;    /* OP 1: tremolo/vibrato/sustain/KSR/multi */
    unsigned char att_dec_1;      /* OP 1: attack rate/decay rate */
    unsigned char sust_rel_1;     /* OP 1: sustain level/release rate */
    unsigned char wave_1;         /* OP 1: waveform select */
    unsigned char scale_1;        /* OP 1: key scale level */
    unsigned char level_1;        /* OP 1: output level */
    unsigned char feedback;       /* feedback/AM-FM (both operators) */
    unsigned char trem_vibr_2;    /* OP 2: tremolo/vibrato/sustain/KSR/multi */
    unsigned char att_dec_2;      /* OP 2: attack rate/decay rate */
    unsigned char sust_rel_2;     /* OP 2: sustain level/release rate */
    unsigned char wave_2;         /* OP 2: waveform select */
    unsigned char scale_2;        /* OP 2: key scale level */
    unsigned char level_2;        /* OP 2: output level */
    unsigned char unused;
    short         basenote;       /* base note offset */
} PACKED_STRUCT;

struct Doom_opl_instr
{
    unsigned short        flags;
#define FL_FIXED_PITCH  0x0001          // note has fixed pitch (drum note)
#define FL_UNKNOWN      0x0002          // ??? (used in instrument #65 only)
#define FL_DOUBLE_VOICE 0x0004          // use two voices instead of one

    unsigned char         finetune;
    unsigned char         note;
    struct Doom_OPL2instrument patchdata[2];
} PACKED_STRUCT;

#ifdef _MSC_VER
#pragma pack(pop)
#endif


static bool LoadDoom(const char *fn, unsigned bank, const char *prefix)
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

    for(unsigned a = 0; a < 175; ++a)
    {
        const size_t offset1 = 0x18A4 + a * 32;
        const size_t offset2 = 8      + a * 36;

        std::string name;
        for(unsigned p = 0; p < 32; ++p)
            if(data[offset1] != '\0')
                name += char(data[offset1 + p]);

        //printf("%3d %3d %3d %8s: ", a,b,c, name.c_str());
        int gmno = int(a < 128 ? a : ((a | 128) + 35));

        char name2[512];
        snprintf(name2, 512, "%s%c%u", prefix, (gmno < 128 ? 'M' : 'P'), gmno & 127);

        Doom_opl_instr &ins = *(Doom_opl_instr *) &data[offset2];

        insdata tmp[2] = {MakeNoSoundIns(), MakeNoSoundIns()};
        tmp[0].diff = false;
        tmp[1].diff = false;
        for(size_t index = 0; index < 2; ++index)
        {
            const Doom_OPL2instrument &src = ins.patchdata[index];
            tmp[index].data[0] = src.trem_vibr_1;
            tmp[index].data[1] = src.trem_vibr_2;
            tmp[index].data[2] = src.att_dec_1;
            tmp[index].data[3] = src.att_dec_2;
            tmp[index].data[4] = src.sust_rel_1;
            tmp[index].data[5] = src.sust_rel_2;
            tmp[index].data[6] = src.wave_1;
            tmp[index].data[7] = src.wave_2;
            tmp[index].data[8] = src.scale_1 | src.level_1;
            tmp[index].data[9] = src.scale_2 | src.level_2;
            tmp[index].data[10] = src.feedback;
            tmp[index].finetune = int8_t(src.basenote + 12);
        }
        struct ins tmp2;
        tmp2.notenum  = ins.note;
        tmp2.pseudo4op = false;
        tmp2.real4op = false;
        tmp2.voice2_fine_tune = 0.0;
        tmp2.midi_velocity_offset = 0;
        while(tmp2.notenum && tmp2.notenum < 20)
        {
            tmp2.notenum += 12;
            tmp[0].finetune -= 12;
            tmp[1].finetune -= 12;
        }

        if(!(ins.flags & FL_DOUBLE_VOICE))
        {
            size_t resno = InsertIns(tmp[0], tmp2, std::string(1, '\377') + name, name2);
            SetBank(bank, (unsigned int)gmno, resno);
        }
        else // Double instrument
        {
            tmp2.pseudo4op = true;
            tmp2.voice2_fine_tune = (((double)ins.finetune - 128.0) * 15.625) / 1000.0;
            if(ins.finetune == 129)
                tmp2.voice2_fine_tune = 0.000025;
            else if(ins.finetune == 127)
                tmp2.voice2_fine_tune = -0.000025;
            //printf("/*DOOM FINE TUNE (flags %000X instrument is %d) IS %d -> %lf*/\n", ins.flags, a, ins.finetune, tmp2.fine_tune);
            size_t resno = InsertIns(tmp[0], tmp[1], tmp2, std::string(1, '\377') + name, name2);
            SetBank(bank, (unsigned int)gmno, resno);
        }

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

    AdlBankSetup setup;
    setup.volumeModel = VOLUME_DMX;
    setup.deepTremolo = false;
    setup.deepVibrato = false;
    setup.adLibPercussions = false;
    setup.scaleModulators = false;
    SetBankSetup(bank, setup);

    return true;
}

#endif // LOAD_OP2_H
