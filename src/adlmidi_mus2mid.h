/*
 * MUS2MIDI: DMX (DOOM) MUS to MIDI Library Header
 *
 * Copyright (C) 2014-2016  Bret Curtis
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef MUSLIB_H
#define MUSLIB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

int AdlMidi_mus2midi(uint8_t *in, uint32_t insize,
                     uint8_t **out, uint32_t *outsize,
                     uint16_t frequency);

#ifdef __cplusplus
}
#endif

#endif /* MUSLIB_H */
