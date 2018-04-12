#include "opl_chip_base.h"

OPLChipBase::OPLChipBase() :
    m_rate(44100)
{}

OPLChipBase::OPLChipBase(const OPLChipBase &c):
    m_rate(c.m_rate)
{}

OPLChipBase::~OPLChipBase()
{}

void OPLChipBase::setRate(uint32_t rate)
{
    m_rate = rate;
}

void OPLChipBase::reset(uint32_t rate)
{
    setRate(rate);
}

int OPLChipBase::generate32(int32_t *output, size_t frames)
{
    enum { maxFramesAtOnce = 256 };
    int16_t temp[2 * maxFramesAtOnce];
    for(size_t left = frames; left > 0;) {
        size_t count = (left < maxFramesAtOnce) ? left : maxFramesAtOnce;
        generate(temp, count);
        for(size_t i = 0; i < 2 * count; ++i)
            output[i] = temp[i];
        left -= count;
        output += 2 * count;
    }
    return (int)frames;
}

int OPLChipBase::generateAndMix32(int32_t *output, size_t frames)
{
    enum { maxFramesAtOnce = 256 };
    int16_t temp[2 * maxFramesAtOnce];
    for(size_t left = frames; left > 0;) {
        size_t count = (left < maxFramesAtOnce) ? left : maxFramesAtOnce;
        generate(temp, count);
        for(size_t i = 0; i < 2 * count; ++i)
            output[i] += temp[i];
        left -= count;
        output += 2 * count;
    }
    return (int)frames;
}
