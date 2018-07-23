#ifndef LOAD_JV_H
#define LOAD_JV_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"

static bool LoadJunglevision(const char *fn, unsigned bank, const char *prefix)
{
    #ifdef HARD_BANKS
    writeIni("Junglevision", fn, prefix, bank, INI_Both);
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

    uint16_t ins_count  = uint16_t(data[0x20] + (data[0x21] << 8));
    uint16_t drum_count = uint16_t(data[0x22] + (data[0x23] << 8));
    uint16_t first_ins  = uint16_t(data[0x24] + (data[0x25] << 8));
    uint16_t first_drum = uint16_t(data[0x26] + (data[0x27] << 8));

    unsigned total_ins = ins_count + drum_count;

    for(unsigned a = 0; a < total_ins; ++a)
    {
        unsigned offset = 0x28 + a * 0x18;
        unsigned gmno = (a < ins_count) ? (a + first_ins) : (a + first_drum);
        int midi_index = gmno < 128 ? int(gmno)
                         : gmno < 128 + 35 ? -1
                         : gmno < 128 + 88 ? int(gmno - 35)
                         : -1;

        insdata tmp[2];

        tmp[0].data[0] = data[offset + 2];
        tmp[0].data[1] = data[offset + 8];
        tmp[0].data[2] = data[offset + 4];
        tmp[0].data[3] = data[offset + 10];
        tmp[0].data[4] = data[offset + 5];
        tmp[0].data[5] = data[offset + 11];
        tmp[0].data[6] = data[offset + 6];
        tmp[0].data[7] = data[offset + 12];
        tmp[0].data[8] = data[offset + 3];
        tmp[0].data[9] = data[offset + 9];
        tmp[0].data[10] = data[offset + 7] & 0x0F;//~0x30;
        tmp[0].finetune = 0;
        tmp[0].diff = false;

        tmp[1].data[0] = data[offset + 2 + 11];
        tmp[1].data[1] = data[offset + 8 + 11];
        tmp[1].data[2] = data[offset + 4 + 11];
        tmp[1].data[3] = data[offset + 10 + 11];
        tmp[1].data[4] = data[offset + 5 + 11];
        tmp[1].data[5] = data[offset + 11 + 11];
        tmp[1].data[6] = data[offset + 6 + 11];
        tmp[1].data[7] = data[offset + 12 + 11];
        tmp[1].data[8] = data[offset + 3 + 11];
        tmp[1].data[9] = data[offset + 9 + 11];
        tmp[1].data[10] = data[offset + 7 + 11] & 0x0F;//~0x30;
        tmp[1].finetune = 0;
        tmp[1].diff = (data[offset] != 0);

        struct ins tmp2;
        tmp2.notenum  = data[offset + 1];
        tmp2.pseudo4op = false;
        tmp2.real4op = (data[offset] != 0);
        tmp2.voice2_fine_tune = 0.0;
        tmp2.midi_velocity_offset = 0;

        while(tmp2.notenum && tmp2.notenum < 20)
        {
            tmp2.notenum += 12;
            tmp[0].finetune -= 12;
            tmp[1].finetune -= 12;
        }

        std::string name;
        if(midi_index >= 0) name = std::string(1, '\377') + MidiInsName[midi_index];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        if(!data[offset])
        {
            size_t resno = InsertIns(tmp[0], tmp2, name, name2);
            SetBank(bank, gmno, resno);
        }
        else // Double instrument
        {
            size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2);
            SetBank(bank, gmno, resno);
        }
    }

    AdlBankSetup setup;
    setup.volumeModel = VOLUME_9X;
    setup.deepTremolo = true;
    setup.deepVibrato = true;
    setup.adLibPercussions = false;
    setup.scaleModulators = false;
    SetBankSetup(bank, setup);

    return true;
}

#endif // LOAD_JV_H

