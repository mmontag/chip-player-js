#ifndef LOAD_WOPL_H
#define LOAD_WOPL_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"
#include "common.h"

static const uint8_t wopl_latest_version = 3;

#define WOPL_INST_SIZE_V2 62
#define WOPL_INST_SIZE_V3 66

enum class WOPL_Flags
{
    Mode_2op         = 0x00,
    Mode_4op         = 0x01,
    Mode_DoubleVoice = 0x02,
};

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

    AdlBankSetup setup;
    setup.deepTremolo = (data[0x11] & 0x01) != 0;
    setup.deepVibrato = (data[0x11] & 0x02) != 0;
    setup.volumeModel = (int)data[0x12];
    setup.adLibPercussions = false;
    setup.scaleModulators  = false;

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

    size_t insSize = WOPL_INST_SIZE_V2;
    if(version >= 3)
        insSize = WOPL_INST_SIZE_V3;

    uint32_t melodic_offset = 0;
    uint32_t percussion_offset = 0;
    if(version < 2)
        melodic_offset = 0x13;
    else
        melodic_offset = 0x13 + 34 * mbanks_count + 34 * pbanks_count;

    percussion_offset = melodic_offset + (insSize * 128 * mbanks_count);

    uint32_t root_offsets[2] = {melodic_offset, percussion_offset};

    for(size_t bset = 0; bset < 2; bset++)
    {
        bool is_percussion = (bset == 1);
        for(uint32_t bankno = 0; bankno < 1; bankno++) // only first melodic bank (Until multi-banks support will be implemented)
        {
            uint32_t bank_offset = root_offsets[bset] + (bankno * insSize * 128);

            for(uint32_t i = 0; i < 128; i++)
            {
                uint32_t offset = bank_offset + uint32_t(i * insSize);
                std::string name;
                insdata tmp[2];

                name.resize(32);
                std::memcpy(&name[0], data.data() + offset, 32);
                name.resize(std::strlen(&name[0]));
    /*
        WOPL's

     0   AM/Vib/Env/Ksr/FMult characteristics
     1   Key Scale Level / Total level register data
     2   Attack / Decay
     3   Systain and Release register data
     4   Wave form

     5   AM/Vib/Env/Ksr/FMult characteristics
     6   Key Scale Level / Total level register data
     7   Attack / Decay
     8   Systain and Release register data
     9   Wave form
    */
                tmp[0].data[0]  = data[offset + 42 + 5];//AMVIB op1
                tmp[0].data[1]  = data[offset + 42 + 0];//AMVIB op2
                tmp[0].data[2]  = data[offset + 42 + 7];//AtDec op1
                tmp[0].data[3]  = data[offset + 42 + 2];//AtDec op2
                tmp[0].data[4]  = data[offset + 42 + 8];//SusRel op1
                tmp[0].data[5]  = data[offset + 42 + 3];//SusRel op2
                tmp[0].data[6]  = data[offset + 42 + 9];//Wave op1
                tmp[0].data[7]  = data[offset + 42 + 4];//Wave op2
                tmp[0].data[8]  = data[offset + 42 + 6];//KSL op1
                tmp[0].data[9]  = data[offset + 42 + 1];//KSL op2
                tmp[0].data[10] = data[offset + 40];    //FeedBack/Connection

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
                /*
                 * We will don't read two millisecond delays on tail of instrument
                 * as there are will be re-calculated by measurer here.
                 * Those fields are made for hot-loading while runtime, but not
                 * for generation of embedded banks database.
                 */

                tmp[0].finetune = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 32));
                tmp[1].finetune = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 34));
                uint8_t flags = data[offset + 39];

                struct ins tmp2;
                tmp2.notenum = is_percussion ? data[offset + 38] : 0;
                bool real4op = (flags & (uint8_t)WOPL_Flags::Mode_4op) != 0;
                tmp2.pseudo4op = (flags & (uint8_t)WOPL_Flags::Mode_DoubleVoice) != 0;
                tmp2.real4op = real4op && !tmp2.pseudo4op;
                tmp2.voice2_fine_tune = 0;
                tmp2.midi_velocity_offset = (int8_t)data[offset + 36];
                tmp[0].diff = false;
                tmp[1].diff = real4op && !tmp2.pseudo4op;

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

                uint32_t gmno = is_percussion ? i + 128 : i;

                if(is_percussion)
                {
                    int midi_index = (gmno < (128 + 35)) ? -1
                                     : (gmno < (128 + 88)) ? int(gmno) - 35
                                     : -1;
                    if(name.empty() && (midi_index >= 0))
                        name = std::string(1, '\377') + MidiInsName[midi_index];
                    if(!name.empty())
                        name.insert(0, 1, '\377');
                }
                else
                {
                    if(name.empty())
                        name = std::string(1, '\377') + MidiInsName[gmno];
                    else
                        name.insert(0, 1, '\377');
                }

                char name2[512];
                std::memset(name2, 0, 512);
                if(is_percussion)
                    snprintf(name2, 512, "%sP%u", prefix, gmno & 127);
                else
                    snprintf(name2, 512, "%sM%u", prefix, i);

                if(!real4op && !tmp2.pseudo4op)
                {
                    size_t resno = InsertIns(tmp[0], tmp2, name, name2);
                    SetBank(bank, gmno, resno);
                }
                else
                {
                    size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2);
                    SetBank(bank, gmno, resno);
                }
            }
        }
    }

    SetBankSetup(bank, setup);

    return true;
}


#endif // LOAD_WOPL_H
