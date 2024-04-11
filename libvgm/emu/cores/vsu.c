/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../RatioCntr.h"
#include "vsu.h"


static void vsu_stream_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_vsu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_vsu(void *info);
static void device_reset_vsu(void *info);

static void VSU_Write(void* info, UINT16 A, UINT8 V);

static void vsu_set_mute_mask(void *info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, VSU_Write},
	//{RWF_REGISTER | RWF_READ, DEVRW_A16D8, 0, NULL},	// read returns 0 in vbjin/Mednafen
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, vsu_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"VSU-VUE", "Mednafen", FCC_MEDN,
	
	device_start_vsu,
	device_stop_vsu,
	device_reset_vsu,
	vsu_stream_update,
	
	NULL,	// SetOptionBits
	vsu_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_VBoyVSU[] =
{
	&devDef,
	NULL
};


typedef struct
{
	DEV_DATA _devData;
	
	UINT8 IntlControl[6];
	UINT8 LeftLevel[6];
	UINT8 RightLevel[6];
	UINT16 Frequency[6];
	UINT16 EnvControl[6];	// Channel 5/6 extra functionality tacked on too.

	UINT8 RAMAddress[6];

	UINT8 SweepControl;

	UINT8 WaveData[5][0x20];

	UINT8 ModData[0x20];

	INT32 EffFreq[6];
	INT32 Envelope[6];

	UINT8 WavePos[6];
	UINT8 ModWavePos;

	INT32 LatcherClockDivider[6];

	INT32 FreqCounter[6];
	INT32 IntervalCounter[6];
	INT32 EnvelopeCounter[6];
	INT32 SweepModCounter;

	INT32 EffectsClockDivider[6];
	INT32 IntervalClockDivider[6];
	INT32 EnvelopeClockDivider[6];
	INT32 SweepModClockDivider;

	INT32 NoiseLatcherClockDivider;
	UINT8 NoiseLatcher;

	UINT32 lfsr;

	UINT32 clock;
	UINT32 smplrate;
	RATIO_CNTR cycleCntr;

	UINT8 Muted[6];
} vsu_state;

static void VSU_Power(vsu_state* chip);

INLINE void VSU_CalcCurrentOutput(vsu_state* chip, int ch, DEV_SMPL* left, DEV_SMPL* right);
static void VSU_Update(vsu_state* chip, UINT32 clocks, DEV_SMPL* outleft, DEV_SMPL* outright);


static const int Tap_LUT[8] = { 15 - 1, 11 - 1, 14 - 1, 5 - 1, 9 - 1, 7 - 1, 10 - 1, 12 - 1 };

static void VSU_Power(vsu_state* chip)
{
	int ch;
	
	chip->SweepControl = 0;
	chip->SweepModCounter = 0;
	chip->SweepModClockDivider = 1;

	for(ch = 0; ch < 6; ch++)
	{
		chip->IntlControl[ch] = 0;
		chip->LeftLevel[ch] = 0;
		chip->RightLevel[ch] = 0;
		chip->Frequency[ch] = 0;
		chip->EnvControl[ch] = 0;
		chip->RAMAddress[ch] = 0;

		chip->EffFreq[ch] = 0;
		chip->Envelope[ch] = 0;
		chip->WavePos[ch] = 0;
		chip->FreqCounter[ch] = 0;
		chip->IntervalCounter[ch] = 0;
		chip->EnvelopeCounter[ch] = 0;

		chip->EffectsClockDivider[ch] = 4800;
		chip->IntervalClockDivider[ch] = 4;
		chip->EnvelopeClockDivider[ch] = 4;

		chip->LatcherClockDivider[ch] = 120;
	}


	chip->NoiseLatcherClockDivider = 120;
	chip->NoiseLatcher = 0;

	memset(chip->WaveData, 0, sizeof(chip->WaveData));
	memset(chip->ModData, 0, sizeof(chip->ModData));

	RC_RESET(&chip->cycleCntr);
}

