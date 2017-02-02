/*********************************************************

    Konami 053260 PCM Sound Chip

*********************************************************/

#ifdef _DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "k053260.h"

static void k053260_update(void* param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_k053260(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_k053260(void* chip);
static void device_reset_k053260(void* chip);

static void k053260_w(void* chip, UINT8 offset, UINT8 data);
static UINT8 k053260_r(void* chip, UINT8 offset);

static void k053260_alloc_rom(void* chip, UINT32 memsize);
static void k053260_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);
static void k053260_set_mute_mask(void* chip, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, k053260_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, k053260_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, k053260_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, k053260_alloc_rom},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"K053260", "MAME", FCC_MAME,
	
	device_start_k053260,
	device_stop_k053260,
	device_reset_k053260,
	k053260_update,
	
	NULL,	// SetOptionBits
	k053260_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};
const DEV_DEF* devDefList_K053260[] =
{
	&devDef,
	NULL
};


/* 2004-02-28: Fixed PPCM decoding. Games sound much better now.*/

#define LOG 0

#define BASE_SHIFT	16

typedef struct _k053260_channel k053260_channel;
struct _k053260_channel
{
	UINT16		rate;
	UINT16		size;
	UINT16		start;
	UINT8		bank;
	UINT8		volume;
	UINT8		play;
	UINT8		pan;
	UINT32		pos;
	UINT8		loop;
	UINT8		ppcm; /* packed PCM ( 4 bit signed ) */
	INT8		ppcm_data;
	UINT8		Muted;
};

typedef struct _k053260_state k053260_state;
struct _k053260_state
{
	void* chipInf;
	
	UINT8						mode;
	UINT8						regs[0x30];
	UINT8						*rom;
	UINT32						rom_size;
	UINT32						*delta_table;
	k053260_channel				channels[4];
};

static void InitDeltaTable( k053260_state *ic, UINT32 rate, UINT32 clock )
{
	int		i;
	double	base = (double)rate;
	double	max = (double)clock; /* Hz */
	UINT32 val;

	for( i = 0; i < 0x1000; i++ ) {
		double v = ( double )( 0x1000 - i );
		double target = (max) / v;
		double fixed = ( double )( 1 << BASE_SHIFT );

		if ( target && base ) {
			target = fixed / ( base / target );
			val = ( UINT32 )target;
			if ( val == 0 )
				val = 1;
		} else
			val = 1;

		ic->delta_table[i] = val;
	}
}

static void device_reset_k053260(void* chip)
{
	k053260_state *ic = (k053260_state *)chip;
	int i;

	for( i = 0; i < 4; i++ ) {
		ic->channels[i].rate = 0;
		ic->channels[i].size = 0;
		ic->channels[i].start = 0;
		ic->channels[i].bank = 0;
		ic->channels[i].volume = 0;
		ic->channels[i].play = 0;
		ic->channels[i].pan = 0;
		ic->channels[i].pos = 0;
		ic->channels[i].loop = 0;
		ic->channels[i].ppcm = 0;
		ic->channels[i].ppcm_data = 0;
	}
}

