#ifndef NUKED_OPL3_H
#define NUKED_OPL3_H

#include "opl_chip_base.h"

class NukedOPL3 final : public OPLChipBase
{
    void *m_chip;
public:
    NukedOPL3();
    NukedOPL3(const NukedOPL3 &c);
    virtual ~NukedOPL3() override;

    virtual void setRate(uint32_t rate) override;
    virtual void reset() override;
    virtual void reset(uint32_t rate) override;
    virtual void writeReg(uint16_t addr, uint8_t data) override;
    virtual int generate(int16_t *output, size_t frames) override;
    virtual int generateAndMix(int16_t *output, size_t frames) override;
    virtual int generate32(int32_t *output, size_t frames) override;
    virtual int generateAndMix32(int32_t *output, size_t frames) override;
    virtual const char *emulatorName() override;
};

#endif // NUKED_OPL3_H
