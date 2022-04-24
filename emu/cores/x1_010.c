// license:BSD-3-Clause
// copyright-holders:Luca Elia
/***************************************************************************

                            -= Seta Hardware =-

                    driver by   Luca Elia (l.elia@tin.it)

                    rewrite by Manbow-J(manbowj@hamal.freemail.ne.jp)

                    X1-010 Seta Custom Sound Chip (80 Pin PQFP)

 Custom programmed Mitsubishi M60016 Gate Array, 3608 gates, 148 Max I/O ports

    The X1-010 is a 16-Voice sound generator, each channel gets its
    waveform from RAM (128 bytes per waveform, 8 bit signed data)
    or sampling PCM (8 bit signed data).

Registers:
    8 registers per channel (mapped to the lower bytes of 16 words on the 68K)

    Reg:    Bits:       Meaning:

    0       7--- ----   Frequency divider flag (only downtown seems to set this)
            -654 3---
            ---- -2--   PCM/Waveform repeat flag (0:Once 1:Repeat) (*1)
            ---- --1-   Sound out select (0:PCM 1:Waveform)
            ---- ---0   Key on / off

    1       7654 ----   PCM Volume 1 (L?)
            ---- 3210   PCM Volume 2 (R?)
                        Waveform No.

    2                   PCM Frequency (4.4 fixed point)
                        Waveform Pitch Lo (6.10 fixed point)

    3                   Waveform Pitch Hi (6.10 fixed point)

    4                   PCM Sample Start / 0x1000           [Start/End in bytes]
                        Waveform Envelope Time (.10 fixed point)

    5                   PCM Sample End 0x100 - (Sample End / 0x1000)    [PCM ROM is Max 1MB?]
                        Waveform Envelope No.
    6                   Reserved
    7                   Reserved

    offset 0x0000 - 0x007f  Channel data
    offset 0x0080 - 0x0fff  Envelope data
    offset 0x1000 - 0x1fff  Wave form data

    *1 : when 0 is specified, hardware interrupt is caused (always return soon)

***************************************************************************/

#include <stdlib.h>
#include <stddef.h>	// for NULL
#include <string.h>	// for memset

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../logging.h"
#include "x1_010.h"


static void seta_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_x1_010(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_x1_010(void *chip);
static void device_reset_x1_010(void *chip);

static UINT8 seta_sound_r(void *chip, UINT16 offset);
static void seta_sound_w(void *chip, UINT16 offset, UINT8 data);

static void x1_010_alloc_rom(void* info, UINT32 memsize);
static void x1_010_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);

