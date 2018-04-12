#ifndef ONP_CHIP_BASE_H
#define ONP_CHIP_BASE_H

#include <stdint.h>
#include <stddef.h>

class OPLChipBase
{
protected:
    uint32_t m_rate;
public:
    OPLChipBase();
    OPLChipBase(const OPLChipBase &c);
    virtual ~OPLChipBase();

    virtual void setRate(uint32_t rate);
    virtual void reset() = 0;
    virtual void reset(uint32_t rate);
    virtual void writeReg(uint16_t addr, uint8_t data) = 0;
    virtual int generate(int16_t *output, size_t frames) = 0;
    virtual int generateAndMix(int16_t *output, size_t frames) = 0;
    virtual int generate32(int32_t *output, size_t frames);
    virtual int generateAndMix32(int32_t *output, size_t frames);
    virtual const char* emulatorName() = 0;
};

#endif // ONP_CHIP_BASE_H
