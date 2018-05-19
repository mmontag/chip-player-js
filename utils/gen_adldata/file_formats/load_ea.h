#ifndef LOAD_EA_H
#define LOAD_EA_H

#include "../progs_cache.h"
#include "../midi_inst_list.h"

static bool LoadEA(const char *fn, unsigned bank, const char *prefix)
{
    FILE *fp = std::fopen(fn, "rb");
    if(!fp)
        return false;

    // Copy all instruments from bank 0
    for(unsigned gmno = 0; gmno < 128; ++gmno)
        progs[bank][gmno] = progs[0][gmno];
    for(unsigned gmno = 35; gmno < 80; ++gmno)
        progs[bank][0x80 + gmno] = progs[0][0x80 + gmno];

    uint16_t sources[20 + 8];
    // Copy also the unused instruments
    sources[20] = 0x245;
    sources[21] = 0x24F;
    sources[22] = 0x263;
    sources[23] = 0x277;
    sources[24] = 0x281;
    sources[25] = 0x28B;
    sources[26] = 0x29F;
    sources[27] = 0x2A9;

    for(unsigned gmno = 0; gmno < 20; ++gmno)
    {
        std::fseek(fp, 0x150 + gmno, SEEK_SET);
        uint8_t bytes[3] = {0};
        if(std::fread(bytes, 1, 1, fp) != 1)
        {
            std::fclose(fp);
            return false;
        }

        long insno = (long)bytes[0];
        std::fseek(fp, 0x187 + insno * 2, SEEK_SET);
        if(std::fread(bytes, 1, 2, fp) != 2)
        {
            std::fclose(fp);
            return false;
        }

        uint16_t offset = (uint16_t)bytes[0];
        offset += bytes[1] * 256;
        sources[gmno] = offset;
    }

    for(unsigned gmno = 0; gmno < 20 + 8; ++gmno)
    {
        unsigned int offset = sources[gmno];
        std::fseek(fp, (long)offset, SEEK_SET);
        uint8_t bytes[10];
        if(std::fread(bytes, 1, 10, fp) != 10)
        {
            std::fclose(fp);
            return false;
        }

        insdata tmp;
        tmp.data[0] = bytes[0]; // reg 0x20: modulator AM/VIG/EG/KSR
        tmp.data[8] = bytes[1]; // reg 0x40: modulator ksl/attenuation
        tmp.data[2] = bytes[2]; // reg 0x60: modulator attack/decay
        tmp.data[4] = bytes[3]; // reg 0x80: modulator sustain/release

        tmp.data[1] = bytes[4]; // reg 0x20: carrier AM/VIG/EG/KSR
        tmp.data[9] = bytes[5]; // reg 0x40: carrier   ksl/attenuation
        tmp.data[3] = bytes[6]; // reg 0x60: carrier attack/decay
        tmp.data[5] = bytes[7]; // reg 0x80: carrier sustain/release

        // bytes[1] will be written directly to register 0x40
        // bytes[5] will be written directly to register 0x43
        // When touching volume, register 0x43 <- bytes[5] - midivolume/4

        tmp.data[10] = bytes[8]; // reg 0xC0 (feedback and connection)

        tmp.finetune = int8_t(bytes[9] + 12); // finetune
        tmp.data[6] = 0;        // reg 0xE0: modulator, never seems to be set
        tmp.data[7] = 0;        // reg 0xE0: carrier,   never seems to be set

        ins tmp2{};
        tmp2.notenum   = 0;
        tmp2.pseudo4op = false;
        tmp2.real4op = false;

        std::string name;
        char name2[512];
        if(gmno < 20)
        {
            snprintf(name2, 512, "%sM%u", prefix, gmno);
        }
        else
        {
            snprintf(name2, 512, "%sunk%04X", prefix, offset);
        }
        size_t resno = InsertIns(tmp, tmp2, std::string(1, '\377') + name, name2);
        SetBank(bank, gmno, resno);

        if(gmno == 10)
        {
            /*tmp.finetune=0;*/ tmp2.notenum = 0x49;
            SetBank(bank, 0x80 + 0x36, InsertIns(tmp, tmp2, std::string(1, '\377') + MidiInsName[0x80 + 0x36 - 35], std::string(1, '\377') + prefix + "P54"));
        }

        if(gmno == 18)
        {
            /*tmp.finetune=0;*/ tmp2.notenum = 0x17;
            SetBank(bank, 0x80 + 0x2A, InsertIns(tmp, tmp2, std::string(1, '\377') + MidiInsName[0x80 + 0x2A - 35], std::string(1, '\377') + prefix + "P42"));
        }

        if(gmno == 16)
        {
            /*tmp.finetune=0;*/ tmp2.notenum = 0x0C;
            SetBank(bank, 0x80 + 0x24, InsertIns(tmp, tmp2, std::string(1, '\377') + MidiInsName[0x80 + 0x24 - 35], std::string(1, '\377') + prefix + "P36"));
        }

        if(gmno == 17)
        {
            /*tmp.finetune=0;*/ tmp2.notenum = 0x01;
            SetBank(bank, 0x80 + 0x26, InsertIns(tmp, tmp2, std::string(1, '\377') + MidiInsName[0x80 + 0x26 - 35], std::string(1, '\377') + prefix + "P38"));
        }
    }

    std::fclose(fp);

    AdlBankSetup setup;
    setup.volumeModel = VOLUME_CMF;
    setup.deepTremolo = false;
    setup.deepVibrato = false;
    setup.adLibPercussions = false;
    setup.scaleModulators = false;
    SetBankSetup(bank, setup);

    return true;
}


#endif // LOAD_EA_H
