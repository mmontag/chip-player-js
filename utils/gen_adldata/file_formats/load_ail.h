#ifndef LOAD_AIL_H
#define LOAD_AIL_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"
#include "common.h"

struct GTL_Head // GTL file header entry structure
{
    uint8_t  patch  = 0;
    uint8_t  bank   = 0;
    uint32_t offset = 0;
};

bool BankFormats::LoadMiles(BanksDump &db, const char *fn, unsigned bank,
                            const std::string &bankTitle, const char *prefix)
{
#ifdef HARD_BANKS
    writeIni("AIL", fn, prefix, bank, INI_Both);
#endif
    FILE *fp = std::fopen(fn, "rb");
    if(!fp)
        return false;
    std::fseek(fp, 0, SEEK_END);
    std::vector<uint8_t> data(size_t(std::ftell(fp)));
    std::rewind(fp);
    if(std::fread(&data[0], 1, data.size(), fp) != data.size())
    {
        std::fclose(fp);
        return false;
    }
    std::fclose(fp);

    GTL_Head head;
    std::vector<GTL_Head> heads;
    uint_fast8_t   max_bank_number = 0;
    heads.reserve(256);
    uint8_t *data_pos = data.data();
    uint8_t *data_end = data.data() + data.size();
    do
    {
        if((data_end - data_pos) < 6)
            return false;

        head.patch = data_pos[0];
        head.bank  = data_pos[1];
        head.offset = toUint32LE(data_pos + 2);

        if((head.patch == 0xFF) || (head.bank == 0xFF))
            break;

        if(head.patch > 127)//Patch ID is more than 127
            return false;

        if((head.bank != 0x7F) && (head.bank > max_bank_number) )
            max_bank_number = head.bank;

        heads.push_back(head);
        data_pos += 6;
    }
    while(data_pos < data_end);

    size_t bankDb = db.initBank(bank, bankTitle, BanksDump::BankEntry::SETUP_AIL);

    std::vector<BanksDump::MidiBank> bnkMelodic;
    bnkMelodic.resize(max_bank_number + 1, BanksDump::MidiBank());
    BanksDump::MidiBank              bnkPercussion;

    {
        uint8_t bank_lsb_counter = 0;
        uint8_t bank_msb_counter = 0;
        for(BanksDump::MidiBank &b : bnkMelodic)
        {
            b.lsb = bank_lsb_counter++;
            b.msb = bank_msb_counter;
            if(bank_lsb_counter == 0)
                bank_msb_counter++;
        }
    }

    uint32_t totalInsts = static_cast<uint32_t>(heads.size());
    for(uint32_t i = 0; i < totalInsts; i++)
    {
        GTL_Head &h = heads[i];
        bool isPerc = (h.bank == 0x7F);
        // int  gmPatchId = isPerc ? h.patch : (h.patch + (h.bank * 128));
        unsigned offset = h.offset;

        BanksDump::MidiBank &bnk = isPerc ? bnkPercussion : bnkMelodic[h.bank];

        int gmno = isPerc ? int(h.patch + 0x80) : int(h.patch);
//        int midi_index = gmno < 128 ? gmno
//                         : gmno < 128 + 35 ? -1
//                         : gmno < 128 + 88 ? gmno - 35
//                         : -1;

        unsigned length = data[offset] + data[offset + 1] * 256;
        signed char notenum = ((signed char)data[offset + 2]);

        BanksDump::InstrumentEntry inst;
        BanksDump::Operator ops[5];

        char name2[512];
        sprintf(name2, "%s%c%u", prefix,
                (gmno < 128 ? 'M' : 'P'), gmno & 127);

        InstBuffer tmp[200];

        const unsigned inscount = (length - 3) / 11;
//        bool twoOp = (inscount == 1);

        for(unsigned i = 0; i < inscount; ++i)
        {
            if(i >= 2)
                break;
            unsigned o = offset + 3 + i * 11;
//            tmp[i].finetune = (gmno < 128 && i == 0) ? notenum : 0;
//            tmp[i].diff = (i == 1);

            uint8_t temp[11] = {0};
            if(o < data.size())
            {
                size_t count = data.size() - o;
                count = (count > sizeof(temp)) ? sizeof(temp) : count;
                std::memcpy(temp, &data[o], count);
            }

            tmp[i].data[0] = temp[0]; // 20
            tmp[i].data[8] = temp[1]; // 40 (vol)
            tmp[i].data[2] = temp[2]; // 60
            tmp[i].data[4] = temp[3]; // 80
            tmp[i].data[6] = temp[4]; // E0
            tmp[i].data[1] = temp[6]; // 23
            tmp[i].data[9] = temp[7]; // 43 (vol)
            tmp[i].data[3] = temp[8]; // 63
            tmp[i].data[5] = temp[9]; // 83
            tmp[i].data[7] = temp[10]; // E3

            unsigned fb_c = data[offset + 3 + 5];
            tmp[i].data[10] = uint8_t(fb_c);
            if(i == 1)
            {
                inst.instFlags |= BanksDump::InstrumentEntry::WOPL_Ins_4op;
                tmp[0].data[10] = fb_c & 0x0F;
                tmp[1].data[10] = uint8_t((fb_c & 0x0E) | (fb_c >> 7));
            }
            db.toOps(tmp[i].d, ops, i * 2);
        }

        if(inscount <= 2)
        {
//            struct ins tmp2;
//            tmp2.notenum  = gmno < 128 ? 0 : (unsigned char)notenum;
//            tmp2.pseudo4op = false;
//            tmp2.real4op = (inscount > 1);
//            tmp2.voice2_fine_tune = 0.0;
//            tmp2.midi_velocity_offset = 0;
//            tmp2.rhythmModeDrum = 0;

//            std::string name;
//            if(midi_index >= 0) name = std::string(1, '\377') + MidiInsName[midi_index];
//            if(h.bank == 0 || h.bank == 0x7F)
//            {
//                size_t resno = InsertIns(tmp[0], tmp[1], tmp2, name, name2, twoOp);
//                SetBank(bank, (unsigned int)gmno, resno);
//            }
            //---------------------------------------------------------------
            inst.percussionKeyNumber = isPerc ? static_cast<uint_fast8_t>(notenum) : 0;
            inst.noteOffset1 = isPerc ? 0 : notenum;
            unsigned fb_c = data[offset + 3 + 5];
            inst.fbConn = (static_cast<uint_fast16_t>(fb_c & 0x0F)) |
                          (static_cast<uint_fast16_t>((fb_c & 0x0E) | (fb_c >> 7)) << 8);
            db.addInstrument(bnk, h.patch, inst, ops, fn);
        }
    }

    for(auto &b : bnkMelodic)
        db.addMidiBank(bankDb, false, b);
    db.addMidiBank(bankDb, true, bnkPercussion);

    return true;
}

#endif // LOAD_AIL_H