static void k053260_update(void* param, UINT32 samples, DEV_SMPL **outputs)
{
	static const INT8 dpcmcnv[] = { 0,1,2,4,8,16,32,64, -128, -64, -32, -16, -8, -4, -2, -1};

	k053260_state *ic = (k053260_state *)param;
	UINT32 i, j;
	INT16 lvol[4], rvol[4];
	UINT8 play[4], loop[4], ppcm[4];
	UINT8 *rom[4];
	UINT32 delta[4], end[4], pos[4];
	INT8 ppcm_data[4];
	int dataL, dataR;
	INT8 d;

	/* precache some values */
	for ( i = 0; i < 4; i++ ) {
		if (ic->channels[i].Muted)
		{
			play[i] = 0;
			continue;
		}
		rom[i] = &ic->rom[ic->channels[i].start | ( ic->channels[i].bank << 16 )];
		delta[i] = ic->delta_table[ic->channels[i].rate];
		lvol[i] = ic->channels[i].volume * ic->channels[i].pan;
		rvol[i] = ic->channels[i].volume * ( 8 - ic->channels[i].pan );
		end[i] = ic->channels[i].size;
		pos[i] = ic->channels[i].pos;
		play[i] = ic->channels[i].play;
		loop[i] = ic->channels[i].loop;
		ppcm[i] = ic->channels[i].ppcm;
		ppcm_data[i] = ic->channels[i].ppcm_data;
		if ( ppcm[i] )
			delta[i] /= 2;
	}

	for ( j = 0; j < samples; j++ ) {

		dataL = dataR = 0;

		for ( i = 0; i < 4; i++ ) {
			/* see if the voice is on */
			if ( play[i] ) {
				/* see if we're done */
				if ( ( pos[i] >> BASE_SHIFT ) >= end[i] ) {

					ppcm_data[i] = 0;
					if ( loop[i] )
						pos[i] = 0;
					else {
						play[i] = 0;
						continue;
					}
				}

				if ( ppcm[i] ) { /* Packed PCM */
					/* we only update the signal if we're starting or a real sound sample has gone by */
					/* this is all due to the dynamic sample rate conversion */
					if ( pos[i] == 0 || ( ( pos[i] ^ ( pos[i] - delta[i] ) ) & 0x8000 ) == 0x8000 )

					{
						int newdata;
						if ( pos[i] & 0x8000 ){

							newdata = ((rom[i][pos[i] >> BASE_SHIFT]) >> 4) & 0x0f; /*high nybble*/
						}
						else{
							newdata = ( ( rom[i][pos[i] >> BASE_SHIFT] ) ) & 0x0f; /*low nybble*/
						}

						ppcm_data[i] += dpcmcnv[newdata];
					}



					d = ppcm_data[i];

					pos[i] += delta[i];
				} else { /* PCM */
					d = rom[i][pos[i] >> BASE_SHIFT];

					pos[i] += delta[i];
				}

				if ( ic->mode & 2 ) {
					dataL += ( d * lvol[i] ) >> 2;
					dataR += ( d * rvol[i] ) >> 2;
				}
			}
		}

		// lvol/rvol is swapped, so this assignment is correct
		outputs[1][j] = dataL;
		outputs[0][j] = dataR;
	}

	/* update the regs now */
	for ( i = 0; i < 4; i++ ) {
		if (ic->channels[i].Muted)
			continue;
		ic->channels[i].pos = pos[i];
		ic->channels[i].play = play[i];
		ic->channels[i].ppcm_data = ppcm_data[i];
	}
}

static UINT8 device_start_k053260(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	k053260_state *ic;
	UINT32 clock;
	UINT32 rate;
	int i;

	ic = (k053260_state *)calloc(1, sizeof(k053260_state));
	if (ic == NULL)
		return 0xFF;

	ic->mode = 0;
	ic->rom = NULL;
	ic->rom_size = 0x00;

	for ( i = 0; i < 0x30; i++ )
		ic->regs[i] = 0;

	clock = CHPCLK_CLOCK(cfg->clock);
	rate = clock / 32;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);

	ic->delta_table = (UINT32*)malloc(0x1000 * sizeof(UINT32));
	InitDeltaTable( ic, rate, clock );

	k053260_set_mute_mask(ic, 0x00);

	ic->chipInf = ic;
	INIT_DEVINF(retDevInf, (DEV_DATA*)ic, rate, &devDef);

	return 0x00;
}

static void device_stop_k053260(void* chip)
{
	k053260_state *ic = (k053260_state *)chip;
	
	free(ic->delta_table);
	free(ic->rom);
	free(ic);
	
	return;
}

INLINE void check_bounds( k053260_state *ic, UINT8 channel )
{

	UINT32 channel_start = ( ic->channels[channel].bank << 16 ) | ic->channels[channel].start;
	UINT32 channel_end = channel_start + ic->channels[channel].size - 1;

	if ( channel_start > ic->rom_size ) {
		logerror("K53260: Attempting to start playing past the end of the ROM ( start = %06x, end = %06x ).\n", channel_start, channel_end );

		ic->channels[channel].play = 0;

		return;
	}

	if ( channel_end > ic->rom_size ) {
		logerror("K53260: Attempting to play past the end of the ROM ( start = %06x, end = %06x ).\n", channel_start, channel_end );

		ic->channels[channel].size = ic->rom_size - channel_start;
	}
	if (LOG) logerror("K053260: Sample Start = %06x, Sample End = %06x, Sample rate = %04x, PPCM = %s\n", channel_start, channel_end, ic->channels[channel].rate, ic->channels[channel].ppcm ? "yes" : "no" );
}

