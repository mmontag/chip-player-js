/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.h                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef M64P_MEMORY_MEMORY_H
#define M64P_MEMORY_MEMORY_H

#include <stdint.h>

#define read_word_in_memory() state->readmem[state->address>>16](state)
#define read_byte_in_memory() state->readmemb[state->address>>16](state)
#define read_hword_in_memory() state->readmemh[state->address>>16](state)
#define read_dword_in_memory() state->readmemd[state->address>>16](state)
#define write_word_in_memory() state->writemem[state->address>>16](state)
#define write_byte_in_memory() state->writememb[state->address >>16](state)
#define write_hword_in_memory() state->writememh[state->address >>16](state)
#define write_dword_in_memory() state->writememd[state->address >>16](state)

#ifndef M64P_BIG_ENDIAN
#if defined(__GNUC__) && (__GNUC__ > 4  || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
#define sl(x) __builtin_bswap32(x)
#else
#define sl(mot) \
( \
((mot & 0x000000FF) << 24) | \
((mot & 0x0000FF00) <<  8) | \
((mot & 0x00FF0000) >>  8) | \
((mot & 0xFF000000) >> 24) \
)
#endif
#define S8 3
#define S16 2
#define Sh16 1

#else

#define sl(mot) mot
#define S8 0
#define S16 0
#define Sh16 0

#endif

static inline void masked_write(uint32_t* dst, uint32_t value, uint32_t mask)
{
    *dst = (*dst & ~mask) | (value & mask);
}

int init_memory(usf_state_t *);

void map_region(usf_state_t *,
                uint16_t region,
                int type,
                void (*read8)(usf_state_t *),
                void (*read16)(usf_state_t *),
                void (*read32)(usf_state_t *),
                void (*read64)(usf_state_t *),
                void (*write8)(usf_state_t *),
                void (*write16)(usf_state_t *),
                void (*write32)(usf_state_t *),
                void (*write64)(usf_state_t *));

/* XXX: cannot make them static because of dynarec + rdp fb */
void read_rdram(usf_state_t *);
void read_rdramb(usf_state_t *);
void read_rdramh(usf_state_t *);
void read_rdramd(usf_state_t *);
void write_rdram(usf_state_t *);
void write_rdramb(usf_state_t *);
void write_rdramh(usf_state_t *);
void write_rdramd(usf_state_t *);
void read_rdramFB(usf_state_t *);
void read_rdramFBb(usf_state_t *);
void read_rdramFBh(usf_state_t *);
void read_rdramFBd(usf_state_t *);
void write_rdramFB(usf_state_t *);
void write_rdramFBb(usf_state_t *);
void write_rdramFBh(usf_state_t *);
void write_rdramFBd(usf_state_t *);

/* Returns a pointer to a block of contiguous memory
 * Can access RDRAM, SP_DMEM, SP_IMEM and ROM, using TLB if necessary
 * Useful for getting fast access to a zone with executable code. */
unsigned int *fast_mem_access(usf_state_t *, unsigned int address);

#endif

