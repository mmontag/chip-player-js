// license:BSD-3-Clause
// copyright-holders:Hiromitsu Shioya, Olivier Galibert
/*********************************************************/
/*    SEGA 16ch 8bit PCM                                 */
/*********************************************************/

#include <stdlib.h>
#include <string.h>	// for memset

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"

#include "segapcm.h"

static void SEGAPCM_update(void *chip, UINT32 samples, DEV_SMPL **outputs);

static UINT8 device_start_segapcm(const SEGAPCM_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_segapcm(void *chip);
static void device_reset_segapcm(void *chip);

static void sega_pcm_w(void *chip, UINT16 offset, UINT8 data);
static UINT8 sega_pcm_r(void *chip, UINT16 offset);
static void sega_pcm_alloc_rom(void *chip, UINT32 memsize);
static void sega_pcm_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);
#ifdef _DEBUG
static void sega_pcm_fwrite_romusage(void *chip);
#endif

static void segapcm_set_mute_mask(void *chip, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, sega_pcm_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D8, 0, sega_pcm_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, sega_pcm_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, sega_pcm_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, segapcm_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"Sega PCM", "MAME", FCC_MAME,
	
	(DEVFUNC_START)device_start_segapcm,
	device_stop_segapcm,
	device_reset_segapcm,
	SEGAPCM_update,
	
	NULL,	// SetOptionBits
	segapcm_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_SegaPCM[] =
{
	&devDef,
	NULL
};


typedef struct _segapcm_state segapcm_state;
struct _segapcm_state
{
	DEV_DATA _devData;

	UINT8  *ram;
	UINT8 low[16];
	UINT32 ROMSize;
	UINT8 *rom;
#ifdef _DEBUG
	UINT8 *romusage;
#endif
	UINT8 bankshift;
	UINT8 bankmask;
	UINT8 intf_mask;
	UINT8 Muted[16];
};

static void SEGAPCM_update(void *chip, UINT32 samples, DEV_SMPL **outputs)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	int ch;

	/* clear the buffers */
	memset(outputs[0], 0, samples*sizeof(DEV_SMPL));
	memset(outputs[1], 0, samples*sizeof(DEV_SMPL));
	if (spcm->rom == NULL)
		return;

	// reg      function
	// ------------------------------------------------
	// 0x00     ?
	// 0x01     ?
	// 0x02     volume left
	// 0x03     volume right
	// 0x04     loop address (08-15)
	// 0x05     loop address (16-23)
	// 0x06     end address
	// 0x07     address delta
	// 0x80     ?
	// 0x81     ?
	// 0x82     ?
	// 0x83     ?
	// 0x84     current address (08-15), 00-07 is internal?
	// 0x85     current address (16-23)
	// 0x86     bit 0: channel disable?
	//          bit 1: loop disable
	//          other bits: bank
	// 0x87     ?

	/* loop over channels */
	for (ch = 0; ch < 16; ch++)
	{
		UINT8 *regs = spcm->ram+8*ch;

		/* only process active channels */
		if (!(regs[0x86] & 1) && ! spcm->Muted[ch])
		{
			UINT32 offset = (regs[0x86] & spcm->bankmask) << spcm->bankshift;
			UINT32 addr = (regs[0x85] << 16) | (regs[0x84] << 8) | spcm->low[ch];
			UINT32 loop = (regs[0x05] << 16) | (regs[0x04] << 8);
			UINT8 end = regs[6] + 1;
			UINT32 i;

			/* loop over samples on this channel */
			for (i = 0; i < samples; i++)
			{
				INT8 v = 0;

				/* handle looping if we've hit the end */
				if ((addr >> 16) == end)
				{
					if (regs[0x86] & 2)
					{
						regs[0x86] |= 1;
						break;
					}
					else addr = loop;
				}

				/* fetch the sample */
				v = spcm->rom[offset | (addr >> 8)] - 0x80;
#ifdef _DEBUG
				if ((spcm->romusage[(offset | addr >> 8)] & 0x03) == 0x02 && (regs[2] || regs[3]))
					printf("Access to empty ROM section! (0x%06X)\n", offset | ((addr >> 8)));
				spcm->romusage[offset | (addr >> 8)] |= 0x01;
#endif

				/* apply panning and advance */
				// fixed Bitmask for volume multiplication, thanks to ctr -Valley Bell
				outputs[0][i] += v * (regs[2] & 0x7F);
				outputs[1][i] += v * (regs[3] & 0x7F);
				addr = (addr + regs[7]) & 0xffffff;
			}

			/* store back the updated address */
			regs[0x84] = addr >> 8;
			regs[0x85] = addr >> 16;
			spcm->low[ch] = regs[0x86] & 1 ? 0 : addr;
		}
	}
}

