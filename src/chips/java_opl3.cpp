/*
 * Interfaces over Yamaha OPL3 (YMF262) chip emulators
 *
 * Copyright (c) 2017-2019 Vitaly Novichkov (Wohlstand)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "java_opl3.h"
#include "java/OPL3.cpp"

JavaOPL3::JavaOPL3() :
    OPLChipBaseBufferedT(),
    m_chip(new JavaOPL::OPL3(true))
{
    reset();
}

JavaOPL3::~JavaOPL3()
{
    JavaOPL::OPL3 *chip_r = reinterpret_cast<JavaOPL::OPL3 *>(m_chip);
    delete chip_r;
}

void JavaOPL3::setRate(uint32_t rate)
{
    OPLChipBaseBufferedT::setRate(rate);
    JavaOPL::OPL3 *chip_r = reinterpret_cast<JavaOPL::OPL3 *>(m_chip);
    chip_r->Reset();
}

void JavaOPL3::reset()
{
    OPLChipBaseBufferedT::reset();
    JavaOPL::OPL3 *chip_r = reinterpret_cast<JavaOPL::OPL3 *>(m_chip);
    chip_r->Reset();
}

void JavaOPL3::writeReg(uint16_t addr, uint8_t data)
{
    JavaOPL::OPL3 *chip_r = reinterpret_cast<JavaOPL::OPL3 *>(m_chip);
    chip_r->WriteReg(addr, data);
}

void JavaOPL3::writePan(uint16_t addr, uint8_t data)
{
    JavaOPL::OPL3 *chip_r = reinterpret_cast<JavaOPL::OPL3 *>(m_chip);
    // TODO panning
}

void JavaOPL3::nativeGenerateN(int16_t *output, size_t frames)
{
    JavaOPL::OPL3 *chip_r = reinterpret_cast<JavaOPL::OPL3 *>(m_chip);

    enum { maxframes = 256 };

    float buf[2 * maxframes];
    while(frames > 0)
    {
        memset(buf, 0, sizeof(buf));

        size_t curframes = (frames < maxframes) ? frames : maxframes;
        chip_r->Update(buf, curframes);

        size_t cursamples = 2 * curframes;
        for(size_t i = 0; i < cursamples; ++i)
        {
            int32_t sample = (int32_t)lround(32768 * buf[i]);
            sample = (sample > -32768) ? sample : -32768;
            sample = (sample < +32767) ? sample : +32767;
            output[i] = (int16_t)sample;
        }

        output += cursamples;
        frames -= curframes;
    }
}

const char *JavaOPL3::emulatorName()
{
    return "Java 1.0.6 OPL3";
}
