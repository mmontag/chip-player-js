// TODO: SCSP and (especially) WonderSwan
 /************************
  *  DAC Stream Control  *
  ***********************/
// (Custom Driver to handle PCM Streams of YM2612 DAC and PWM.)
//
// Initially written on 3 February 2011 by Valley Bell

/* How it basically works:

1. send command X with data Y at frequency F to chip C
2. do that until you receive a STOP command, or until you sent N commands

*/

#include <stdlib.h>
#include <string.h>
#include <stddef.h>	// for NULL

#include "../stdtype.h"
#include "EmuStructs.h"
#include "EmuHelper.h"	// for INIT_DEVINF
#include "snddef.h"
#include "SoundDevs.h"
#include "SoundEmu.h"
#include "RatioCntr.h"
#include "dac_control.h"

static DEV_DEF devDef_DAC =
{
	NULL, NULL, 0,
	
	device_start_daccontrol,
	device_stop_daccontrol,
	device_reset_daccontrol,
	daccontrol_update,
	
	NULL,	// SetOptionBits
	NULL,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	NULL,	// rwFuncs
};

typedef struct
{
	DEVFUNC_READ_A8D8 A8D8;
	DEVFUNC_READ_A8D16 A8D16;
	DEVFUNC_READ_A16D8 A16D8;
	DEVFUNC_READ_A16D16 A16D16;
} DEVREAD_FUNCS;
typedef struct
{
	DEVFUNC_WRITE_A8D8 A8D8;
	DEVFUNC_WRITE_A8D16 A8D16;
	DEVFUNC_WRITE_A16D8 A16D8;
	DEVFUNC_WRITE_A16D16 A16D16;
} DEVWRITE_FUNCS;

typedef struct _dac_control
{
	const DEV_DEF* devDef;
	DEV_DATA* chipData;	// points to chip data structure
	DEVREAD_FUNCS Read;
	DEVWRITE_FUNCS Write;
	
	// Commands sent to dest-chip
	UINT8 DstChipType;
	UINT16 DstCommand;
	UINT8 CmdSize;
	
	UINT32 sampleRate;
	UINT32 Frequency;	// Frequency (Hz) at which the commands are sent
	UINT32 DataLen;		// to protect from reading beyond End Of Data
	const UINT8* Data;
	UINT32 DataStart;	// Position where to start
	UINT8 StepSize;		// usually 1, set to 2 for L/R interleaved data
	UINT8 StepBase;		// usually 0, set to 0/1 for L/R interleaved data
	UINT32 CmdsToSend;
	
	// Running Bits:	0 (01) - is playing
	//					2 (04) - loop sample (simple loop from start to end)
	//					7 (80) - disabled (needs setup)
	UINT8 Running;
	UINT8 Reverse;
	RATIO_CNTR stepCntr;
	UINT32 RemainCmds;
	UINT32 RealPos;		// true Position in Data (== Pos, if Reverse is off)
	UINT8 DataStep;		// always StepSize * CmdSize
} dac_control;

