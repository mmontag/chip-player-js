
#include <stdint.h>
#include <string.h>

#include "usf.h"

#include <stdio.h>
#include <stdlib.h>

#include "usf_internal.h"

#include "api/callbacks.h"
#include "main/savestates.h"
#include "r4300/cached_interp.h"
#include "r4300/r4300.h"

size_t usf_get_state_size()
{
    return sizeof(usf_state_t) + 8192;
}

void usf_clear(void * state)
{
    size_t offset;
    memset(state, 0, usf_get_state_size());
    offset = 4096 - (((uintptr_t)state) & 4095);
    USF_STATE_HELPER->offset_to_structure = offset;
    
    //USF_STATE->enablecompare = 0;
    //USF_STATE->enableFIFOfull = 0;
    
    //USF_STATE->enable_hle_audio = 0;

    // Constants, never written to
    USF_STATE->trunc_mode = 0xF3F;
    USF_STATE->round_mode = 0x33F;
    USF_STATE->ceil_mode = 0xB3F;
    USF_STATE->floor_mode = 0x73F;
    USF_STATE->precomp_instr_size = sizeof(precomp_instr);
    
    USF_STATE->save_state = malloc( 0x80275c );
    USF_STATE->save_state_size = 0x80275c;
    
    for (offset = 0; offset < 0x10000; offset += 4)
    {
        USF_STATE->EmptySpace[offset / 4] = (uint32_t)((offset << 16) | offset);
    }
}

void usf_set_hle_audio(void * state, int enable)
{
    USF_STATE->enable_hle_audio = enable;
}

static uint32_t get_le32( const void * _p )
{
    const uint8_t * p = (const uint8_t *) _p;
    return p[0] + p[1] * 0x100 + p[2] * 0x10000 + p[3] * 0x1000000;
}

int usf_upload_section(void * state, const uint8_t * data, size_t size)
{
    uint32_t temp;
    
    if ( size < 4 ) return -1;
    temp = get_le32( data ); data += 4; size -= 4;

    if(temp == 0x34365253) { //there is a rom section
        uint32_t len, start;
        
        if ( size < 4 ) return -1;
        len = get_le32( data ); data += 4; size -= 4;

		while(len) {
            if ( size < 4 ) return -1;
            start = get_le32( data ); data += 4; size -= 4;

            if ( start + len > USF_STATE->g_rom_size )
            {
                while ( start + len > USF_STATE->g_rom_size )
                {
                    if (!USF_STATE->g_rom_size)
                        USF_STATE->g_rom_size = 1024 * 1024;
                    else
                        USF_STATE->g_rom_size *= 2;
                }
                
                USF_STATE->g_rom = realloc( USF_STATE->g_rom, USF_STATE->g_rom_size );
                if ( !USF_STATE->g_rom )
                    return -1;
            }
            
            memcpy( USF_STATE->g_rom + start, data, len );
            data += len; size -= len;

            if ( size < 4 ) return -1;
            len = get_le32( data ); data += 4; size -= 4;
		}
	}

    if ( size < 4 ) return -1;
    temp = get_le32( data ); data += 4; size -= 4;

	if(temp == 0x34365253) {
		uint32_t len, start;
        
        if ( size < 4 ) return -1;
        len = get_le32( data ); data += 4; size -= 4;

		while(len) {
            if ( size < 4 ) return -1;
            start = get_le32( data ); data += 4; size -= 4;

            if ( size < len ) return -1;
            memcpy( USF_STATE->save_state + start, data, len );
            data += len; size -= len;

            if ( size < 4 ) return -1;
            len = get_le32( data ); data += 4; size -= 4;
		}
	}

	return 0;
}

static int usf_startup(void * state)
{
    // Detect the Ramsize before the memory allocation
	
	if(get_le32(USF_STATE->save_state + 4) == 0x400000) {
        void * savestate;
		savestate = realloc(USF_STATE->save_state, 0x40275c);
        if ( savestate )
            USF_STATE->save_state = savestate;
        USF_STATE->save_state_size = 0x40275c;
	}
    
    open_rom(USF_STATE);

    main_start(USF_STATE);
    
    USF_STATE->MemoryState = 1;
    
    return 0;
}

