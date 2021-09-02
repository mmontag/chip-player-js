#ifndef LOAD_JV_H
#define LOAD_JV_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"

bool BankFormats::LoadJunglevision(BanksDump &db, const char *fn, unsigned bank, const std::string &bankTitle, const char *prefix)
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

    size_t bankDb = db.initBank(bank, bankTitle, BanksDump::BankEntry::SETUP_Win9X);
    BanksDump::MidiBank bnkMelodique;
    BanksDump::MidiBank bnkPercussion;

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

        bool isPercussion = a >= ins_count;
        size_t patchId = (a < ins_count) ? (a + first_ins) : ((a - ins_count) + first_drum);
        BanksDump::MidiBank &bnk = isPercussion ? bnkPercussion : bnkMelodique;
        BanksDump::InstrumentEntry inst;
        BanksDump::Operator ops[5];
        uint8_t notenum;

        InstBuffer tmp[2];

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
        inst.noteOffset1 = 0;

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
        inst.noteOffset2 = 0;

        notenum = data[offset + 1];

        while(notenum && notenum < 20)
        {
            notenum += 12;
            inst.noteOffset1 -= 12;
            inst.noteOffset2 -= 12;
        }

        if(data[offset] != 0)
            inst.instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_4op;
        inst.percussionKeyNumber = notenum;
        inst.setFbConn(data[offset + 7], data[offset + 7 + 11]);
        db.toOps(tmp[0].d, ops, 0);
        db.toOps(tmp[1].d, ops, 2);

        std::string name;
        if(midi_index >= 0)
            name = std::string(1, '\377') + MidiInsName[midi_index];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        db.addInstrument(bnk, patchId, inst, ops, fn);
    }

    db.addMidiBank(bankDb, false, bnkMelodique);
    db.addMidiBank(bankDb, true, bnkPercussion);

    return true;
}

#endif // LOAD_JV_H
