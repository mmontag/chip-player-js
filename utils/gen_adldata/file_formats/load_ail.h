#ifndef LOAD_AIL_H
#define LOAD_AIL_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"

static bool LoadMiles(const char *fn, unsigned bank, const char *prefix)
{
    #ifdef HARD_BANKS
    writeIni("AIL", fn, prefix, bank, INI_Both);
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

    for(unsigned a = 0; a < 2000; ++a)
    {
        unsigned gm_patch  = data[a * 6 + 0];
        unsigned gm_bank = data[a * 6 + 1];
        unsigned offset    = *(unsigned *)&data[a * 6 + 2];

        if(gm_patch == 0xFF)
            break;

        int gmno = gm_bank == 0x7F ? int(gm_patch + 0x80) : int(gm_patch);
        int midi_index = gmno < 128 ? gmno
                         : gmno < 128 + 35 ? -1
                         : gmno < 128 + 88 ? gmno - 35
                         : -1;
        unsigned length = data[offset] + data[offset + 1] * 256;
        signed char notenum = ((signed char)data[offset + 2]);

        /*printf("%02X %02X %08X ", gmnumber,gmnumber2, offset);
        for(unsigned b=0; b<length; ++b)
        {
            if(b > 3 && (b-3)%11 == 0) printf("\n                        ");
            printf("%02X ", data[offset+b]);
        }
        printf("\n");*/

        if(gm_bank != 0 && gm_bank != 0x7F)
            continue;

        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        insdata tmp[200];

        const unsigned inscount = (length - 3) / 11;
        bool twoOp = (inscount == 1);

        for(unsigned i = 0; i < inscount; ++i)
        {
            unsigned o = offset + 3 + i * 11;
            tmp[i].finetune = (gmno < 128 && i == 0) ? notenum : 0;
            tmp[i].diff = (i == 1);
            tmp[i].data[0] = data[o + 0]; // 20
            tmp[i].data[8] = data[o + 1]; // 40 (vol)
            tmp[i].data[2] = data[o + 2]; // 60
            tmp[i].data[4] = data[o + 3]; // 80
            tmp[i].data[6] = data[o + 4]; // E0
            tmp[i].data[1] = data[o + 6]; // 23
            tmp[i].data[9] = data[o + 7]; // 43 (vol)
            tmp[i].data[3] = data[o + 8]; // 63
            tmp[i].data[5] = data[o + 9]; // 83
            tmp[i].data[7] = data[o + 10]; // E3

            unsigned fb_c = data[offset + 3 + 5];
            tmp[i].data[10] = uint8_t(fb_c);
            if(i == 1)
            {
                tmp[0].data[10] = fb_c & 0x0F;
                tmp[1].data[10] = uint8_t((fb_c & 0x0E) | (fb_c >> 7));
            }
        }

        if(inscount <= 2)
        {
            struct ins tmp2;
            tmp2.notenum  = gmno < 128 ? 0 : (unsigned char)notenum;
            tmp2.pseudo4op = false;
            tmp2.real4op = (inscount > 1);
            tmp2.voice2_fine_tune = 0.0;
            tmp2.midi_velocity_offset = 0;
            std::string name;
            if(midi_index >= 0) name = std::string(1, '\377') + MidiInsName[midi_index];
            size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2, twoOp);
            SetBank(bank, (unsigned int)gmno, resno);
        }
    }

    AdlBankSetup setup;
    setup.volumeModel = VOLUME_Generic;
    setup.deepTremolo = true;
    setup.deepVibrato = true;
    setup.adLibPercussions = false;
    setup.scaleModulators = false;
    SetBankSetup(bank, setup);

    return true;
}

#endif // LOAD_AIL_H
