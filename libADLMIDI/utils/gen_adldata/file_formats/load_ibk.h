#ifndef LOAD_IBK_H
#define LOAD_IBK_H

#include "../progs_cache.h"

bool BankFormats::LoadIBK(BanksDump &db, const char *fn, unsigned bank,
                          const std::string &bankTitle, const char *prefix,
                          bool percussive, bool noRhythmMode)
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

    size_t bankDb = db.initBank(bank, bankTitle, BanksDump::BankEntry::SETUP_Generic);
    BanksDump::MidiBank bnk;

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

        BanksDump::InstrumentEntry inst;
        BanksDump::Operator ops[5];

        InstBuffer tmp;
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
        // bisqwit: [+11] seems to be used also, what is it for?
        // Wohlstand: You wanna know? It's the rhythm-mode drum number! If 0 - melodic, >0 - rhythm-mode drum

        db.toOps(tmp.d, ops, 0);
        inst.noteOffset1 = percussive ? 0 : data[offset2 + 12];
        inst.percussionKeyNumber = percussive ? data[offset2 + 13] : 0;
        inst.setFbConn(data[offset2 + 10]);

        if(percussive && !noRhythmMode)
        {
            int rm = data[offset2 + 11];
            switch(rm)
            {
            case 6:
                inst.instFlags |= BanksDump::InstrumentEntry::WOPL_RM_BassDrum;
                break;
            case 7:
                inst.instFlags |= BanksDump::InstrumentEntry::WOPL_RM_Snare;
                break;
            case 8:
                inst.instFlags |= BanksDump::InstrumentEntry::WOPL_RM_TomTom;
                break;
            case 9:
                inst.instFlags |= BanksDump::InstrumentEntry::WOPL_RM_Cymbal;
                break;
            case 10:
                inst.instFlags |= BanksDump::InstrumentEntry::WOPL_RM_HiHat;
                break;
            default:
                // IBK logic: make non-percussion instrument be silent
                inst.instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_IsBlank;
                break;
            }
        }

        db.addInstrument(bnk, a, inst, ops, fn);
    }

    db.addMidiBank(bankDb, percussive, bnk);

    return true;
}

#endif // LOAD_IBK_H
