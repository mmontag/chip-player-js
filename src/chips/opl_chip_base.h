#ifndef ONP_CHIP_BASE_H
#define ONP_CHIP_BASE_H

#include <stdint.h>
#include <stddef.h>

#if !defined(_MSC_VER) && (__cplusplus <= 199711L)
#define final
#define override
#endif

#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
class VResampler;
#endif

class OPLChipBase
{
protected:
    uint32_t m_rate;
public:
    OPLChipBase();
    virtual ~OPLChipBase();

    virtual void setRate(uint32_t rate) = 0;
    virtual void reset() = 0;
    virtual void writeReg(uint16_t addr, uint8_t data) = 0;

    virtual void nativePreGenerate() = 0;
    virtual void nativePostGenerate() = 0;
    virtual void nativeGenerate(int16_t *frame) = 0;

    virtual void generate(int16_t *output, size_t frames) = 0;
    virtual void generateAndMix(int16_t *output, size_t frames) = 0;
    virtual void generate32(int32_t *output, size_t frames) = 0;
    virtual void generateAndMix32(int32_t *output, size_t frames) = 0;

    virtual const char* emulatorName() = 0;
private:
    OPLChipBase(const OPLChipBase &c);
    OPLChipBase &operator=(const OPLChipBase &c);
};

// A base class providing F-bounded generic and efficient implementations,
// supporting resampling of chip outputs
template <class T>
class OPLChipBaseT : public OPLChipBase
{
public:
    OPLChipBaseT();
    virtual ~OPLChipBaseT();

    virtual void setRate(uint32_t rate) override;
    virtual void reset() override;
    void generate(int16_t *output, size_t frames) override;
    void generateAndMix(int16_t *output, size_t frames) override;
    void generate32(int32_t *output, size_t frames) override;
    void generateAndMix32(int32_t *output, size_t frames) override;
private:
    void setupResampler(uint32_t rate);
    void resetResampler();
    void resampledGenerate(int32_t *output);
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    VResampler *m_resampler;
#else
    int32_t m_oldsamples[2];
    int32_t m_samples[2];
    int32_t m_samplecnt;
    int32_t m_rateratio;
    enum { rsm_frac = 10 };
#endif
    // amplitude scale factors in and out of resampler, varying for chips;
    // values are OK to "redefine", the static polymorphism will accept it.
    enum { resamplerPreAmplify = 1, resamplerPostAttenuate = 1 };
};

// A base class which provides frame-by-frame interfaces on emulations which
// don't have a routine for it. It produces outputs in fixed size buffers.
// Fast register updates will suffer some latency because of buffering.
template <class T, unsigned Buffer = 256>
class OPLChipBaseBufferedT : public OPLChipBaseT<T>
{
public:
    OPLChipBaseBufferedT()
        : OPLChipBaseT<T>(), m_bufferIndex(0) {}
    virtual ~OPLChipBaseBufferedT()
        {}
public:
    void reset() override;
    void nativeGenerate(int16_t *frame) override;
protected:
    virtual void nativeGenerateN(int16_t *output, size_t frames) = 0;
private:
    unsigned m_bufferIndex;
    int16_t m_buffer[2 * Buffer];
};

#include "opl_chip_base.tcc"

#endif // ONP_CHIP_BASE_H