static void VSU_Write(void* info, UINT16 A, UINT8 V)
{
	vsu_state* chip = (vsu_state*)info;
	
	A <<= 2;
	A &= 0x7FF;

	//printf("VSU Write: %d, %08x %02x\n", timestamp, A, V);

	if(A < 0x280)
		chip->WaveData[A >> 7][(A >> 2) & 0x1F] = V & 0x3F;
	else if(A < 0x400)
	{
		//if(A >= 0x300)
		// printf("Modulation mirror write? %08x %02x\n", A, V);
		chip->ModData[(A >> 2) & 0x1F] = V;
	}
	else if(A < 0x600)
	{
		int ch = (A >> 6) & 0xF;

		//if(ch < 6)
		//printf("Ch: %d, Reg: %d, Value: %02x\n", ch, (A >> 2) & 0xF, V);

		if(ch > 5)
		{
			if(A == 0x580 && (V & 1))
			{
				int i;
				//puts("STOP, HAMMER TIME");
				for(i = 0; i < 6; i++)
					chip->IntlControl[i] &= ~0x80;
			}
		}
		else
			switch((A >> 2) & 0xF)
			{
			case 0x0:
				chip->IntlControl[ch] = V & ~0x40;

				if(V & 0x80)
				{
					chip->EffFreq[ch] = chip->Frequency[ch];
					if(ch == 5)
						chip->FreqCounter[ch] = 10 * (2048 - chip->EffFreq[ch]);
					else
						chip->FreqCounter[ch] = 2048 - chip->EffFreq[ch];
					chip->IntervalCounter[ch] = (V & 0x1F) + 1;
					chip->EnvelopeCounter[ch] = (chip->EnvControl[ch] & 0x7) + 1;

					if(ch == 4)
					{
						chip->SweepModCounter = (chip->SweepControl >> 4) & 7;
						chip->SweepModClockDivider = (chip->SweepControl & 0x80) ? 8 : 1;
						chip->ModWavePos = 0;
					}

					chip->WavePos[ch] = 0;

					if(ch == 5)
						chip->lfsr = 1;

					//if(!(chip->IntlControl[ch] & 0x80))
					// chip->Envelope[ch] = (chip->EnvControl[ch] >> 4) & 0xF;

					chip->EffectsClockDivider[ch] = 4800;
					chip->IntervalClockDivider[ch] = 4;
					chip->EnvelopeClockDivider[ch] = 4;
				}
				break;

			case 0x1:
				chip->LeftLevel[ch] = (V >> 4) & 0xF;
				chip->RightLevel[ch] = (V >> 0) & 0xF;
				break;

			case 0x2:
				chip->Frequency[ch] &= 0xFF00;
				chip->Frequency[ch] |= V << 0;
				chip->EffFreq[ch] &= 0xFF00;
				chip->EffFreq[ch] |= V << 0;
				break;

			case 0x3:
				chip->Frequency[ch] &= 0x00FF;
				chip->Frequency[ch] |= (V & 0x7) << 8;
				chip->EffFreq[ch] &= 0x00FF;
				chip->EffFreq[ch] |= (V & 0x7) << 8;
				break;

			case 0x4:
				chip->EnvControl[ch] &= 0xFF00;
				chip->EnvControl[ch] |= V << 0;

				chip->Envelope[ch] = (V >> 4) & 0xF;
				break;

			case 0x5:
				chip->EnvControl[ch] &= 0x00FF;
				if(ch == 4)
					chip->EnvControl[ch] |= (V & 0x73) << 8;
				else if(ch == 5)
					chip->EnvControl[ch] |= (V & 0x73) << 8;
				else
					chip->EnvControl[ch] |= (V & 0x03) << 8;
				break;

			case 0x6:
				chip->RAMAddress[ch] = V & 0xF;
				break;

			case 0x7:
				if(ch == 4)
				{
					chip->SweepControl = V;
				}
				break;
			}
	}
}

INLINE void VSU_CalcCurrentOutput(vsu_state* chip, int ch, DEV_SMPL* left, DEV_SMPL* right)
{
	INT16 WD;
	int l_ol, r_ol;

	if(!(chip->IntlControl[ch] & 0x80) || chip->Muted[ch])
		return;

	if(ch == 5)
		WD = chip->NoiseLatcher;
	else
	{
		if(chip->RAMAddress[ch] > 4)
			WD = 0x20;
		else
			WD = chip->WaveData[chip->RAMAddress[ch]][chip->WavePos[ch]];
	}
	l_ol = chip->Envelope[ch] * chip->LeftLevel[ch];
	if(l_ol)
	{
		l_ol >>= 3;
		l_ol += 1;
	}

	r_ol = chip->Envelope[ch] * chip->RightLevel[ch];
	if(r_ol)
	{
		r_ol >>= 3;
		r_ol += 1;
	}

	WD -= 0x20;
	(*left) += WD * l_ol;
	(*right) += WD * r_ol;
	return;
}