void usf_set_audio_format(void *opaque, unsigned int frequency, unsigned int bits)
{
    usf_state_t * state = (usf_state_t *)opaque;
    
    state->SampleRate = frequency;
}

void usf_push_audio_samples(void *opaque, const void * buffer, size_t size)
{
    usf_state_t * state = (usf_state_t *)opaque;
    
    int16_t * samplePtr = (int16_t *)buffer;
    int16_t * samplesOut;
    size_t samplesTodo;
    size_t i;
    size /= 4;
    
    samplesTodo = size;
    if (samplesTodo > state->sample_buffer_count)
        samplesTodo = state->sample_buffer_count;
    state->sample_buffer_count -= samplesTodo;
    
    samplesOut = state->sample_buffer;
    size -= samplesTodo;
    
    if (samplesOut)
    {
        for (i = 0; i < samplesTodo; ++i)
        {
            *samplesOut++ = samplePtr[1];
            *samplesOut++ = samplePtr[0];
            samplePtr += 2;
        }
        state->sample_buffer = samplesOut;
    }
    else
        samplePtr += samplesTodo * 2;
    
    if (size)
    {
        samplesTodo = 8192 - state->samples_in_buffer;
        if (samplesTodo > size)
            samplesTodo = size;
        
        samplesOut = state->samplebuf + state->samples_in_buffer * 2;
        size -= samplesTodo;
        state->samples_in_buffer += samplesTodo;
        
        for (i = 0; i < samplesTodo; ++i)
        {
            *samplesOut++ = samplePtr[1];
            *samplesOut++ = samplePtr[0];
            samplePtr += 2;
        }
        
        state->stop = 1;
    }
    
    if (size)
        DebugMessage(state, 1, "Sample buffer full!");
}

const char * usf_render(void * state, int16_t * buffer, size_t count, int32_t * sample_rate)
{
    USF_STATE->last_error = 0;
    USF_STATE->error_message[0] = '\0';
    
    if ( !USF_STATE->MemoryState )
    {
        if ( usf_startup( state ) < 0 )
            return USF_STATE->last_error;
    }
    
    if ( USF_STATE->samples_in_buffer )
    {
        size_t do_max = USF_STATE->samples_in_buffer;
        if ( do_max > count )
            do_max = count;
       
        if ( buffer ) 
            memcpy( buffer, USF_STATE->samplebuf, sizeof(int16_t) * 2 * do_max );
        
        USF_STATE->samples_in_buffer -= do_max;
        
        if ( sample_rate )
            *sample_rate = USF_STATE->SampleRate;
        
        if ( USF_STATE->samples_in_buffer )
        {
            memmove( USF_STATE->samplebuf, USF_STATE->samplebuf + do_max * 2, sizeof(int16_t) * 2 * USF_STATE->samples_in_buffer );
            return 0;
        }
        
        if ( buffer )
            buffer += 2 * do_max;
        count -= do_max;
    }

    USF_STATE->sample_buffer = buffer;
    USF_STATE->sample_buffer_count = count;
    
    USF_STATE->stop = 0;
    
    main_run(USF_STATE);
    
    if ( sample_rate )
        *sample_rate = USF_STATE->SampleRate;
    
    return USF_STATE->last_error;
}

void usf_restart(void * state)
{
    if ( USF_STATE->MemoryState )
        savestates_load(state, USF_STATE->save_state, USF_STATE->save_state_size, 0);
    
    USF_STATE->samples_in_buffer = 0;
}

void usf_shutdown(void * state)
{
    r4300_end(USF_STATE);
    free_blocks(USF_STATE);
    free(USF_STATE->save_state);
    USF_STATE->save_state = 0;
    close_rom(USF_STATE);
    free(USF_STATE->g_rom);
    USF_STATE->g_rom = 0;
}