INLINE void daccontrol_SendCommand(dac_control* chip)
{
	UINT8 Port;
	UINT8 Command;
	UINT8 Data;
	UINT16 Data16;
	const UINT8* ChipData;
	
	if (chip->DataStart + chip->RealPos >= chip->DataLen)
		return;
	
	ChipData = &chip->Data[chip->DataStart + chip->RealPos];
	switch(chip->DstChipType)
	{
	// Explicit support for the important chips
	case DEVID_YM2612:	// 16-bit Register (actually 9 Bit), 8-bit Data
		Port = (chip->DstCommand & 0xFF00) >> 8;
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		chip->Write.A8D8(chip->chipData, (Port << 1) | 0, Command);
		chip->Write.A8D8(chip->chipData, (Port << 1) | 1, Data);
		break;
	case DEVID_32X_PWM:	// 4-bit Register, 12-bit Data
		Port = (chip->DstCommand & 0x000F) >> 0;
		Data16 = ((ChipData[0x01] & 0x0F) << 8) | (ChipData[0x00] << 0);
		chip->Write.A8D16(chip->chipData, Port, Data16);
		break;
	// Support for other chips (mainly for completeness)
	case DEVID_SN76496:	// 4-bit Register, 4-bit/10-bit Data
		Command = (chip->DstCommand & 0x00F0) >> 0;
		Data = ChipData[0x00] & 0x0F;
		if (Command & 0x10)
		{
			// Volume Change (4-Bit value)
			chip->Write.A8D8(chip->chipData, 0, Command | Data);
		}
		else
		{
			// Frequency Write (10-Bit value)
			Port = ((ChipData[0x01] & 0x03) << 4) | ((ChipData[0x00] & 0xF0) >> 4);
			chip->Write.A8D8(chip->chipData, 0, Command | Data);
			chip->Write.A8D8(chip->chipData, 0, Port);
		}
		break;
	case DEVID_OKIM6295:	// TODO: verify
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		
		if (! Command)
		{
			Port = (chip->DstCommand & 0x0F00) >> 8;
			if (Data & 0x80)
			{
				// Sample Start
				// write sample ID
				chip->Write.A8D8(chip->chipData, Command, Data);
				// write channel(s) that should play the sample
				chip->Write.A8D8(chip->chipData, Command, Port << 4);
			}
			else
			{
				// Sample Stop
				chip->Write.A8D8(chip->chipData, Command, Port << 3);
			}
		}
		else
		{
			chip->Write.A8D8(chip->chipData, Command, Data);
		}
		break;
		// Generic support: 8-bit Register, 8-bit Data
	case DEVID_YM2413:
	case DEVID_YM2151:
	case DEVID_YM2203:
	case DEVID_YM3812:
	case DEVID_YM3526:
	case DEVID_Y8950:
	case DEVID_YMZ280B:
	case DEVID_AY8910:
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		chip->Write.A8D8(chip->chipData, 0, Command);
		chip->Write.A8D8(chip->chipData, 1, Data);
		break;
	case DEVID_GB_DMG:
	case DEVID_NES_APU:
//	case DEVID_YMW258:
	case DEVID_uPD7759:
	case DEVID_OKIM6258:
	case DEVID_K053260:	// TODO: Verify
	case DEVID_POKEY:	// TODO: Verify
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		chip->Write.A8D8(chip->chipData, Command, Data);
		break;
		// Generic support: 16-bit Register, 8-bit Data
	case DEVID_YM2608:
	case DEVID_YM2610:
	case DEVID_YMF262:
	case DEVID_YMF278B:
	case DEVID_YMF271:
		Port = (chip->DstCommand & 0xFF00) >> 8;
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		chip->Write.A8D8(chip->chipData, (Port << 1) | 0, Command);
		chip->Write.A8D8(chip->chipData, (Port << 1) | 1, Data);
		break;
	case DEVID_K051649:	// TODO: Verify
	case DEVID_K054539:	// TODO: Verify
	case DEVID_C140:	// TODO: Verify
		Port = (chip->DstCommand & 0xFF00) >> 8;
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		//chip_reg_write(chip->DstChipType, chip->DstChipID, Port, Command, Data);
		break;
		// Generic support: 8-bit Register with Channel Select, 8-bit Data
	case DEVID_RF5C68:
	case DEVID_C6280:
		Port = (chip->DstCommand & 0xFF00) >> 8;
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		
		if (Port == 0xFF)
		{
			chip->Write.A8D8(chip->chipData, Command & 0x0F, Data);
		}
		else
		{
			UINT8 prevChn;
			
			prevChn = Port;	// by default don't restore channel
			// get current channel for supported chips
			if (chip->DstChipType == DEVID_RF5C68)
				;	// TODO
			else if (chip->DstChipType == DEVID_C6280)
				prevChn = chip->Read.A8D8(chip->chipData, 0x00);
			
			// Send Channel Select
			chip->Write.A8D8(chip->chipData, Command >> 4, Port);
			// Send Data
			chip->Write.A8D8(chip->chipData, Command & 0x0F, Data);
			// restore old channel
			if (prevChn != Port)
				chip->Write.A8D8(chip->chipData, Command >> 4, prevChn);
		}
		break;
		// Generic support: 8-bit Register, 16-bit Data
	case DEVID_QSOUND:
		Command = (chip->DstCommand & 0x00FF) >> 0;
		chip->Write.A8D8(chip->chipData, 0x00, ChipData[0x00]);	// Data MSB
		chip->Write.A8D8(chip->chipData, 0x01, ChipData[0x01]);	// Data LSB
		chip->Write.A8D8(chip->chipData, 0x02, Command);		// Register
		break;
	}
	
	return;
}