static void VSU_Update(vsu_state* chip, UINT32 clocks, DEV_SMPL* outleft, DEV_SMPL* outright)
{
	int ch;

	*outleft = 0;
	*outright = 0;
	
	//puts("VSU Start");
	for(ch = 0; ch < 6; ch++)
	{
		INT32 rem_clocks = (INT32)clocks;

		if(!(chip->IntlControl[ch] & 0x80) || chip->Muted[ch])
			continue;	// channel disabled - don't add anything to output

		while(rem_clocks > 0)
		{
			INT32 chunk_clocks = rem_clocks;

			if(chunk_clocks > chip->EffectsClockDivider[ch])
				chunk_clocks = chip->EffectsClockDivider[ch];

			if(ch == 5)
			{
				if(chunk_clocks > chip->NoiseLatcherClockDivider)
					chunk_clocks = chip->NoiseLatcherClockDivider;
			}
			else
			{
				if(chip->EffFreq[ch] >= 2040)
				{
					if(chunk_clocks > chip->LatcherClockDivider[ch])
						chunk_clocks = chip->LatcherClockDivider[ch];
				}
				else
				{
					if(chunk_clocks > chip->FreqCounter[ch])
						chunk_clocks = chip->FreqCounter[ch];
				}
			}

			if(ch == 5 && chunk_clocks > chip->NoiseLatcherClockDivider)
				chunk_clocks = chip->NoiseLatcherClockDivider;

			chip->FreqCounter[ch] -= chunk_clocks;
			while(chip->FreqCounter[ch] <= 0)
			{
				if(ch == 5)
				{
					int feedback = ((chip->lfsr >> 7) & 1) ^ ((chip->lfsr >> Tap_LUT[(chip->EnvControl[5] >> 12) & 0x7]) & 1);
					chip->lfsr = ((chip->lfsr << 1) & 0x7FFF) | feedback;

					chip->FreqCounter[ch] += 10 * (2048 - chip->EffFreq[ch]);
				}
				else
				{
					chip->FreqCounter[ch] += 2048 - chip->EffFreq[ch];
					chip->WavePos[ch] = (chip->WavePos[ch] + 1) & 0x1F;
				}
			}

			chip->LatcherClockDivider[ch] -= chunk_clocks;
			while(chip->LatcherClockDivider[ch] <= 0)
				chip->LatcherClockDivider[ch] += 120;

			if(ch == 5)
			{
				chip->NoiseLatcherClockDivider -= chunk_clocks;
				if(!chip->NoiseLatcherClockDivider)
				{
					chip->NoiseLatcherClockDivider = 120;
					chip->NoiseLatcher = ((chip->lfsr & 1) << 6) - (chip->lfsr & 1);	// 0x3F or 0x00
				}
			}

			chip->EffectsClockDivider[ch] -= chunk_clocks;
			while(chip->EffectsClockDivider[ch] <= 0)
			{
				chip->EffectsClockDivider[ch] += 4800;

				chip->IntervalClockDivider[ch]--;
				while(chip->IntervalClockDivider[ch] <= 0)
				{
					chip->IntervalClockDivider[ch] += 4;

					if(chip->IntlControl[ch] & 0x20)
					{
						chip->IntervalCounter[ch]--;
						if(!chip->IntervalCounter[ch])
						{
							chip->IntlControl[ch] &= ~0x80;
						}
					}

					chip->EnvelopeClockDivider[ch]--;
					while(chip->EnvelopeClockDivider[ch] <= 0)
					{
						chip->EnvelopeClockDivider[ch] += 4;

						if(chip->EnvControl[ch] & 0x0100)			// Enveloping enabled?
						{
							chip->EnvelopeCounter[ch]--;
							if(!chip->EnvelopeCounter[ch])
							{
								chip->EnvelopeCounter[ch] = (chip->EnvControl[ch] & 0x7) + 1;

								if(chip->EnvControl[ch] & 0x0008)	// Grow
								{
									if(chip->Envelope[ch] < 0xF || (chip->EnvControl[ch] & 0x200))
										chip->Envelope[ch] = (chip->Envelope[ch] + 1) & 0xF;
								}
								else						// Decay
								{
									if(chip->Envelope[ch] > 0 || (chip->EnvControl[ch] & 0x200))
										chip->Envelope[ch] = (chip->Envelope[ch] - 1) & 0xF;
								}
							}
						}

					} // end while(chip->EnvelopeClockDivider[ch] <= 0)
				} // end while(chip->IntervalClockDivider[ch] <= 0)

				if(ch == 4)
				{
					chip->SweepModClockDivider--;
					while(chip->SweepModClockDivider <= 0)
					{
						chip->SweepModClockDivider += (chip->SweepControl & 0x80) ? 8 : 1;

						if(((chip->SweepControl >> 4) & 0x7) && (chip->EnvControl[ch] & 0x4000))
						{
							if(chip->SweepModCounter)
								chip->SweepModCounter--;

							if(!chip->SweepModCounter)
							{
								chip->SweepModCounter = (chip->SweepControl >> 4) & 0x7;

								if(chip->EnvControl[ch] & 0x1000)	// Modulation
								{
									if(chip->ModWavePos < 32 || (chip->EnvControl[ch] & 0x2000))
									{
										chip->ModWavePos &= 0x1F;

										chip->EffFreq[ch] = (chip->EffFreq[ch] + (INT8)chip->ModData[chip->ModWavePos]);
										if(chip->EffFreq[ch] < 0)
										{
											//puts("Underflow");
											chip->EffFreq[ch] = 0;
										}
										else if(chip->EffFreq[ch] > 0x7FF)
										{
											//puts("Overflow");
											chip->EffFreq[ch] = 0x7FF;
										}
										chip->ModWavePos++;
									}
									//puts("Mod");
								}
								else						// Sweep
								{
									INT32 delta = chip->EffFreq[ch] >> (chip->SweepControl & 0x7);
									INT32 NewFreq = chip->EffFreq[ch] + ((chip->SweepControl & 0x8) ? delta : -delta);

									//printf("Sweep(%d): Old: %d, New: %d\n", ch, EffFreq[ch], NewFreq);

									if(NewFreq < 0)
										chip->EffFreq[ch] = 0;
									else if(NewFreq > 0x7FF)
									{
										//chip->EffFreq[ch] = 0x7FF;
										chip->IntlControl[ch] &= ~0x80;
									}
									else
										chip->EffFreq[ch] = NewFreq;
								}
							}
						}
					} // end while(chip->SweepModClockDivider <= 0)
				} // end if(ch == 4)
			} // end while(chip->EffectsClockDivider[ch] <= 0)
			rem_clocks -= chunk_clocks;
		}

		VSU_CalcCurrentOutput(chip, ch, outleft, outright);
	}
	//puts("VSU End");
}


