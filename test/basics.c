#define _ISOC99_SOURCE
#include "../gme/gme.h"

#include "Wave_Writer.h" /* wave_ functions for writing sound file */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

void handle_error( const char* str );

int main(int argc, char *argv[])
{
	if ( argc < 3 ) {
		return 1;
	}
	const char *filename = argv[1];
	const char *outname  = argv[2];

	long sample_rate = 44100; /* number of samples per second */
	int track = 0;

	/* Open music file in new emulator */
	Music_Emu* emu;
	handle_error( gme_open_file( filename, &emu, sample_rate ) );

	/* Start track */
	handle_error( gme_start_track( emu, track ) );

	/* Begin writing to wave file */
	wave_open( sample_rate, outname );
	wave_enable_stereo();

	/* Record 3 minutes of track */
	while ( gme_tell( emu ) < 180 * 1000L )
	{
		/* Sample buffer */
		#define buf_size 16384 /* can be any multiple of 2 */
		short buf [buf_size];

		/* Fill sample buffer */
		handle_error( gme_play( emu, buf_size, buf ) );

		/* Write samples to wave file */
		wave_write( buf, buf_size );
	}

	/* Cleanup */
	gme_delete( emu );
	wave_close();

	return 0;
}

void handle_error( const char* str )
{
	if ( str )
	{
		printf( "Error: %s\n", str );
		exit( EXIT_FAILURE );
	}
}