void daccontrol_update(void* info, UINT32 samples, DEV_SMPL** dummy)
{
	dac_control* chip = (dac_control*)info;
	INT32 RealDataStp;
	UINT32 cmdsToProc;
	
	if (chip->Running & 0x80)	// disabled
		return;
	if (! (chip->Running & 0x01))	// stopped
		return;
	
	RealDataStp = (chip->Reverse) ? -chip->DataStep : +chip->DataStep;
	
	RC_STEPS(&chip->stepCntr, samples);
	cmdsToProc = RC_GET_VAL(&chip->stepCntr);
	RC_MASK(&chip->stepCntr);
	
	if (cmdsToProc > chip->RemainCmds)
		cmdsToProc = chip->RemainCmds;
	chip->RemainCmds -= cmdsToProc;
	
	if (cmdsToProc > 0x20)
	{
		// very effective Speed Hack for fast seeking
		chip->RealPos += RealDataStp * (cmdsToProc - 0x10);
		cmdsToProc = 0x10;
	}
	
	for (; cmdsToProc > 0; cmdsToProc --)
	{
		daccontrol_SendCommand(chip);
		chip->RealPos += RealDataStp;
	}
	
	if (! chip->RemainCmds && (chip->Running & 0x04))
	{
		// loop back to start
		chip->RemainCmds = chip->CmdsToSend;
		if (! chip->Reverse)
			chip->RealPos = 0x00;
		else
			chip->RealPos = (chip->CmdsToSend - 1) * chip->DataStep;
	}
	
	if (! chip->RemainCmds)
		chip->Running &= ~0x01;	// stop
	
	return;
}

UINT8 device_start_daccontrol(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	dac_control* chip;
	DEV_DATA* devData;
	
	chip = (dac_control*)calloc(1, sizeof(dac_control));
	if (chip == NULL)
		return 0xFF;
	
	chip->sampleRate = cfg->smplRate;
	chip->devDef = NULL;
	chip->chipData = NULL;
	chip->DstChipType = 0xFF;
	chip->DstCommand = 0x0000;
	
	chip->Running = 0xFF;	// disable all actions (except setup_chip)
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, chip->sampleRate, &devDef_DAC);
	return 0x00;
}

void device_stop_daccontrol(void* info)
{
	dac_control* chip = (dac_control*)info;
	
	free(chip);
	
	return;
}

void device_reset_daccontrol(void* info)
{
	dac_control* chip = (dac_control*)info;
	
	chip->devDef = NULL;
	chip->chipData = NULL;
	chip->DstChipType = 0xFF;
	chip->DstCommand = 0x00;
	chip->CmdSize = 0x00;
	
	chip->Frequency = 0;
	chip->DataLen = 0x00;
	chip->Data = NULL;
	chip->DataStart = 0x00;
	chip->StepSize = 0x00;
	chip->StepBase = 0x00;
	
	chip->Running = 0xFF;
	chip->Reverse = 0x00;
	RC_RESET(&chip->stepCntr);
	chip->RealPos = 0x00;
	chip->RemainCmds = 0x00;
	chip->DataStep = 0x00;
	
	return;
}

void daccontrol_setup_chip(void* info, DEV_INFO* devInf, UINT8 ChType, UINT16 Command)
{
	dac_control* chip = (dac_control*)info;
	
	chip->devDef = devInf->devDef;
	chip->chipData = devInf->dataPtr;
	chip->DstChipType = ChType;	// TypeID (e.g. 0x02 for YM2612)
	chip->DstCommand = Command;	// Port and Command (would be 0x02A for YM2612)
	
	memset(&chip->Write, 0x00, sizeof(DEVWRITE_FUNCS));
	memset(&chip->Read, 0x00, sizeof(DEVREAD_FUNCS));
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8,   0, (void**)&chip->Write.A8D8);
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D16,  0, (void**)&chip->Write.A8D16);
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D8,  0, (void**)&chip->Write.A16D8);
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D16, 0, (void**)&chip->Write.A16D16);
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_READ, DEVRW_A8D8,   0, (void**)&chip->Read.A8D8);
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_READ, DEVRW_A8D16,  0, (void**)&chip->Read.A8D16);
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_READ, DEVRW_A16D8,  0, (void**)&chip->Read.A16D8);
	SndEmu_GetDeviceFunc(chip->devDef, RWF_REGISTER | RWF_READ, DEVRW_A16D16, 0, (void**)&chip->Read.A16D16);
	
	switch(chip->DstChipType)
	{
	case DEVID_SN76496:
		if (chip->DstCommand & 0x0010)
			chip->CmdSize = 0x01;	// Volume Write
		else
			chip->CmdSize = 0x02;	// Frequency Write
		break;
	case DEVID_YM2612:
		chip->CmdSize = 0x01;
		break;
	case DEVID_32X_PWM:
	case DEVID_QSOUND:
		chip->CmdSize = 0x02;
		break;
	default:
		chip->CmdSize = 0x01;
		break;
	}
	chip->DataStep = chip->CmdSize * chip->StepSize;
	
	chip->Running &= 0x00;
	
	return;
}

