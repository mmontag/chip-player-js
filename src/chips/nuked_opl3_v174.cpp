#include "nuked_opl3_v174.h"
#include "nuked/nukedopl3_174.h"
#include <cstring>
#include <cmath>

#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
#include <zita-resampler/vresampler.h>
#endif

NukedOPL3v174::NukedOPL3v174() :
    OPLChipBase()
{
    m_chip = new opl3_chip;
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    m_resampler = new VResampler;
#endif
    reset(m_rate);
}

NukedOPL3v174::~NukedOPL3v174()
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    delete chip_r;
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    delete m_resampler;
#endif
}

void NukedOPL3v174::setRate(uint32_t rate)
{
    OPLChipBase::setRate(rate);
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3v17_Reset(chip_r, rate);
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    m_resampler->setup(rate * (1.0 / 49716), 2, 48);
#endif
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
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    for(size_t i = 0; i < frames; ++i)
    {
        generateResampledHq(output);
        output += 2;
    }
#else
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_GenerateStream(chip_r, output, (Bit32u)frames);
#endif
    return (int)frames;
}

int NukedOPL3v174::generateAndMix(int16_t *output, size_t frames)
{
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    for(size_t i = 0; i < frames; ++i)
    {
        int32_t frame[2];
        generateResampledHq32(frame);
        for (unsigned c = 0; c < 2; ++c) {
            int32_t temp = (int32_t)output[c] + frame[c];
            temp = (temp > -32768) ? temp : -32768;
            temp = (temp < 32767) ? temp : 32767;
            output[c] = temp;
        }
        output += 2;
    }
#else
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_GenerateStreamMix(chip_r, output, (Bit32u)frames);
#endif
    return (int)frames;
}

int NukedOPL3v174::generate32(int32_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    for(size_t i = 0; i < frames; ++i) {
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
        (void)chip_r;
        generateResampledHq32(output);
#else
        int16_t frame[2];
        OPL3v17_GenerateResampled(chip_r, frame);
        output[0] = (int32_t)frame[0];
        output[1] = (int32_t)frame[1];
#endif
        output += 2;
    }
    return (int)frames;
}

int NukedOPL3v174::generateAndMix32(int32_t *output, size_t frames)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    for(size_t i = 0; i < frames; ++i) {
#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
        (void)chip_r;
        int32_t frame[2];
        generateResampledHq32(frame);
#else
        int16_t frame[2];
        OPL3v17_GenerateResampled(chip_r, frame);
#endif
        output[0] += (int32_t)frame[0];
        output[1] += (int32_t)frame[1];
        output += 2;
    }
    return (int)frames;
}

#if defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
void NukedOPL3v174::generateResampledHq(int16_t *out)
{
    int32_t temps[2];
    generateResampledHq32(temps);
    for(unsigned i = 0; i < 2; ++i)
    {
        int32_t temp = temps[i];
        temp = (temp > -32768) ? temp : -32768;
        temp = (temp < 32767) ? temp : 32767;
        out[i] = temp;
    }
}

void NukedOPL3v174::generateResampledHq32(int32_t *out)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    VResampler *rsm = m_resampler;
    float f_in[2];
    float f_out[2];
    rsm->inp_count = 0;
    rsm->inp_data = f_in;
    rsm->out_count = 1;
    rsm->out_data = f_out;
    while(rsm->process(), rsm->out_count != 0)
    {
        int16_t in[2];
        OPL3v17_Generate(chip_r, in);
        f_in[0] = (float)in[0], f_in[1] = (float)in[1];
        rsm->inp_count = 1;
        rsm->inp_data = f_in;
        rsm->out_count = 1;
        rsm->out_data = f_out;
    }
    out[0] = std::lround(f_out[0]);
    out[1] = std::lround(f_out[1]);
}
#endif

const char *NukedOPL3v174::emulatorName()
{
    return "Nuked OPL3 (v 1.7.4)";
}
