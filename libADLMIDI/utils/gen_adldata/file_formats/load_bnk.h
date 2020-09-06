#ifndef LOAD_BNK_H
#define LOAD_BNK_H

#include "../progs_cache.h"

#include <cstdio>
#include <cstdint>
#include <string>

bool BankFormats::LoadBNK(BanksDump &db, const char *fn, unsigned bank,
                          const std::string &bankTitle, const char *prefix,
                          bool is_fat, bool percussive)
{
#ifdef HARD_BANKS
    writeIni("HMI", fn, prefix, bank, percussive ? INI_Drums : INI_Melodic);
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

    size_t bankDb = db.initBank(bank, bankTitle, BanksDump::BankEntry::SETUP_HMI);
    BanksDump::MidiBank bnk;

    /*printf("%s:\n", fn);*/
    //unsigned short version = *(short*)&data[0]; // major,minor (2 bytes)
    //                                             "ADLIB-"    (6 bytes)
    uint16_t        entries_used = *(uint16_t *)&data[8];  // entries used
    //unsigned short total_entries = *(short*)&data[10]; // total entries
    unsigned        name_offset = *(unsigned *)&data[12]; // name_offset
    unsigned        data_offset = *(unsigned *)&data[16]; // data_offset
    // 16..23: 8 byte sof filler
    /*printf("version=%u %u %u %u %u\n",
        version, entries_used,total_entries,name_offset,data_offset);*/

    for(unsigned n = 0; n < entries_used; ++n)
    {
        const size_t offset1 = name_offset + n * 12;

        unsigned short data_index = data[offset1 + 0] + data[offset1 + 1] * 256;
        unsigned char usage_flag = data[offset1 + 2];
        std::string name;
        for(unsigned p = 0; p < 9; ++p)
        {
            if(data[offset1 + 3 + p] == '\0') break;
            name += char(data[offset1 + 3 + p]);
        }

        const size_t offset2 = data_offset + data_index * 30;
        //const unsigned char mode      = data[offset2+0];
        const unsigned char voice_num = data[offset2 + 1];
        const unsigned char *op1 = &data[offset2 + 2]; // 13 bytes
        const unsigned char *op2 = &data[offset2 + 15];
        const unsigned char waveform_mod = data[offset2 + 28];
        const unsigned char waveform_car = data[offset2 + 29];

        /*printf("%5d %3d %8s mode=%02X voice=%02X: ", data_index,usage_flag, name.c_str(),
            mode,voice_num);*/

        int gmno = int(is_fat
                   ? ((n & 127) + percussive * 128)
                   : (n + percussive * 128));

        if(is_fat && percussive)
        {
            if(name[2] == 'O'
               || name[2] == 'S')
            {
                std::string n = name.substr(3);
                gmno = 128 + std::atoi(n.c_str());
            }
        }

        char name2[512];
        if(is_fat)
            sprintf(name2, "%s%c%u", prefix, percussive ? 'P' : 'M', gmno & 127);
        else
            sprintf(name2, "%s%u", prefix, n);

        BanksDump::InstrumentEntry inst;
        BanksDump::Operator ops[5];

        InstBuffer tmp;
        tmp.data[0] = uint8_t(
                      (op1[ 9] << 7) // TREMOLO FLAG
                      + (op1[10] << 6) // VIBRATO FLAG
                      + (op1[ 5] << 5) // SUSTAIN FLAG
                      + (op1[11] << 4) // SCALING FLAG
                      + op1[ 1]);      // FREQMUL

        tmp.data[1] = uint8_t((op2[ 9] << 7)
                      + (op2[10] << 6)
                      + (op2[ 5] << 5)
                      + (op2[11] << 4)
                      + op2[ 1]);
        tmp.data[2] = op1[3] * 0x10 + op1[6]; // ATTACK, DECAY
        tmp.data[3] = op2[3] * 0x10 + op2[6];
        tmp.data[4] = op1[4] * 0x10 + op1[7]; // SUSTAIN, RELEASE
        tmp.data[5] = op2[4] * 0x10 + op2[7];
        tmp.data[6] = waveform_mod;
        tmp.data[7] = waveform_car;
        tmp.data[8] = op1[0] * 0x40 + op1[8]; // KSL , LEVEL
        tmp.data[9] = op2[0] * 0x40 + op2[8]; // KSL , LEVEL
        tmp.data[10] = op1[2] * 2 + op1[12]; // FEEDBACK, ADDITIVEFLAG

        // Note: op2[2] and op2[12] are unused and contain garbage.

        if(is_fat)
            tmp.data[10] ^= 1;

        db.toOps(tmp.d, ops, 0);
        inst.percussionKeyNumber = is_fat ? voice_num : (percussive ? usage_flag : 0);
        inst.setFbConn(op1[2] * 2 + op1[12]);

        if(!is_fat)
            db.addInstrument(bnk, n & 127, inst, ops, fn);

        /*
        for(unsigned p=0; p<30; ++p)
        {
            unsigned char value = data[offset2+p];
            if(value > maxvalues[p]) maxvalues[p] = value;

            #define dot(maxval) if(value<=maxval) printf("."); else printf("?[%u]%X",p,value);

          {
            //if(p==6 || p==7 || p==19||p==20) value=15-value;

            if(p==4 || p==10 || p==17 || p==23)// || p==25)
                printf(" %2X", value);
            else
                printf(" %X", value);
         }
        nl:;
            //if(p == 12) printf("\n%*s", 22, "");
            //if(p == 25) printf("\n%*s", 22, "");
        }
        printf("\n");
        */
    }

    db.addMidiBank(bankDb, percussive, bnk);

    return true;
}

#endif // LOAD_BNK_H
