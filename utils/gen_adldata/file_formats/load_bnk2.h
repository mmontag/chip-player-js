#ifndef LOAD_BNK2_H
#define LOAD_BNK2_H

#include "../progs_cache.h"

inline int stdstoi(const std::string& str)
{
    return std::atoi(str.c_str());
}

static bool LoadBNK2(const char *fn, unsigned bank, const char *prefix,
                     const std::string &melo_filter,
                     const std::string &perc_filter)
{
    #ifdef HARD_BANKS
    writeIni("AdLibGold", fn, prefix, bank, INI_Both, melo_filter.c_str(), perc_filter.c_str());
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

    unsigned short ins_entries = *(unsigned short *)&data[28 + 2 + 10];
    unsigned char *records = &data[48];

    for(unsigned n = 0; n < ins_entries; ++n)
    {
        const size_t offset1 = n * 28;
        int used     =         records[offset1 + 15];
        //int attrib   = *(int*)&records[offset1 + 16];
        int offset2  = *(int *)&records[offset1 + 20];
        if(used == 0) continue;

        std::string name;
        for(unsigned p = 0; p < 12; ++p)
        {
            if(records[offset1 + 3 + p] == '\0') break;
            name += char(records[offset1 + 3 + p]);
        }

        int gmno = 0;
        if(name.substr(0, melo_filter.size()) == melo_filter)
            gmno = stdstoi(name.substr(melo_filter.size()));
        else if(name.substr(0, perc_filter.size()) == perc_filter)
            gmno = stdstoi(name.substr(perc_filter.size())) + 128;
        else
            continue;

        const unsigned char *insdata = &data[size_t(offset2)];
        const unsigned char *ops[4] = { insdata + 0, insdata + 6, insdata + 12, insdata + 18 };
        unsigned char C4xxxFFFC = insdata[24];
        unsigned char xxP24NNN = insdata[25];
        unsigned char TTTTTTTT = insdata[26];
        //unsigned char xxxxxxxx = insdata[27];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix, (gmno & 128) ? 'P' : 'M', gmno & 127);

        struct insdata tmp[2];
        for(unsigned a = 0; a < 2; ++a)
        {
            tmp[a].data[0] = ops[a * 2 + 0][0];
            tmp[a].data[1] = ops[a * 2 + 1][0];
            tmp[a].data[2] = ops[a * 2 + 0][2];
            tmp[a].data[3] = ops[a * 2 + 1][2];
            tmp[a].data[4] = ops[a * 2 + 0][3];
            tmp[a].data[5] = ops[a * 2 + 1][3];
            tmp[a].data[6] = ops[a * 2 + 0][4] & 0x07;
            tmp[a].data[7] = ops[a * 2 + 1][4] & 0x07;
            tmp[a].data[8] = ops[a * 2 + 0][1];
            tmp[a].data[9] = ops[a * 2 + 1][1];
            tmp[a].finetune = (int8_t)TTTTTTTT;
            tmp[a].diff = false;
        }
        tmp[0].data[10] = C4xxxFFFC & 0x0F;
        tmp[1].data[10] = (tmp[0].data[10] & 0x0E) | (C4xxxFFFC >> 7);

        ins tmp2;
        tmp2.notenum = (gmno & 128) ? 35 : 0;
        tmp2.pseudo4op = false;
        tmp2.real4op = false;
        tmp2.voice2_fine_tune = 0.0;
        tmp2.midi_velocity_offset = 0;

        if(xxP24NNN & 8)
        {
            // dual-op
            tmp2.real4op = true;
            tmp[1].diff = true;
            size_t resno = InsertIns(tmp[0], tmp[1], tmp2, std::string(1, '\377') + name, name2);
            SetBank(bank, (unsigned int)gmno, resno);
        }
        else
        {
            // single-op
            size_t resno = InsertIns(tmp[0], tmp2, std::string(1, '\377') + name, name2);
            SetBank(bank, (unsigned int)gmno, resno);
        }
    }

    AdlBankSetup setup;
    setup.volumeModel = VOLUME_Generic;
    setup.deepTremolo = false;
    setup.deepVibrato = false;
    setup.adLibPercussions = false;
    setup.scaleModulators = false;
    SetBankSetup(bank, setup);

    return true;
}

#endif // LOAD_BNK2_H
