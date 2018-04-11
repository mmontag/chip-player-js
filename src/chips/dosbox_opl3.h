#ifndef DOSBOX_OPL3_H
#define DOSBOX_OPL3_H

#include "opl_chip_base.h"

class DosBoxOPL3 final : public OPLChipBase
{
    void *m_chip;
public:
    DosBoxOPL3();
    DosBoxOPL3(const DosBoxOPL3 &c);
    virtual ~DosBoxOPL3() override;

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

#endif // DOSBOX_OPL3_H