void daccontrol_set_data(void* info, UINT8* Data, UINT32 DataLen, UINT8 StepSize, UINT8 StepBase)
{
	dac_control* chip = (dac_control*)info;
	
	if (chip->Running & 0x80)
		return;
	
	if (DataLen && Data != NULL)
	{
		chip->DataLen = DataLen;
		chip->Data = Data;
	}
	else
	{
		chip->DataLen = 0x00;
		chip->Data = NULL;
	}
	chip->StepSize = StepSize ? StepSize : 1;
	chip->StepBase = StepBase;
	chip->DataStep = chip->CmdSize * chip->StepSize;
	
	return;
}

void daccontrol_refresh_data(void* info, UINT8* Data, UINT32 DataLen)
{
	// should be called to fix the data pointer (e.g. after a realloc)
	dac_control* chip = (dac_control*)info;
	
	if (chip->Running & 0x80)
		return;
	
	if (DataLen && Data != NULL)
	{
		chip->DataLen = DataLen;
		chip->Data = Data;
	}
	else
	{
		chip->DataLen = 0x00;
		chip->Data = NULL;
	}
	
	return;
}

void daccontrol_set_frequency(void* info, UINT32 Frequency)
{
	dac_control* chip = (dac_control*)info;
	
	if (chip->Running & 0x80)
		return;
	
	chip->Frequency = Frequency;
	RC_SET_RATIO(&chip->stepCntr, chip->Frequency, chip->sampleRate);
	
	return;
}

void daccontrol_start(void* info, UINT32 DataPos, UINT8 LenMode, UINT32 Length)
{
	dac_control* chip = (dac_control*)info;
	UINT16 CmdStepBase;
	
	if (chip->Running & 0x80)
		return;
	
	CmdStepBase = chip->CmdSize * chip->StepBase;
	if (DataPos != (UINT32)-1)	// skip setting DataStart, if Pos == -1
	{
		chip->DataStart = DataPos + CmdStepBase;
		if (chip->DataStart > chip->DataLen)	// catch bad value and force silence
			chip->DataStart = chip->DataLen;
	}
	
	switch(LenMode & 0x0F)
	{
	case DCTRL_LMODE_IGNORE:	// Length is already set - ignore
		break;
	case DCTRL_LMODE_CMDS:		// Length = number of commands
		chip->CmdsToSend = Length;
		break;
	case DCTRL_LMODE_MSEC:		// Length = time in msec
		chip->CmdsToSend = 1000 * Length / chip->Frequency;
		break;
	case DCTRL_LMODE_TOEND:		// play until stop-command is received (or data-end is reached)
		chip->CmdsToSend = (chip->DataLen - (chip->DataStart - CmdStepBase)) / chip->DataStep;
		break;
	case DCTRL_LMODE_BYTES:		// raw byte count
		chip->CmdsToSend = Length / chip->DataStep;
		break;
	default:
		chip->CmdsToSend = 0x00;
		break;
	}
	chip->Reverse = (LenMode & 0x10) >> 4;
	
	chip->RemainCmds = chip->CmdsToSend;
	RC_RESET_PRESTEP(&chip->stepCntr);	// first sample is sent with next update
	if (! chip->Reverse)
		chip->RealPos = 0x00;
	else
		chip->RealPos = (chip->CmdsToSend - 1) * chip->DataStep;
	
	chip->Running &= ~0x04;
	chip->Running |= (LenMode & 0x80) ? 0x04 : 0x00;	// set loop mode
	
	chip->Running |= 0x01;	// start
	
	return;
}

void daccontrol_stop(void* info)
{
	dac_control* chip = (dac_control*)info;
	
	if (chip->Running & 0x80)
		return;
	
	chip->Running &= ~0x01;	// stop
	
	return;
}
