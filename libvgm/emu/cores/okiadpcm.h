// license:BSD-3-Clause
// copyright-holders:Andrew Gardner,Aaron Giles
/***************************************************************************

    okiadpcm.h

    OKI ADCPM emulation.

***************************************************************************/

#ifndef __OKIADPCM_H__
#define __OKIADPCM_H__


// Internal ADPCM state, used by external ADPCM generators with compatible specs to the OKIM 6295.
typedef struct _oki_adpcm_state
{
	INT16   signal;
	INT16   step;

	const INT8* index_shift;
	const INT16* diff_lookup;
} oki_adpcm_state;

oki_adpcm_state* oki_adpcm_create(const INT8* custom_index_shift, const INT16* custom_diff_lookup);
void oki_adpcm_destroy(oki_adpcm_state* adpcm);
void oki_adpcm_init(oki_adpcm_state* adpcm, const INT8* custom_index_shift, const INT16* custom_diff_lookup);
void oki_adpcm_reset(oki_adpcm_state* adpcm);
INT16 oki_adpcm_clock(oki_adpcm_state* adpcm, UINT8 nibble);


#endif	// __OKIADPCM_H__