static void k053260_w(void* chip, UINT8 offset, UINT8 data)
{
	k053260_state *ic = (k053260_state *)chip;
	UINT8 i, t;
	UINT8 r = offset;
	UINT8 v = data;

	if ( r > 0x2f ) {
		logerror("K053260: Writing past registers\n" );
		return;
	}

	/* before we update the regs, we need to check for a latched reg */
	if ( r == 0x28 ) {
		t = ic->regs[r] ^ v;

		for ( i = 0; i < 4; i++ ) {
			if ( t & ( 1 << i ) ) {
				if ( v & ( 1 << i ) ) {
					ic->channels[i].play = 1;
					ic->channels[i].pos = 0;
					ic->channels[i].ppcm_data = 0;
					check_bounds( ic, i );
				} else
					ic->channels[i].play = 0;
			}
		}

		ic->regs[r] = v;
		return;
	}

	/* update regs */
	ic->regs[r] = v;

	/* communication registers */
	if ( r < 8 )
		return;

	/* channel setup */
	if ( r < 0x28 ) {
		int channel = ( r - 8 ) / 8;

		switch ( ( r - 8 ) & 0x07 ) {
			case 0: /* sample rate low */
				ic->channels[channel].rate &= 0x0f00;
				ic->channels[channel].rate |= v;
			break;

			case 1: /* sample rate high */
				ic->channels[channel].rate &= 0x00ff;
				ic->channels[channel].rate |= ( v & 0x0f ) << 8;
			break;

			case 2: /* size low */
				ic->channels[channel].size &= 0xff00;
				ic->channels[channel].size |= v;
			break;

			case 3: /* size high */
				ic->channels[channel].size &= 0x00ff;
				ic->channels[channel].size |= v << 8;
			break;

			case 4: /* start low */
				ic->channels[channel].start &= 0xff00;
				ic->channels[channel].start |= v;
			break;

			case 5: /* start high */
				ic->channels[channel].start &= 0x00ff;
				ic->channels[channel].start |= v << 8;
			break;

			case 6: /* bank */
				ic->channels[channel].bank = v & 0xff;
			break;

			case 7: /* volume is 7 bits. Convert to 8 bits now. */
				ic->channels[channel].volume = ( ( v & 0x7f ) << 1 ) | ( v & 1 );
			break;
		}

		return;
	}

	switch( r ) {
		case 0x2a: /* loop, ppcm */
			for ( i = 0; i < 4; i++ )
				ic->channels[i].loop = ( v & ( 1 << i ) ) != 0;

			for ( i = 4; i < 8; i++ )
				ic->channels[i-4].ppcm = ( v & ( 1 << i ) ) != 0;
		break;

		case 0x2c: /* pan */
			ic->channels[0].pan = v & 7;
			ic->channels[1].pan = ( v >> 3 ) & 7;
		break;

		case 0x2d: /* more pan */
			ic->channels[2].pan = v & 7;
			ic->channels[3].pan = ( v >> 3 ) & 7;
		break;

		case 0x2f: /* control */
			ic->mode = v & 7;
			/* bit 0 = read ROM */
			/* bit 1 = enable sound output */
			/* bit 2 = unknown */
		break;
	}
}

static UINT8 k053260_r(void* chip, UINT8 offset)
{
	k053260_state *ic = (k053260_state *)chip;

	switch ( offset ) {
		case 0x29: /* channel status */
			{
				int i, status = 0;

				for ( i = 0; i < 4; i++ )
					status |= ic->channels[i].play << i;

				return status;
			}
		break;

		case 0x2e: /* read ROM */
			if ( ic->mode & 1 )
			{
				UINT32 offs = ic->channels[0].start + ( ic->channels[0].pos >> BASE_SHIFT ) + ( ic->channels[0].bank << 16 );

				ic->channels[0].pos += ( 1 << 16 );

				if ( offs > ic->rom_size ) {
					logerror("K53260: Attempting to read past ROM size in ROM Read Mode (offs = %06x, size = %06x).\n", offs,ic->rom_size );

					return 0;
				}

				return ic->rom[offs];
			}
		break;
	}

	return ic->regs[offset];
}

static void k053260_alloc_rom(void* chip, UINT32 memsize)
{
	k053260_state *info = (k053260_state *)chip;
	
	if (info->rom_size == memsize)
		return;
	
	info->rom = (UINT8*)realloc(info->rom, memsize);
	info->rom_size = memsize;
	memset(info->rom, 0xFF, memsize);
	
	return;
}

static void k053260_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	k053260_state *info = (k053260_state *)chip;
	
	if (offset > info->rom_size)
		return;
	if (offset + length > info->rom_size)
		length = info->rom_size - offset;
	
	memcpy(info->rom + offset, data, length);
	
	return;
}


static void k053260_set_mute_mask(void* chip, UINT32 MuteMask)
{
	k053260_state *info = (k053260_state *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
		info->channels[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
