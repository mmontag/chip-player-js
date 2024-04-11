/*
	panning.c by Maxim in 2006
	Implements "simple equal power" panning using sine distribution
	I am not an expert on this stuff, but this is the best sounding of the methods I've tried
*/

#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "../stdtype.h"
#include "panning.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

#define RANGE		(0x100 * 2)
#define POS_MULT	(M_PI / 2.0 / RANGE)

//-----------------------------------------------------------------
// Set the panning values for the two stereo channels (L,R)
// for a position -256..0..256 L..C..R
//-----------------------------------------------------------------
void Panning_Calculate(INT32 channels[2], INT16 position)
{
	if ( position > RANGE / 2 )
		position = RANGE / 2;
	else if ( position < -RANGE / 2 )
		position = -RANGE / 2;
	position += RANGE / 2;	// make -256..0..256 -> 0..256..512
	
	// Equal power law: equation is
	// right = sin( position / range * pi / 2) * sqrt( 2 )
	// left is equivalent to right with position = range - position
	// position is in the range 0 .. RANGE
	// RANGE / 2 = centre, result = 1.0f
	channels[1] = (INT32)( sin((double)position * POS_MULT) * M_SQRT2 * PANNING_NORMAL );
	position = RANGE - position;
	channels[0] = (INT32)( sin((double)position * POS_MULT) * M_SQRT2 * PANNING_NORMAL );
}

//-----------------------------------------------------------------
// Reset the panning values to the centre position
//-----------------------------------------------------------------
void Panning_Centre(INT32 channels[2])
{
	channels[0] = channels[1] = PANNING_NORMAL;
}

/*//-----------------------------------------------------------------
// Generate a stereo position in the range 0..RANGE
// with Gaussian distribution, mean RANGE/2, S.D. RANGE/5
//-----------------------------------------------------------------
int random_stereo()
{
	int n = (int)(RANGE/2 + gauss_rand() * (RANGE * 0.2) );
	if ( n > RANGE ) n = RANGE;
	if ( n < 0 ) n = 0;
	return n;
}

//-----------------------------------------------------------------
// Generate a Gaussian random number with mean 0, variance 1
// Copied from an ancient C newsgroup FAQ
//-----------------------------------------------------------------
double gauss_rand()
{
	static double V1, V2, S;
	static int phase = 0;
	double X;

	if(phase == 0) {
		do {
			double U1 = (double)rand() / RAND_MAX;
			double U2 = (double)rand() / RAND_MAX;

			V1 = 2 * U1 - 1;
			V2 = 2 * U2 - 1;
			S = V1 * V1 + V2 * V2;
			} while(S >= 1 || S == 0);

		X = V1 * sqrt(-2 * log(S) / S);
	} else
		X = V2 * sqrt(-2 * log(S) / S);

	phase = 1 - phase;

	return X;
}*/
