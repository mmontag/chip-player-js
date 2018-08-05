#ifndef LOAD_TMB_H
#define LOAD_TMB_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"

static bool LoadTMB(const char *fn, unsigned bank, const char *prefix)
{
    #ifdef HARD_BANKS
    writeIni("TMB", fn, prefix, bank, INI_Both);
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

    for(unsigned a = 0; a < 256; ++a)
    {
        unsigned offset = a * 0x0D;
        unsigned gmno = a;
        int midi_index = gmno < 128 ? int(gmno)
                         : gmno < 128 + 35 ? -1
                         : gmno < 128 + 88 ? int(gmno - 35)
                         : -1;

        insdata tmp;

        tmp.data[0] = data[offset + 0];
        tmp.data[1] = data[offset + 1];
        tmp.data[2] = data[offset + 4];
        tmp.data[3] = data[offset + 5];
        tmp.data[4] = data[offset + 6];
        tmp.data[5] = data[offset + 7];
        tmp.data[6] = data[offset + 8];
        tmp.data[7] = data[offset + 9];
        tmp.data[8] = data[offset + 2];
        tmp.data[9] = data[offset + 3];
        tmp.data[10] = data[offset + 10];
        tmp.finetune = 0; //data[offset + 12];
        tmp.diff = false;

        struct ins tmp2;
        tmp2.notenum   = data[offset + 11];
        tmp2.pseudo4op = false;
        tmp2.real4op = false;
        tmp2.voice2_fine_tune = 0.0;
        tmp2.midi_velocity_offset = (int8_t)data[offset + 12];

        std::string name;
        if(midi_index >= 0) name = std::string(1, '\377') + MidiInsName[midi_index];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        size_t resno = InsertIns(tmp, tmp2, name, name2);
        SetBank(bank, gmno, resno);
    }

    AdlBankSetup setup;
    setup.volumeModel = VOLUME_APOGEE;
    setup.deepTremolo = false;
    setup.deepVibrato = false;
    setup.adLibPercussions = false;
    setup.scaleModulators = false;
    SetBankSetup(bank, setup);

    return true;
}

#endif // LOAD_TMB_H
