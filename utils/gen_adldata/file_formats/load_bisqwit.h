#ifndef LOAD_BISQWIT_H
#define LOAD_BISQWIT_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"

static bool LoadBisqwit(const char *fn, unsigned bank, const char *prefix)
{
    #ifdef HARD_BANKS
    writeIni("Bisqwit", fn, prefix, bank, INI_Both);
    #endif
    FILE *fp = std::fopen(fn, "rb");
    if(!fp)
        return false;

    for(uint32_t a = 0; a < 256; ++a)
    {
        //unsigned offset = a * 25;
        uint32_t gmno = a;
        int32_t midi_index = gmno < 128 ? int32_t(gmno)
                         : gmno < 128 + 35 ? -1
                         : gmno < 128 + 88 ? int32_t(gmno - 35)
                         : -1;

        struct ins tmp2;
        tmp2.notenum = (uint8_t)std::fgetc(fp);
        tmp2.pseudo4op = false;
        tmp2.voice2_fine_tune = 0.0;
        tmp2.midi_velocity_offset = 0;

        insdata tmp[2];
        for(int side = 0; side < 2; ++side)
        {
            tmp[side].finetune = (int8_t)std::fgetc(fp);
            tmp[side].diff = false;
            if(std::fread(tmp[side].data, 1, 11, fp) != 11)
                return false;
        }

        std::string name;
        if(midi_index >= 0) name = std::string(1, '\377') + MidiInsName[midi_index];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        tmp[1].diff = (tmp[0] != tmp[1]);
        tmp2.real4op = tmp[1].diff;
        size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2, (tmp[0] == tmp[1]));
        SetBank(bank, gmno, resno);
    }
    std::fclose(fp);

    AdlBankSetup setup;
    setup.volumeModel = VOLUME_Generic;
    setup.deepTremolo = true;
    setup.deepVibrato = true;
    setup.adLibPercussions = false;
    setup.scaleModulators = false;
    SetBankSetup(bank, setup);

    return true;
}

#endif // LOAD_BISQWIT_H
