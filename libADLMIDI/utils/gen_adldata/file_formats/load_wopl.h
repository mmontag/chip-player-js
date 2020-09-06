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
    WOPL_RhythmModeMask = 0x38,
};

bool BankFormats::LoadWopl(BanksDump &db, const char *fn, unsigned bank, const std::string bankTitle, const char *prefix)
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

    size_t bankDb = db.initBank(bank, bankTitle, static_cast<uint_fast16_t>((static_cast<unsigned>(data[0x11]) << 8) | static_cast<unsigned>(data[0x12])));

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
    uint32_t melodic_meta_offset = 0;
    uint32_t percussion_meta_offset = 0;

    if(version < 2)
    {
        melodic_offset = 0x13;
        melodic_meta_offset = 0;
    }
    else
    {
        melodic_offset = 0x13 + 34 * mbanks_count + 34 * pbanks_count;
        melodic_meta_offset = 0x13;
        percussion_meta_offset = 0x13 + 34 * mbanks_count;
    }

    percussion_offset = melodic_offset + (insSize * 128 * mbanks_count);

    uint32_t root_sizes[2]  =   {mbanks_count, pbanks_count};
    uint32_t root_offsets[2] =  {melodic_offset, percussion_offset};
    uint32_t root_meta_offsets[2] =  {melodic_meta_offset, percussion_meta_offset};

    for(size_t bset = 0; bset < 2; bset++)
    {
        bool is_percussion = (bset == 1);
        for(uint32_t bankno = 0; bankno < root_sizes[bset]; bankno++) // only first melodic bank (Until multi-banks support will be implemented)
        {
            uint32_t bank_offset = root_offsets[bset] + (bankno * insSize * 128);

            BanksDump::MidiBank bnk;
            if(version >= 2)
            {
                uint32_t meta_offset = root_meta_offsets[bset] + (bankno * 34);
                bnk.lsb = data[meta_offset + 32 + 0];
                bnk.msb = data[meta_offset + 32 + 1];
            }

            for(uint32_t i = 0; i < 128; i++)
            {
                uint32_t offset = bank_offset + uint32_t(i * insSize);
                std::string name;
                InstBuffer tmp[2];

                BanksDump::InstrumentEntry inst;
                BanksDump::Operator ops[5];

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
                tmp[0].d.op1_amvib  = data[offset + 42 + 5];//AMVIB op1
                tmp[0].d.op2_amvib  = data[offset + 42 + 0];//AMVIB op2
                tmp[0].d.op1_atdec  = data[offset + 42 + 7];//AtDec op1
                tmp[0].d.op2_atdec  = data[offset + 42 + 2];//AtDec op2
                tmp[0].d.op1_susrel = data[offset + 42 + 8];//SusRel op1
                tmp[0].d.op2_susrel = data[offset + 42 + 3];//SusRel op2
                tmp[0].d.op1_wave   = data[offset + 42 + 9];//Wave op1
                tmp[0].d.op2_wave   = data[offset + 42 + 4];//Wave op2
                tmp[0].d.op1_ksltl  = data[offset + 42 + 6];//KSL op1
                tmp[0].d.op2_ksltl  = data[offset + 42 + 1];//KSL op2
                tmp[0].d.fbconn     = data[offset + 40];    //FeedBack/Connection

                tmp[1].d.op1_amvib  = data[offset + 52 + 5];
                tmp[1].d.op2_amvib  = data[offset + 52 + 0];
                tmp[1].d.op1_atdec  = data[offset + 52 + 7];
                tmp[1].d.op2_atdec  = data[offset + 52 + 2];
                tmp[1].d.op1_susrel = data[offset + 52 + 8];
                tmp[1].d.op2_susrel = data[offset + 52 + 3];
                tmp[1].d.op1_wave   = data[offset + 52 + 9];
                tmp[1].d.op2_wave   = data[offset + 52 + 4];
                tmp[1].d.op1_ksltl  = data[offset + 52 + 6];
                tmp[1].d.op2_ksltl  = data[offset + 52 + 1];
                tmp[1].d.fbconn     = data[offset + 41];
                /*
                 * We will don't read two millisecond delays on tail of instrument
                 * as there are will be re-calculated by measurer here.
                 * Those fields are made for hot-loading while runtime, but not
                 * for generation of embedded banks database.
                 */
                db.toOps(tmp[0].d, ops, 0);
                db.toOps(tmp[1].d, ops, 2);

                uint8_t flags = data[offset + 39];

                //----------------
                inst.instFlags = flags;
                inst.percussionKeyNumber = is_percussion ? data[offset + 38] : 0;
                inst.noteOffset1 = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 32));
                inst.noteOffset2 = int8_t(toSint16BE((const uint8_t *)data.data() + offset + 34));
                inst.secondVoiceDetune = static_cast<int_fast8_t>(data[offset + 37]);
                inst.midiVelocityOffset = static_cast<int_fast8_t>(data[offset + 36]);
                inst.fbConn = (static_cast<uint_fast16_t>(data[offset + 40])) |
                              (static_cast<uint_fast16_t>(data[offset + 41]) << 8);
                if(version >= 2)
                {
                    inst.delay_on_ms = toUint16BE((const uint8_t *)data.data() + offset + 62);
                    inst.delay_off_ms = toUint16BE((const uint8_t *)data.data() + offset + 64);
                }
                //----------------

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

                db.addInstrument(bnk, i, inst, ops, fn);
            }
            db.addMidiBank(bankDb, is_percussion, bnk);
        }
    }

    return true;
}


#endif // LOAD_WOPL_H
