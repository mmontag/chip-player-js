#ifndef NUKED_OPL3174_H
#define NUKED_OPL3174_H

#include "opl_chip_base.h"

#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
class VResampler;
#endif

class NukedOPL3v174 final : public OPLChipBase
{
    void *m_chip;
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    VResampler *m_resampler;
#endif
public:
    NukedOPL3v174();
    virtual ~NukedOPL3v174() override;

    virtual void setRate(uint32_t rate) override;
    virtual void reset() override;
    virtual void reset(uint32_t rate) override;
    virtual void writeReg(uint16_t addr, uint8_t data) override;
    virtual int generate(int16_t *output, size_t frames) override;
    virtual int generateAndMix(int16_t *output, size_t frames) override;
    virtual int generate32(int32_t *output, size_t frames) override;
    virtual int generateAndMix32(int32_t *output, size_t frames) override;
    virtual const char *emulatorName() override;
private:
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    void generateResampledHq(int16_t *out);
    void generateResampledHq32(int32_t *out);
#endif
};

#endif // NUKED_OPL3174_H