static void x1_010_set_mute_mask(void *chip, UINT32 MuteMask);
static void x1_010_set_log_cb(void *chip, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, seta_sound_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D8, 0, seta_sound_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, x1_010_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, x1_010_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, x1_010_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"X1-010", "MAME", FCC_MAME,
	
	device_start_x1_010,
	device_stop_x1_010,
	device_reset_x1_010,
	seta_update,
	
	NULL,	// SetOptionBits
	x1_010_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	x1_010_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_X1_010[] =
{
	&devDef,
	NULL
};


#define VERBOSE_SOUND 0
#define VERBOSE_REGISTER_WRITE 0
#define VERBOSE_REGISTER_READ 0

//#define LOG_SOUND(x) do { if (VERBOSE_SOUND) logerror x; } while (0)
#define LOG_REGISTER_WRITE(x) do { if (VERBOSE_REGISTER_WRITE) logerror x; } while (0)
#define LOG_REGISTER_READ(x) do { if (VERBOSE_REGISTER_READ) logerror x; } while (0)

#define SETA_NUM_CHANNELS 16

//#define FREQ_BASE_BITS          8                 // Frequency fixed decimal shift bits
#define FREQ_BASE_BITS        11                // using UINT32, 20 bits (1 MB) of address space -> 32-20=12 bits left, -1 for position overflow checking
#define ENV_BASE_BITS        16                 // wave form envelope fixed decimal shift bits
#define VOL_BASE    (2*32*256/30)               // Volume base

/* this structure defines the parameters for a channel */
typedef struct {
	UINT8   status;
	UINT8   volume;                     //        volume / wave form no.
	UINT8   frequency;                  //     frequency / pitch lo
	UINT8   pitch_hi;                   //      reserved / pitch hi
	UINT8   start;                      // start address / envelope time
	UINT8   end;                        //   end address / envelope no.
	UINT8   reserve[2];
} X1_010_CHANNEL;

typedef struct _x1_010_state x1_010_state;
struct _x1_010_state
{
	DEV_DATA _devData;
	DEV_LOGGER logger;
	
	/* Variables only used here */
	UINT32 ROMSize;
	UINT8* rom;                             // ROM
	UINT32 rate;                            // Output sampling rate (Hz)
//	UINT16 adr;                             // address
//	UINT8 sound_enable;                     // sound output enable/disable
	UINT8   reg[0x2000];                    // X1-010 Register & wave form area
//	UINT8   HI_WORD_BUF[0x2000];            // X1-010 16bit access ram check avoidance work
	UINT32  smp_offset[SETA_NUM_CHANNELS];
	UINT32  env_offset[SETA_NUM_CHANNELS];

	UINT32 base_clock;

	UINT8 Muted[SETA_NUM_CHANNELS];
};


/*--------------------------------------------------------------
 generate sound to the mix buffer
--------------------------------------------------------------*/
static void seta_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	x1_010_state *info = (x1_010_state *)param;
	X1_010_CHANNEL  *reg;
	UINT32  ch;
	UINT32  i;
	int     volL, volR, freq, div;
	INT8    *start, *end, data;
	UINT8   *env;
	UINT32  smp_offs, smp_step, env_offs, env_step, delta;
	DEV_SMPL *bufL = outputs[0];
	DEV_SMPL *bufR = outputs[1];

	// mixer buffer zero clear
	memset( outputs[0], 0, samples*sizeof(*outputs[0]) );
	memset( outputs[1], 0, samples*sizeof(*outputs[1]) );
	if (info->rom == NULL)
		return;

//	if( info->sound_enable == 0 ) return;

	for( ch = 0; ch < SETA_NUM_CHANNELS; ch++ ) {
		reg = (X1_010_CHANNEL *)&(info->reg[ch*sizeof(X1_010_CHANNEL)]);
		if( (reg->status&1) != 0 && ! info->Muted[ch]) {        // Key On
			div = (reg->status&0x80) ? 1 : 0;
			if( (reg->status&2) == 0 ) {                        // PCM sampling
				start    = (INT8 *)(info->rom + reg->start*0x1000);
				end      = (INT8 *)(info->rom + (0x100-reg->end)*0x1000);
				volL     = ((reg->volume>>4)&0xf)*VOL_BASE;
				volR     = ((reg->volume>>0)&0xf)*VOL_BASE;
				smp_offs = info->smp_offset[ch];
				freq     = reg->frequency>>div;
				// Meta Fox does write the frequency register, but this is a hack to make it "work" with the current setup
				// This is broken for Arbalester (it writes 8), but that'll be fixed later.
				if( freq == 0 ) freq = 4;
				smp_step = (UINT32)((float)info->base_clock/8192.0f
							*freq*(1<<FREQ_BASE_BITS)/(float)info->rate+0.5f);
				if( smp_offs == 0 && VERBOSE_SOUND ) {
					emu_logf(&info->logger, DEVLOG_TRACE, "Play sample %p - %p, channel %X volume %d:%d freq %X step %X offset %X\n",
						start, end, ch, volL, volR, freq, smp_step, smp_offs );
				}
				for( i = 0; i < samples; i++ ) {
					delta = smp_offs>>FREQ_BASE_BITS;
					// sample ended?
					if( start+delta >= end ) {
						reg->status &= ~0x01;                   // Key off
						break;
					}
					data = start[delta];
					bufL[i] += (data*volL/256);
					bufR[i] += (data*volR/256);
					smp_offs += smp_step;
				}
				info->smp_offset[ch] = smp_offs;
			} else {                                            // Wave form
				start    = (INT8 *)&(info->reg[reg->volume*128+0x1000]);
				smp_offs = info->smp_offset[ch];
				freq     = ((reg->pitch_hi<<8)+reg->frequency)>>div;
				smp_step = (UINT32)((float)info->base_clock/128.0/1024.0/4.0*freq*(1<<FREQ_BASE_BITS)/(float)info->rate+0.5f);

				env      = (UINT8 *)&(info->reg[reg->end*128]);
				env_offs = info->env_offset[ch];
				env_step = (UINT32)((float)info->base_clock/128.0/1024.0/4.0*reg->start*(1<<ENV_BASE_BITS)/(float)info->rate+0.5f);
				/* Print some more debug info */
				if( smp_offs == 0 && VERBOSE_SOUND ) {
					emu_logf(&info->logger, DEVLOG_TRACE, "Play waveform %X, channel %X volume %X freq %4X step %X offset %X\n",
						reg->volume, ch, reg->end, freq, smp_step, smp_offs );
				}
				for( i = 0; i < samples; i++ ) {
					int vol;
					delta = env_offs>>ENV_BASE_BITS;
					// Envelope one shot mode
					if( (reg->status&4) != 0 && delta >= 0x80 ) {
						reg->status &= ~0x01;                   // Key off
						break;
					}
					vol = env[delta&0x7f];
					volL = ((vol>>4)&0xf)*VOL_BASE;
					volR = ((vol>>0)&0xf)*VOL_BASE;
					data  = start[(smp_offs>>FREQ_BASE_BITS)&0x7f];
					bufL[i] += (data*volL/256);
					bufR[i] += (data*volR/256);
					smp_offs += smp_step;
					env_offs += env_step;
				}
				info->smp_offset[ch] = smp_offs;
				info->env_offset[ch] = env_offs;
			}
		}
	}
}



