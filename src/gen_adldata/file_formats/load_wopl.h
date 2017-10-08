#ifndef LOAD_WOPL_H
#define LOAD_WOPL_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"
#include "common.h"

static uint8_t wopl_latest_version = 2;

static bool LoadWopl(const char *fn, unsigned bank, const char *prefix)
{
    FILE *fp = std::fopen(fn, "rb");
    if(!fp)
    {
        std::fprintf(stderr, "WOPL: CAN'T OPEN FILE %s\n", fn);
        std::fflush(stderr);
        return false;
    }
    std::fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(size_t(std::ftell(fp)));
    std::rewind(fp);
    if(std::fread(&data[0], 1, data.size(), fp) != data.size())
    {
        std::fclose(fp);
        std::fprintf(stderr, "WOPL: CAN'T READ FILE %s\n", fn);
        std::fflush(stderr);
        return false;
    }
    std::fclose(fp);

    if(data.size() < 19) // Smaller than header size
    {
        std::fprintf(stderr, "WOPL: Too small header %s\n", fn);
        std::fflush(stderr);
        return false;
    }

    uint16_t version = toUint16LE((const uint8_t *)data.data() + 11);
    if(version > wopl_latest_version)
    {
        std::fprintf(stderr, "WOPL: Version %d is not supported (latest %d) %s\n", version, wopl_latest_version, fn);
        std::fflush(stderr);
        return false;
    }

    uint16_t mbanks_count = toUint16BE((const uint8_t *)data.data() + 0x0d);
    uint16_t pbanks_count = toUint16BE((const uint8_t *)data.data() + 0x0f);

    // Validate file format by size calculation
    if(version == 1)
    {
        //Header size + melodic banks + percussion banks
        if(data.size() < size_t(19 + (62 * 128 * mbanks_count) + (62 * 128 * pbanks_count)))
        {
            std::fprintf(stderr, "WOPL: Version 1 size calculation failed %s\n", fn);
            std::fflush(stderr);
            return false;
        }
    }
    else if(version >= 2)
    {
        //Header size + melodic bank meta data + percussion bank meta data  + melodic banks + percussion banks
        if(data.size() < size_t(19 + (34 * mbanks_count) + (34 * pbanks_count) + (62 * 128 * mbanks_count) + (62 * 128 * pbanks_count)))
        {
            std::fprintf(stderr, "WOPL: Version %d size calculation failed %s\n", version, fn);
            std::fflush(stderr);
            return false;
        }
    }

    uint32_t melodic_offset = 0;
    uint32_t percussion_offset = 0;
    if(version < 2)
        melodic_offset = 0x13;
    else
        melodic_offset = 0x13 + 34 * mbanks_count + 34 * pbanks_count;

    percussion_offset = melodic_offset + (62 * 128 * mbanks_count);

    for(uint32_t mbank = 0; mbank < 1; mbank++) // only first melodic bank (Until multi-banks support will be implemented)
    {
        uint32_t bank_offset = melodic_offset + (mbank * 62 * 128);

        for(unsigned i = 0; i < 128; i++)
        {
            uint32_t offset = bank_offset + uint32_t(i * 62);
            std::string name;
            insdata tmp[2];

            name.resize(32);
            std::memcpy(&name[0], data.data() + offset, 32);
            name.resize(std::strlen(&name[0]));

            tmp[0].data[0]  = data[offset + 42 + 5];
            tmp[0].data[1]  = data[offset + 42 + 0];
            tmp[0].data[2]  = data[offset + 42 + 7];
            tmp[0].data[3]  = data[offset + 42 + 2];
            tmp[0].data[4]  = data[offset + 42 + 8];
            tmp[0].data[5]  = data[offset + 42 + 3];
            tmp[0].data[6]  = data[offset + 42 + 9];
            tmp[0].data[7]  = data[offset + 42 + 4];
            tmp[0].data[8]  = data[offset + 42 + 6];
            tmp[0].data[9]  = data[offset + 42 + 1];
            tmp[0].data[10] = data[offset + 40];

            tmp[1].data[0]  = data[offset + 52 + 5];
            tmp[1].data[1]  = data[offset + 52 + 0];
            tmp[1].data[2]  = data[offset + 52 + 7];
            tmp[1].data[3]  = data[offset + 52 + 2];
            tmp[1].data[4]  = data[offset + 52 + 8];
            tmp[1].data[5]  = data[offset + 52 + 3];
            tmp[1].data[6]  = data[offset + 52 + 9];
            tmp[1].data[7]  = data[offset + 52 + 4];
            tmp[1].data[8]  = data[offset + 52 + 6];
            tmp[1].data[9]  = data[offset + 52 + 1];
            tmp[1].data[10] = data[offset + 41];

            tmp[0].finetune = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 32));
            tmp[1].finetune = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 34));

            uint8_t flags = data[offset + 39];

            struct ins tmp2;
            tmp2.notenum = 0;
            tmp2.pseudo4op = (flags & 0x02) != 0;
            tmp2.voice2_fine_tune = 0;

            int8_t fine_tune = (int8_t)data[offset + 37];
            if(fine_tune != 0)
            {
                if(fine_tune == 1)
                    tmp2.voice2_fine_tune = 0.000025;
                else if(fine_tune == -1)
                    tmp2.voice2_fine_tune = -0.000025;
                else
                    tmp2.voice2_fine_tune = ((fine_tune * 15.625) / 1000.0);
            }

            if(name.empty())
                name = std::string(1, '\377') + MidiInsName[i];
            else
                name.insert(0, 1, '\377');

            char name2[512];
            sprintf(name2, "%sM%u", prefix, i);

            if(!flags)
            {
                size_t resno = InsertIns(tmp[0], tmp[0], tmp2, name, name2);
                SetBank(bank, i, resno);
            }
            else
            {
                size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2);
                SetBank(bank, i, resno);
            }
        }
    }

    for(uint32_t pbank = 0; pbank < 1; pbank++) // only first percussion bank (Until multi-banks support will be implemented)
    {
        uint32_t bank_offset = percussion_offset + (pbank * 62 * 128);

        for(uint32_t i = 0; i < 128; i++)
        {
            uint32_t offset = bank_offset + (i * 62);
            std::string name;
            insdata tmp[2];

            name.resize(32);
            std::memcpy(&name[0], data.data() + offset, 32);
            name.resize(std::strlen(&name[0]));

            tmp[0].data[0]  = data[offset + 42 + 5];
            tmp[0].data[1]  = data[offset + 42 + 0];
            tmp[0].data[2]  = data[offset + 42 + 7];
            tmp[0].data[3]  = data[offset + 42 + 2];
            tmp[0].data[4]  = data[offset + 42 + 8];
            tmp[0].data[5]  = data[offset + 42 + 3];
            tmp[0].data[6]  = data[offset + 42 + 9];
            tmp[0].data[7]  = data[offset + 42 + 4];
            tmp[0].data[8]  = data[offset + 42 + 6];
            tmp[0].data[9]  = data[offset + 42 + 1];
            tmp[0].data[10] = data[offset + 40];

            tmp[1].data[0]  = data[offset + 52 + 5];
            tmp[1].data[1]  = data[offset + 52 + 0];
            tmp[1].data[2]  = data[offset + 52 + 7];
            tmp[1].data[3]  = data[offset + 52 + 2];
            tmp[1].data[4]  = data[offset + 52 + 8];
            tmp[1].data[5]  = data[offset + 52 + 3];
            tmp[1].data[6]  = data[offset + 52 + 9];
            tmp[1].data[7]  = data[offset + 52 + 4];
            tmp[1].data[8]  = data[offset + 52 + 6];
            tmp[1].data[9]  = data[offset + 52 + 1];
            tmp[1].data[10] = data[offset + 41];

            tmp[0].finetune = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 32));
            tmp[1].finetune = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 34));

            uint8_t flags = data[offset + 39];

            struct ins tmp2;
            tmp2.notenum = data[offset + 38];
            tmp2.pseudo4op = (flags & 0x02) != 0;
            tmp2.voice2_fine_tune = 0;

            int8_t fine_tune = (int8_t)data[offset + 37];
            if(fine_tune != 0)
            {
                if(fine_tune == 1)
                    tmp2.voice2_fine_tune = 0.000025;
                else if(fine_tune == -1)
                    tmp2.voice2_fine_tune = -0.000025;
                else
                    tmp2.voice2_fine_tune = ((fine_tune * 15.625) / 1000.0);
            }

            uint32_t gmno = i + 128;
            int midi_index = (gmno < (128 + 35)) ? -1
                             : (gmno < (128 + 88)) ? int(gmno) - 35
                             : -1;

            if(name.empty() && (midi_index >= 0))
                name = std::string(1, '\377') + MidiInsName[midi_index];
            if(!name.empty())
                name.insert(0, 1, '\377');

            char name2[512];
            sprintf(name2, "%sP%u", prefix, gmno & 127);

            if(!flags)
            {
                size_t resno = InsertIns(tmp[0], tmp[0], tmp2, name, name2);
                SetBank(bank, gmno, resno);
            }
            else
            {
                size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2);
                SetBank(bank, gmno, resno);
            }
        }
    }
    return true;
}


#endif // LOAD_WOPL_H
