#ifndef LOAD_BISQWIT_H
#define LOAD_BISQWIT_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"

bool BankFormats::LoadBisqwit(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix)
{
#ifdef HARD_BANKS
    writeIni("Bisqwit", fn, prefix, bank, INI_Both);
#endif
    FILE *fp = std::fopen(fn, "rb");
    if(!fp)
        return false;

    size_t bankDb = db.initBank(bank, bankTitle, BanksDump::BankEntry::SETUP_Generic);
    BanksDump::MidiBank bnkMelodique;
    BanksDump::MidiBank bnkPercussion;

    for(uint32_t a = 0, patchId = 0; a < 256; ++a, patchId++)
    {
        //unsigned offset = a * 25;
        uint32_t gmno = a;
        bool isPercussion = gmno >= 128;
        int32_t midi_index = gmno < 128 ? int32_t(gmno)
                         : gmno < 128 + 35 ? -1
                         : gmno < 128 + 88 ? int32_t(gmno - 35)
                         : -1;
        if(patchId == 128)
            patchId = 0;

        BanksDump::MidiBank &bnk = isPercussion ? bnkPercussion : bnkMelodique;
        BanksDump::InstrumentEntry inst;
        BanksDump::Operator ops[5];

        uint8_t notenum = static_cast<uint8_t>(std::fgetc(fp));

        InstBuffer tmp[2];
        for(int side = 0; side < 2; ++side)
        {
            std::fseek(fp, +1, SEEK_CUR); // skip first byte, unused "fine tune"
            if(std::fread(tmp[side].data, 1, 11, fp) != 11)
                return false;
        }

        std::string name;
        if(midi_index >= 0) name = std::string(1, '\377') + MidiInsName[midi_index];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        db.toOps(tmp[0].d, ops, 0);
        if(tmp[0].d != tmp[1].d)
        {
            inst.instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_4op;
            db.toOps(tmp[1].d, ops, 2);
        }

        inst.fbConn = uint_fast16_t(tmp[0].data[10]) | (uint_fast16_t(tmp[1].data[10]) << 8);
        inst.percussionKeyNumber = a >= 128 ? notenum : 0;
        inst.noteOffset1 = a < 128 ? notenum : 0;
        db.addInstrument(bnk, patchId, inst, ops, fn);
    }
    std::fclose(fp);

    db.addMidiBank(bankDb, false, bnkMelodique);
    db.addMidiBank(bankDb, true, bnkPercussion);

    return true;
}

#endif // LOAD_BISQWIT_H
