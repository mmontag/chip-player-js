/*
	panning.h by Maxim in 2006
	Implements "simple equal power" panning using sine distribution
	I am not an expert on this stuff, but this is the best sounding of the methods I've tried
*/

#ifndef PANNING_H
#define PANNING_H

#include <stdtype.h>

#define PANNING_BITS	16	// 16.16 fixed point
#define PANNING_NORMAL	(1 << PANNING_BITS)

// apply panning value to x, x should be within +-16384
#define APPLY_PANNING(x, panval)	((x * panval) >> PANNING_BITS)
// apply panning value to x, version for larger values
#define APPLY_PANNING_L(x, panval)	(INT32)(((INT64)x * panval) >> PANNING_BITS)

void calc_panning(INT32 channels[2], INT16 position);
void centre_panning(INT32 channels[2]);

#endif
