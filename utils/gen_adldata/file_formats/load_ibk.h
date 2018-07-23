#ifndef LOAD_IBK_H
#define LOAD_IBK_H

#include "../progs_cache.h"

static bool LoadIBK(const char *fn, unsigned bank, const char *prefix, bool percussive)
{
    #ifdef HARD_BANKS
    writeIni("IBK", fn, prefix, bank, percussive ? INI_Drums : INI_Melodic);
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

    unsigned offs1_base = 0x804, offs1_len = 9;
    unsigned offs2_base = 0x004, offs2_len = 16;

    for(unsigned a = 0; a < 128; ++a)
    {
        unsigned offset1 = offs1_base + a * offs1_len;
        unsigned offset2 = offs2_base + a * offs2_len;

        std::string name;
        for(unsigned p = 0; p < 9; ++p)
            if(data[offset1] != '\0')
                name += char(data[offset1 + p]);

        int gmno = int(a + 128 * percussive);
        /*int midi_index = gmno < 128 ? gmno
                       : gmno < 128+35 ? -1
                       : gmno < 128+88 ? gmno-35
                       : -1;*/
        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        insdata tmp;
        tmp.data[0] = data[offset2 + 0];
        tmp.data[1] = data[offset2 + 1];
        tmp.data[8] = data[offset2 + 2];
        tmp.data[9] = data[offset2 + 3];
        tmp.data[2] = data[offset2 + 4];
        tmp.data[3] = data[offset2 + 5];
        tmp.data[4] = data[offset2 + 6];
        tmp.data[5] = data[offset2 + 7];
        tmp.data[6] = data[offset2 + 8];
        tmp.data[7] = data[offset2 + 9];
        tmp.data[10] = data[offset2 + 10];
        // [+11] seems to be used also, what is it for?
        tmp.finetune = 0;
        tmp.diff = false;
        struct ins tmp2;
        tmp2.notenum  = gmno < 128 ? 0 : 35;
        tmp2.pseudo4op = false;
        tmp2.real4op = false;
        tmp2.voice2_fine_tune = 0.0;
        tmp2.midi_velocity_offset = 0;

        size_t resno = InsertIns(tmp, tmp2, std::string(1, '\377') + name, name2);
        SetBank(bank, (unsigned int)gmno, resno);
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

#endif // LOAD_IBK_H
