#include <stdlib.h>
#include <string.h>

#include "../usf/usf.h"

#include <psflib.h>

unsigned char * state = 0;

unsigned int enable_compare = 0;
unsigned int enable_fifo_full = 0;

unsigned long length_ms = 0;
unsigned long fade_ms = 0;

static void * stdio_fopen( const char * path )
{
    return fopen( path, "rb" );
}

static size_t stdio_fread( void *p, size_t size, size_t count, void *f )
{
    return fread( p, size, count, (FILE*) f );
}

static int stdio_fseek( void * f, int64_t offset, int whence )
{
    return fseek( (FILE*) f, offset, whence );
}

static int stdio_fclose( void * f )
{
    return fclose( (FILE*) f );
}

static long stdio_ftell( void * f )
{
    return ftell( (FILE*) f );
}

static psf_file_callbacks stdio_callbacks =
{
    "\\/:",
    stdio_fopen,
    stdio_fread,
    stdio_fseek,
    stdio_fclose,
    stdio_ftell
};

static int usf_loader(void * context, const uint8_t * exe, size_t exe_size,
                      const uint8_t * reserved, size_t reserved_size)
{
    if ( exe && exe_size > 0 ) return -1;

    return usf_upload_section( state, reserved, reserved_size );
}

#define BORK_TIME 0xC0CAC01A

static unsigned long parse_time_crap(const char *input)
{
    unsigned long value = 0;
    unsigned long multiplier = 1000;
    const char * ptr = input;
    unsigned long colon_count = 0;
    
    while (*ptr && ((*ptr >= '0' && *ptr <= '9') || *ptr == ':'))
    {
        colon_count += *ptr == ':';
        ++ptr;
    }
    if (colon_count > 2) return BORK_TIME;
    if (*ptr && *ptr != '.' && *ptr != ',') return BORK_TIME;
    if (*ptr) ++ptr;
    while (*ptr && *ptr >= '0' && *ptr <= '9') ++ptr;
    if (*ptr) return BORK_TIME;
    
    ptr = strrchr(input, ':');
    if (!ptr)
        ptr = input;
    for (;;)
    {
        char * end;
        if (ptr != input) ++ptr;
        if (multiplier == 1000)
        {
            double temp = strtod(ptr, &end);
            if (temp >= 60.0) return BORK_TIME;
            value = (long)(temp * 1000.0f);
        }
        else
        {
            unsigned long temp = strtoul(ptr, &end, 10);
            if (temp >= 60 && multiplier < 3600000) return BORK_TIME;
            value += temp * multiplier;
        }
        if (ptr == input) break;
        ptr -= 2;
        while (ptr > input && *ptr != ':') --ptr;
        multiplier *= 60;
    }
    
    return value;
}

static int usf_info(void * context, const char * name, const char * value)
{
    if (!strcasecmp(name, "length"))
        length_ms += parse_time_crap(value);
    else if (!strcasecmp(name, "fade"))
        fade_ms += parse_time_crap(value);
    else if (!strcasecmp(name, "_enablecompare") && *value)
        enable_compare = 1;
    else if (!strcasecmp(name, "_enablefifofull") && *value)
        enable_fifo_full = 1;
    
    return 0;
}

static void print_message(void * unused, const char * message)
{
	fputs(message, stderr);
}

int main(int argc, char ** argv)
{
	if ( argc == 3 )
	{
        FILE * f;
        
        int32_t sample_rate;
        int32_t length_to_render;
        int32_t fade_total;
        int32_t i;
        
        int16_t sample_buffer[2048];
        
        state = (unsigned char *) malloc(usf_get_state_size());
        
        usf_clear(state);
 
		if ( psf_load( argv[1], &stdio_callbacks, 0x21, usf_loader, 0, usf_info, 0, 1, print_message, 0 ) <= 0 )
            return 1;
        
        usf_set_compare(state, enable_compare);
        usf_set_fifo_full(state, enable_fifo_full);
        
        usf_set_hle_audio(state, 1);
        
        f = fopen(argv[2], "wb");
        
        length_to_render = (int32_t)(length_ms * 441 / 10);
        
        while (length_to_render)
        {
            int32_t samples_todo = length_to_render;
            if (samples_todo > 1024)
                samples_todo = 1024;
            
            usf_render_resampled(state, sample_buffer, samples_todo, 44100);
            fwrite(sample_buffer, samples_todo, 4, f);
            
            length_to_render -= samples_todo;
        }
        
        fade_total = length_to_render = (int32_t)(fade_ms * 441 / 10);
        
        while (length_to_render)
        {
            int32_t samples_todo = length_to_render;
            if (samples_todo > 1024)
                samples_todo = 1024;
            
            usf_render_resampled(state, sample_buffer, samples_todo, 44100);
            
            for (i = 0; i < samples_todo; ++i)
            {
                sample_buffer[i * 2 + 0] = (int64_t)sample_buffer[i * 2 + 0] * (length_to_render - i) / fade_total;
                sample_buffer[i * 2 + 1] = (int64_t)sample_buffer[i * 2 + 1] * (length_to_render - i) / fade_total;
            }
            
            fwrite(sample_buffer, samples_todo, 4, f);
            
            length_to_render -= samples_todo;
        }
        
        fclose(f);
        
        usf_shutdown(state);
        
        free(state);
	}
    
    return 0;
}