static UINT8 device_start_x1_010(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	x1_010_state *info;

	info = (x1_010_state *)calloc(1, sizeof(x1_010_state));
	if (info == NULL)
		return 0xFF;

	info->base_clock    = cfg->clock;
	info->rate          = info->base_clock / 512;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, info->rate, cfg->smplRate);

	info->ROMSize       = 0x00;
	info->rom           = NULL;

	/* Print some more debug info */
	//LOG_SOUND(("masterclock = %d rate = %d\n", info->base_clock, info->rate ));

	x1_010_set_mute_mask(info, 0x0000);

	info->_devData.chipInf = info;
	INIT_DEVINF(retDevInf, &info->_devData, info->rate, &devDef);

	return 0x00;
}

static void device_stop_x1_010(void *chip)
{
	x1_010_state *info = (x1_010_state *)chip;
	
	free(info->rom);
	free(info);
	
	return;
}

static void device_reset_x1_010(void *chip)
{
	x1_010_state *info = (x1_010_state *)chip;
	
	memset(info->reg, 0, 0x2000);
//	memset(HI_WORD_BUF, 0, sizeof(m_HI_WORD_BUF));
	memset(info->smp_offset, 0, SETA_NUM_CHANNELS * sizeof(UINT32));
	memset(info->env_offset, 0, SETA_NUM_CHANNELS * sizeof(UINT32));
	
	return;
}


#if 0
static void x1_010_enable_w(void *chip, UINT8 data)
{
	x1_010_state *info = (x1_010_state *)chip;
	info->sound_enable = data;
}
#endif

static UINT8 seta_sound_r(void *chip, UINT16 offset)
{
	x1_010_state *info = (x1_010_state *)chip;
	//offset ^= info->adr;
	return info->reg[offset];
}

static void seta_sound_w(void *chip, UINT16 offset, UINT8 data)
{
	x1_010_state *info = (x1_010_state *)chip;
	int channel, reg;
	//offset ^= info->adr;

	channel = offset/sizeof(X1_010_CHANNEL);
	reg     = offset%sizeof(X1_010_CHANNEL);

	if( channel < SETA_NUM_CHANNELS && reg == 0
		&& (info->reg[offset]&1) == 0 && (data&1) != 0 ) {
		info->smp_offset[channel] = 0;
		info->env_offset[channel] = 0;
	}
	//LOG_REGISTER_WRITE(("%s: offset %6X : data %2X\n", machine().describe_context(), offset, data ));
	info->reg[offset] = data;
}

static void x1_010_alloc_rom(void* chip, UINT32 memsize)
{
	x1_010_state *info = (x1_010_state *)chip;
	
	if (info->ROMSize == memsize)
		return;
	
	info->rom = (UINT8*)realloc(info->rom, memsize);
	info->ROMSize = memsize;
	memset(info->rom, 0xFF, memsize);
	
	return;
}

static void x1_010_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	x1_010_state *info = (x1_010_state *)chip;
	
	if (offset > info->ROMSize)
		return;
	if (offset + length > info->ROMSize)
		length = info->ROMSize - offset;
	
	memcpy(info->rom + offset, data, length);
	
	return;
}


static void x1_010_set_mute_mask(void *chip, UINT32 MuteMask)
{
	x1_010_state *info = (x1_010_state *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < SETA_NUM_CHANNELS; CurChn ++)
		info->Muted[CurChn] = (MuteMask >> CurChn) & 0x01;
	
	return;
}

static void x1_010_set_log_cb(void *chip, DEVCB_LOG func, void* param)
{
	x1_010_state *info = (x1_010_state *)chip;
	dev_logger_set(&info->logger, info, func, param);
	return;
}
