// license:BSD-3-Clause
// copyright-holders:Andrew Gardner,Aaron Giles
/***************************************************************************

    okiadpcm.h

    OKI ADCPM emulation.

***************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "../../stdtype.h"
#include "okiadpcm.h"


//**************************************************************************
//  ADPCM STATE HELPER
//**************************************************************************

// ADPCM state and tables
static UINT8 s_tables_computed = 0;
static const INT8 s_index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static INT16 s_diff_lookup[49*16];

static void compute_tables(void);

oki_adpcm_state* oki_adpcm_create(const INT8* custom_index_shift, const INT16* custom_diff_lookup)
{
	oki_adpcm_state* adpcm;
	
	adpcm = (oki_adpcm_state*)calloc(1, sizeof(oki_adpcm_state));
	if (adpcm == NULL)
		return NULL;
	
	oki_adpcm_init(adpcm, custom_index_shift, custom_diff_lookup);
	
	return adpcm;
}

void oki_adpcm_destroy(oki_adpcm_state* adpcm)
{
	free(adpcm);
	
	return;
}

void oki_adpcm_init(oki_adpcm_state* adpcm, const INT8* custom_index_shift, const INT16* custom_diff_lookup)
{
	adpcm->index_shift =  (custom_index_shift != NULL) ? custom_index_shift : s_index_shift;
	if (custom_diff_lookup != NULL)
	{
		adpcm->diff_lookup = custom_diff_lookup;
	}
	else
	{
		compute_tables();
		adpcm->diff_lookup = s_diff_lookup;
	}
	oki_adpcm_reset(adpcm);
	
	return;
}

//-------------------------------------------------
//  reset - reset the ADPCM state
//-------------------------------------------------

void oki_adpcm_reset(oki_adpcm_state* adpcm)
{
	// reset the signal/step
	adpcm->signal = -2;
	adpcm->step = 0;
}


//-------------------------------------------------
//  clock -- clock the next ADPCM byte
//-------------------------------------------------

INT16 oki_adpcm_clock(oki_adpcm_state* adpcm, UINT8 nibble)
{
	// update the signal
	adpcm->signal += adpcm->diff_lookup[adpcm->step * 16 + (nibble & 15)];

	// clamp to the maximum
	if (adpcm->signal > 2047)
		adpcm->signal = 2047;
	else if (adpcm->signal < -2048)
		adpcm->signal = -2048;

	// adjust the step size and clamp
	adpcm->step += adpcm->index_shift[nibble & 7];
	if (adpcm->step > 48)
		adpcm->step = 48;
	else if (adpcm->step < 0)
		adpcm->step = 0;

	// return the signal
	return adpcm->signal;
}


//-------------------------------------------------
//  compute_tables - precompute tables for faster
//  sound generation
//-------------------------------------------------

static void compute_tables(void)
{
	// nibble to bit map
	static const INT8 nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};
	int step, nib;

	// skip if we already did it
	if (s_tables_computed)
		return;
	s_tables_computed = 1;

	// loop over all possible steps
	for (step = 0; step <= 48; step++)
	{
		// compute the step value
		int stepval = (int)floor(16.0 * pow(11.0 / 10.0, (double)step));

		// loop over all nibbles and compute the difference
		for (nib = 0; nib < 16; nib++)
		{
			s_diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
					stepval/2 * nbl2bit[nib][2] +
					stepval/4 * nbl2bit[nib][3] +
					stepval/8);
		}
	}
	
	return;
}
