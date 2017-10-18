/* snes_spc 0.9.0. http://www.slack.net/~ant/ */

#include "wave_writer.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Copyright (C) 2003-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

enum { buf_size = 32768 * 2 };
enum { header_size = 0x2C };

typedef short sample_t;

static unsigned char* buf;
static FILE* file;
static long  sample_count_;
static long  sample_rate_;
static long  buf_pos;
static int   chan_count;

static void exit_with_error( const char* str )
{
    fprintf(stderr, "WAVE Writer Error: %s\n", str );
    fflush(stderr);
}

int wave_open( long sample_rate, const char* filename )
{
    sample_count_ = 0;
    sample_rate_  = sample_rate;
    buf_pos       = header_size;
    chan_count    = 1;

    buf = (unsigned char*) malloc( buf_size * sizeof *buf );
    if ( !buf )
    {
        exit_with_error( "Out of memory" );
        return -1;
    }

#ifndef _WIN32
    file = fopen( filename, "wb" );
#else
    wchar_t widePath[MAX_PATH];
    int size = MultiByteToWideChar(CP_UTF8, 0, filename, strlen(filename), widePath, MAX_PATH);
    widePath[size] = '\0';
    file = _wfopen( widePath, L"wb" );
#endif
    if (!file)
    {
        exit_with_error( "Couldn't open WAVE file for writing" );
        return -1;
    }

    setvbuf( file, 0, _IOFBF, 32 * 1024L );
    return 0;
}

void wave_enable_stereo( void )
{
    chan_count = 2;
}

static void flush_()
{
    if ( buf_pos && !fwrite( buf, (size_t)buf_pos, 1, file ) )
        exit_with_error( "Couldn't write WAVE data" );
    buf_pos = 0;
}

void wave_write( short const* in, long remain )
{
    sample_count_ += remain;
    while ( remain )
    {
        if ( buf_pos >= buf_size )
            flush_();

        {
            unsigned char* p = &buf [buf_pos];
            long n = (buf_size - (unsigned long)buf_pos) / sizeof (sample_t);
            if ( n > remain )
                n = remain;
            remain -= n;

            /* convert to LSB first format */
            while ( n-- )
            {
                int s = *in++;
                *p++ = (unsigned char) s & (0x00FF);
                *p++ = (unsigned char) (s >> 8) & (0x00FF);
            }

            buf_pos = p - buf;
            assert( buf_pos <= buf_size );
        }
    }
}

long wave_sample_count( void )
{
    return sample_count_;
}

static void set_le32( void* p, unsigned long n )
{
    ((unsigned char*) p) [0] = (unsigned char) n & (0xFF);
    ((unsigned char*) p) [1] = (unsigned char) (n >> 8) & (0xFF);
    ((unsigned char*) p) [2] = (unsigned char) (n >> 16) & (0xFF);
    ((unsigned char*) p) [3] = (unsigned char) (n >> 24) & (0xFF);
}

void wave_close( void )
{
    if ( file )
    {
        /* generate header */
        unsigned char h [header_size] =
        {
            'R','I','F','F',
            0,0,0,0,        /* length of rest of file */
            'W','A','V','E',
            'f','m','t',' ',
            0x10,0,0,0,     /* size of fmt chunk */
            1,0,            /* uncompressed format */
            0,0,            /* channel count */
            0,0,0,0,        /* sample rate */
            0,0,0,0,        /* bytes per second */
            0,0,            /* bytes per sample frame */
            16,0,           /* bits per sample */
            'd','a','t','a',
            0,0,0,0,        /* size of sample data */
            /* ... */       /* sample data */
        };
        long ds = sample_count_ * (long)sizeof (sample_t);
        int frame_size = chan_count * (long)sizeof (sample_t);

        set_le32( h + 0x04, header_size - 8 + ds );
        h [0x16] = (unsigned char)chan_count;
        set_le32( h + 0x18, (unsigned long)sample_rate_ );
        set_le32( h + 0x1C, (unsigned long)sample_rate_ * (unsigned long)frame_size );
        h [0x20] = (unsigned char)frame_size;
        set_le32( h + 0x28, (unsigned long)ds );

        flush_();

        /* write header */
        fseek( file, 0, SEEK_SET );
        fwrite( h, header_size, 1, file );
        fclose( file );
        file = 0;
        free( buf );
        buf = 0;
    }
}