static UINT8 device_start_segapcm(const SEGAPCM_CFG* cfg, DEV_INFO* retDevInf)
{
	static const UINT32 STD_ROM_SIZE = 0x80000;
	segapcm_state *spcm;
	
	spcm = (segapcm_state *)calloc(1, sizeof(segapcm_state));
	spcm->bankshift = cfg->bnkshift;
	spcm->intf_mask = cfg->bnkmask;
	if (! spcm->intf_mask)
		spcm->intf_mask = SEGAPCM_BANK_MASK7;
	spcm->bankmask = spcm->intf_mask & (0x1fffff >> spcm->bankshift);
	
	spcm->ROMSize = 0;
	spcm->rom = NULL;
#ifdef _DEBUG
	spcm->romusage = NULL;
#endif
	spcm->ram = (UINT8*)malloc(0x800);
	// RAM clear is done at device_reset
	
	sega_pcm_alloc_rom(spcm, STD_ROM_SIZE);
	
	segapcm_set_mute_mask(spcm, 0x0000);
	
	spcm->_devData.chipInf = spcm;
	INIT_DEVINF(retDevInf, &spcm->_devData, cfg->_genCfg.clock / 128, &devDef);
	return 0x00;
}

void device_stop_segapcm(void *chip)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	free(spcm->rom);	spcm->rom = NULL;
#ifdef _DEBUG
	//sega_pcm_fwrite_romusage(spcm);
	free(spcm->romusage);
#endif
	free(spcm->ram);
	free(spcm);
	
	return;
}

static void device_reset_segapcm(void *chip)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	
	memset(spcm->ram, 0xFF, 0x800);
	
	return;
}


static void sega_pcm_w(void *chip, UINT16 offset, UINT8 data)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	
	spcm->ram[offset & 0x07ff] = data;
}

static UINT8 sega_pcm_r(void *chip, UINT16 offset)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	return spcm->ram[offset & 0x07ff];
}

static void sega_pcm_alloc_rom(void *chip, UINT32 memsize)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	
	if (spcm->ROMSize == memsize)
		return;
	
	spcm->rom = (UINT8*)realloc(spcm->rom, memsize);
#ifndef _DEBUG
	//memset(spcm->rom, 0xFF, memsize);
	// filling 0xFF would actually be more true to the hardware,
	// (unused ROMs have all FFs)
	// but 0x80 is the effective 'null' byte
	memset(spcm->rom, 0x80, memsize);
#else
	spcm->romusage = (UINT8*)realloc(spcm->romusage, memsize);
	// filling with FF makes it easier to find bugs in a .wav-log
	memset(spcm->rom, 0xFF, memsize);
	memset(spcm->romusage, 0x02, memsize);
#endif
	spcm->ROMSize = memsize;
	
	spcm->bankmask = spcm->intf_mask & (0x1fffff >> spcm->bankshift);
	
	return;
}

static void sega_pcm_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	
	if (offset > spcm->ROMSize)
		return;
	if (offset + length > spcm->ROMSize)
		length = spcm->ROMSize - offset;
	
	memcpy(&spcm->rom[offset], data, length);
#ifdef _DEBUG
	memset(&spcm->romusage[offset], 0x00, length);
#endif
	
	return;
}


#ifdef _DEBUG
static void sega_pcm_fwrite_romusage(void *chip)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	FILE* hFile;
	
	hFile = fopen("SPCM_ROMUsage.bin", "wb");
	if (hFile == NULL)
		return;
	
	fwrite(spcm->romusage, 0x01, spcm->ROMSize, hFile);
	
	fclose(hFile);
	return;
}
#endif

static void segapcm_set_mute_mask(void *chip, UINT32 MuteMask)
{
	segapcm_state *spcm = (segapcm_state *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 16; CurChn ++)
		spcm->Muted[CurChn] = (MuteMask >> CurChn) & 0x01;
	
	return;
}
