#include "dosbox_opl3.h"
#include "dosbox/dbopl.h"
#include <cstdlib>
#include <assert.h>

DosBoxOPL3::DosBoxOPL3() :
    OPLChipBase(),
    m_chip(NULL)
{
    reset();
}

DosBoxOPL3::DosBoxOPL3(const DosBoxOPL3 &c) :
    OPLChipBase(c),
    m_chip(NULL)
{
    setRate(c.m_rate);
}

DosBoxOPL3::~DosBoxOPL3()
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    delete chip_r;
}

void DosBoxOPL3::setRate(uint32_t rate)
{
    OPLChipBase::setRate(rate);
    reset();
}

void DosBoxOPL3::reset()
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    if(m_chip && chip_r)
        delete chip_r;
    m_chip = new DBOPL::Handler;
    chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->Init(m_rate);
}

void DosBoxOPL3::reset(uint32_t rate)
{
    setRate(rate);
}

void DosBoxOPL3::writeReg(uint16_t addr, uint8_t data)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->WriteReg(static_cast<Bit32u>(addr), data);
}

int DosBoxOPL3::generate(int16_t *output, size_t frames)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    Bitu left = (Bitu)frames;
    while(left > 0)
    {
        Bitu frames_i = left;
        chip_r->GenerateArr(output, &frames_i);
        output += (frames_i * 2);
        left -= frames_i;
    }
    return (int)frames;
}

int DosBoxOPL3::generateAndMix(int16_t *output, size_t frames)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    Bitu left = (Bitu)frames;
    while(left > 0)
    {
        Bitu frames_i = left;
        chip_r->GenerateArrMix(output, &frames_i);
        output += (frames_i * 2);
        left -= frames_i;
    }
    return (int)frames;
}

int DosBoxOPL3::generate32(int32_t *output, size_t frames)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    Bitu left = (Bitu)frames;
    while(left > 0)
    {
        Bitu frames_i = left;
        chip_r->GenerateArr(output, &frames_i);
        output += (frames_i * 2);
        left -= frames_i;
    }
    return (int)frames;
}

int DosBoxOPL3::generateAndMix32(int32_t *output, size_t frames)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    Bitu left = (Bitu)frames;
    while(left > 0)
    {
        Bitu frames_i = left;
        chip_r->GenerateArrMix(output, &frames_i);
        output += (frames_i * 2);
        left -= frames_i;
    }
    return (int)frames;
}

const char *DosBoxOPL3::emulatorName()
{
    return "DosBox 0.74 OPL3";
}
