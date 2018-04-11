#include "nuked_opl3_v174.h"
#include "nuked/nukedopl3_174.h"
#include <cstring>

NukedOPL3v174::NukedOPL3v174() :
    OPLChipBase()
{
    m_chip = new opl3_chip;
    reset(m_rate);
}

NukedOPL3v174::NukedOPL3v174(const NukedOPL3v174 &c):
    OPLChipBase(c)
{
    m_chip = new opl3_chip;
    std::memset(m_chip, 0, sizeof(opl3_chip));
    reset(c.m_rate);
}

NukedOPL3v174::~NukedOPL3v174()
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    delete chip_r;
}

void NukedOPL3v174::setRate(uint32_t rate)
{
    OPLChipBase::setRate(rate);
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3v17_Reset(chip_r, rate);
}

void NukedOPL3v174::reset()
{
    setRate(m_rate);
}

void NukedOPL3v174::reset(uint32_t rate)
{
    setRate(rate);
}

void NukedOPL3v174::writeReg(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_WriteReg(chip_r, addr, data);
}

int NukedOPL3v174::generate(int16_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_GenerateStream(chip_r, output, (Bit32u)frames);
    return (int)frames;
}

int NukedOPL3v174::generateAndMix(int16_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_GenerateStreamMix(chip_r, output, (Bit32u)frames);
    return (int)frames;
}

int NukedOPL3v174::generate32(int32_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    for(size_t i = 0; i < frames; ++i) {
        int16_t frame[2];
        OPL3v17_GenerateResampled(chip_r, frame);
        output[0] = (int32_t)frame[0];
        output[1] = (int32_t)frame[1];
        output += 2;
    }
    return (int)frames;
}

int NukedOPL3v174::generateAndMix32(int32_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    for(size_t i = 0; i < frames; ++i) {
        int16_t frame[2];
        OPL3v17_GenerateResampled(chip_r, frame);
        output[0] += (int32_t)frame[0];
        output[1] += (int32_t)frame[1];
        output += 2;
    }
    return (int)frames;
}

const char *NukedOPL3v174::emulatorName()
{
    return "Nuked OPL3 (v 1.7.4)";
}