static void vsu_stream_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	vsu_state* chip = (vsu_state*)param;
	UINT32 curSmpl;
	
	for (curSmpl = 0; curSmpl < samples; curSmpl ++)
	{
		RC_STEP(&chip->cycleCntr);
		VSU_Update(chip, RC_GET_VAL(&chip->cycleCntr), &outputs[0][curSmpl], &outputs[1][curSmpl]);
		RC_MASK(&chip->cycleCntr);
		
		// Volume per channel: 0x1F (envelope/volume) * 0x3F (unsigned sample) = 0x7A1 (~0x800)
		// I turned the samples into signed format (-0x20..0x1F), so the used range is +-0x400.
		// 16-bit values are up to 0x8000
		// 0x8000 / 0x400 / 6 = 5.33 (possible boost without clipping)
		// Because music usually doesn't use the maximum volume (SFX do), I boost by 2^3 = 8.
		outputs[0][curSmpl] <<= 3;
		outputs[1][curSmpl] <<= 3;
	}
	return;
}

static UINT8 device_start_vsu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	vsu_state* chip;
	
	chip = (vsu_state*)calloc(1, sizeof(vsu_state));
	if (chip == NULL)
		return 0xFF;
	
	chip->clock = cfg->clock;
	// sample rate according to https://github.com/emu-rs/rustual-boy/blob/master/rustual-boy-core/src/vsu/mod.rs
	// 20 MHz / 480 = 41.667 Hz, VGMs use 5 MHz / 120
	chip->smplrate = chip->clock / 120;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, chip->smplrate, cfg->smplRate);
	
	RC_SET_RATIO(&chip->cycleCntr, cfg->clock, chip->smplrate);
	
	vsu_set_mute_mask(chip, 0x00);
	
	chip->_devData.chipInf = chip;
	INIT_DEVINF(retDevInf, &chip->_devData, chip->smplrate, &devDef);
	
	return 0x00;
}

static void device_stop_vsu(void* info)
{
	vsu_state* chip = (vsu_state*)info;
	
	free(chip);
	
	return;
}

static void device_reset_vsu(void* info)
{
	vsu_state* chip = (vsu_state*)info;
	
	VSU_Power(chip);
	
	return;
}

static void vsu_set_mute_mask(void* info, UINT32 MuteMask)
{
	vsu_state* chip = (vsu_state*)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 6; CurChn ++)
		chip->Muted[CurChn] = (MuteMask >> CurChn) & 0x01;
	
	return;
}
