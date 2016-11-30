#include <stddef.h>
#include <stdlib.h>	// for malloc/free

#include <stdtype.h>
#include "EmuStructs.h"
#include "SoundEmu.h"

#include "sn764intf.h"
#include "okim6295.h"

const DEVINF_LIST* SndEmu_GetDevInfList(UINT8 deviceID)
{
	switch(deviceID)
	{
	case DEVID_SN76496:
		return devInfList_SN76496;
	case DEVID_OKIM6295:
		return devInfList_OKIM6295;
	}
	return NULL;
}

UINT8 SndEmu_Start(UINT8 deviceID, const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	const DEVINF_LIST* diList;
	const DEVINF_LIST* curDIL;
	
	diList = SndEmu_GetDevInfList(deviceID);
	if (diList == NULL)
		return EERR_UNK_DEVICE;
	
	// TODO: make using emuCore optional (-> use default device)
	for (curDIL = diList; curDIL->devInf != NULL; curDIL ++)
	{
		if (curDIL->coreID == cfg->emuCore)
			return curDIL->devInf->Start(cfg, retDevInf);
	}
	return EERR_UNK_DEVICE;
}

UINT8 SndEmu_Stop(DEV_INFO* devInf)
{
	devInf->Stop(devInf->dataPtr);
	devInf->dataPtr = NULL;
	
	return 0x00;
}

UINT8 SndEmu_GetDeviceFunc(const DEV_INFO* devInf, UINT8 funcType, UINT8 rwType, UINT16 reserved, void** retFuncPtr)
{
	UINT32 curFunc;
	const DEVINF_RWFUNC* tempFnc;
	UINT32 firstFunc;
	UINT32 foundFunc;
	
	foundFunc = 0;
	firstFunc = 0;
	for (curFunc = 0; curFunc < devInf->rwFuncCount; curFunc ++)
	{
		tempFnc = &devInf->rwFuncs[curFunc];
		if (tempFnc->funcType == funcType && tempFnc->rwType == rwType)
		{
			if (foundFunc == 0)
				firstFunc = curFunc;
			foundFunc ++;
		}
	}
	if (foundFunc == 0)
		return 0xFF;	// not found
	*retFuncPtr = devInf->rwFuncs[firstFunc].funcPtr;
	if (foundFunc == 1)
		return 0x00;
	else
		return 0x01;	// found multiple matching functions
}

void SndEmu_ResamplerInit(RESMPL_STATE* CAA)
{
	if (! CAA->SmpRateSrc)
	{
		CAA->Resampler = 0xFF;
		return;
	}
	
	if (CAA->ResampleMode == 0xFF)
	{
		if (CAA->SmpRateSrc < CAA->SmpRateDst)
			CAA->Resampler = 0x01;
		else if (CAA->SmpRateSrc == CAA->SmpRateDst)
			CAA->Resampler = 0x02;
		else if (CAA->SmpRateSrc > CAA->SmpRateDst)
			CAA->Resampler = 0x03;
	}
	/*if (CAA->Resampler == 0x01 || CAA->Resampler == 0x03)
	{
		if (ResampleMode == 0x02 || (ResampleMode == 0x01 && CAA->Resampler == 0x03))
			CAA->Resampler = 0x00;
	}*/
	
	CAA->SmplBufSize = CAA->SmpRateSrc / 10;
	CAA->SmplBufs[0] = (DEV_SMPL*)malloc(CAA->SmplBufSize * 2 * sizeof(DEV_SMPL));
	CAA->SmplBufs[1] = &CAA->SmplBufs[0][CAA->SmplBufSize];
	
	CAA->SmpP = 0x00;
	CAA->SmpLast = 0x00;
	CAA->SmpNext = 0x00;
	CAA->LSmpl.L = 0x00;
	CAA->LSmpl.R = 0x00;
	if (CAA->Resampler == 0x01)
	{
		// Pregenerate first Sample (the upsampler is always one too late)
		CAA->StreamUpdate(CAA->SU_DataPtr, 1, CAA->SmplBufs);
		CAA->NSmpl.L = CAA->SmplBufs[0][0];
		CAA->NSmpl.R = CAA->SmplBufs[1][0];
	}
	else
	{
		CAA->NSmpl.L = 0x00;
		CAA->NSmpl.R = 0x00;
	}
	
	return;
}

void SndEmu_ResamplerDeinit(RESMPL_STATE* CAA)
{
	free(CAA->SmplBufs[0]);
	CAA->SmplBufs[0] = NULL;
	CAA->SmplBufs[1] = NULL;
	
	return;
}

void SndEmu_ResamplerReset(void* DataPtr, UINT32 NewSmplRate)
{
	RESMPL_STATE* CAA = (RESMPL_STATE*)DataPtr;
	
	if (CAA->SmpRateSrc == NewSmplRate)
		return;
	
	// quick and dirty hack to make sample rate changes work
	CAA->SmpRateSrc = NewSmplRate;
	if (CAA->ResampleMode == 0xFF)
	{
		if (CAA->SmpRateSrc < CAA->SmpRateDst)
			CAA->Resampler = 0x01;
		else if (CAA->SmpRateSrc == CAA->SmpRateDst)
			CAA->Resampler = 0x02;
		else if (CAA->SmpRateSrc > CAA->SmpRateDst)
			CAA->Resampler = 0x03;
	}
	CAA->SmpP = 1;
	CAA->SmpNext -= CAA->SmpLast;
	CAA->SmpLast = 0x00;
	
	return;
}

// I recommend 11 bits as it's fast and accurate
#define FIXPNT_BITS		11
#define FIXPNT_FACT		(1 << FIXPNT_BITS)
#if (FIXPNT_BITS <= 11)
	typedef UINT32	SLINT;	// 32-bit is a lot faster
#else
	typedef UINT64	SLINT;
#endif
#define FIXPNT_MASK		(FIXPNT_FACT - 1)

#define getfraction(x)	((x) & FIXPNT_MASK)
#define getnfraction(x)	((FIXPNT_FACT - (x)) & FIXPNT_MASK)
#define fpi_floor(x)	((x) & ~FIXPNT_MASK)
#define fpi_ceil(x)		((x + FIXPNT_MASK) & ~FIXPNT_MASK)
#define fp2i_floor(x)	((x) / FIXPNT_FACT)
#define fp2i_ceil(x)	((x + FIXPNT_MASK) / FIXPNT_FACT)

void SndEmu_ResamplerExecute(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample)
{
	DEV_SMPL* CurBufL;
	DEV_SMPL* CurBufR;
	DEV_SMPL* StreamPnt[0x02];
	UINT32 InBase;
	UINT32 InPos;
	UINT32 InPosNext;
	UINT32 OutPos;
	UINT32 SmpFrc;	// Sample Fraction
	UINT32 InPre;
	UINT32 InNow;
	SLINT InPosL;
	INT64 TempSmpL;
	INT64 TempSmpR;
	INT32 TempS32L;
	INT32 TempS32R;
	INT32 SmpCnt;	// must be signed, else I'm getting calculation errors
	INT32 CurSmpl;
	UINT64 ChipSmpRateFP;
	
	CurBufL = CAA->SmplBufs[0];
	CurBufR = CAA->SmplBufs[1];
	
	switch(CAA->Resampler)
	{
	case 0x00:	// old, but very fast resampler
		for (OutPos = 0; OutPos < Length; OutPos ++)
		{
		CAA->SmpLast = CAA->SmpNext;
		CAA->SmpP ++;
		CAA->SmpNext = (UINT32)((UINT64)CAA->SmpP * CAA->SmpRateSrc / CAA->SmpRateDst);
		if (CAA->SmpLast >= CAA->SmpNext)
		{
			RetSample[OutPos].L += CAA->LSmpl.L * CAA->VolumeL;
			RetSample[OutPos].R += CAA->LSmpl.R * CAA->VolumeR;
		}
		else //if (CAA->SmpLast < CAA->SmpNext)
		{
			SmpCnt = CAA->SmpNext - CAA->SmpLast;
			
			CAA->StreamUpdate(CAA->SU_DataPtr, SmpCnt, CAA->SmplBufs);
			
			if (SmpCnt == 1)
			{
				RetSample[OutPos].L += CurBufL[0] * CAA->VolumeL;
				RetSample[OutPos].R += CurBufR[0] * CAA->VolumeR;
				CAA->LSmpl.L = CurBufL[0];
				CAA->LSmpl.R = CurBufR[0];
			}
			else if (SmpCnt == 2)
			{
				RetSample[OutPos].L += (CurBufL[0] + CurBufL[1]) * CAA->VolumeL >> 1;
				RetSample[OutPos].R += (CurBufR[0] + CurBufR[1]) * CAA->VolumeR >> 1;
				CAA->LSmpl.L = CurBufL[1];
				CAA->LSmpl.R = CurBufR[1];
			}
			else
			{
				TempS32L = CurBufL[0];
				TempS32R = CurBufR[0];
				for (CurSmpl = 1; CurSmpl < SmpCnt; CurSmpl ++)
				{
					TempS32L += CurBufL[CurSmpl];
					TempS32R += CurBufR[CurSmpl];
				}
				RetSample[OutPos].L += TempS32L * CAA->VolumeL / SmpCnt;
				RetSample[OutPos].R += TempS32R * CAA->VolumeR / SmpCnt;
				CAA->LSmpl.L = CurBufL[SmpCnt - 1];
				CAA->LSmpl.R = CurBufR[SmpCnt - 1];
			}
		}
		}
		break;
	case 0x01:	// Upsampling
		ChipSmpRateFP = FIXPNT_FACT * CAA->SmpRateSrc;
		StreamPnt[0] = &CurBufL[2];
		StreamPnt[1] = &CurBufR[2];
		// TODO: Make it work *properly* with large blocks. (this is very hackish)
		for (OutPos = 0; OutPos < Length; OutPos ++)
		{
			InPosL = (SLINT)(CAA->SmpP * ChipSmpRateFP / CAA->SmpRateDst);
			InPre = (UINT32)fp2i_floor(InPosL);
			InNow = (UINT32)fp2i_ceil(InPosL);
			
			CurBufL[0] = CAA->LSmpl.L;
			CurBufR[0] = CAA->LSmpl.R;
			CurBufL[1] = CAA->NSmpl.L;
			CurBufR[1] = CAA->NSmpl.R;
			if (InNow != CAA->SmpNext)
				CAA->StreamUpdate(CAA->SU_DataPtr, InNow - CAA->SmpNext, StreamPnt);
			
			InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->SmpNext * FIXPNT_FACT);
			SmpCnt = FIXPNT_FACT;
			CAA->SmpLast = InPre;
			CAA->SmpNext = InNow;
			//for (OutPos = 0; OutPos < Length; OutPos ++)
			//{
			//InPos = InBase + (UINT32)(OutPos * ChipSmpRateFP / CAA->SmpRateDst);
			InPos = InBase;
			
			InPre = fp2i_floor(InPos);
			InNow = fp2i_ceil(InPos);
			SmpFrc = getfraction(InPos);
			
			// Linear interpolation
			TempSmpL = ((INT64)CurBufL[InPre] * (FIXPNT_FACT - SmpFrc)) +
						((INT64)CurBufL[InNow] * SmpFrc);
			TempSmpR = ((INT64)CurBufR[InPre] * (FIXPNT_FACT - SmpFrc)) +
						((INT64)CurBufR[InNow] * SmpFrc);
			RetSample[OutPos].L += (INT32)(TempSmpL * CAA->VolumeL / SmpCnt);
			RetSample[OutPos].R += (INT32)(TempSmpR * CAA->VolumeR / SmpCnt);
			//}
			CAA->LSmpl.L = CurBufL[InPre];
			CAA->LSmpl.R = CurBufR[InPre];
			CAA->NSmpl.L = CurBufL[InNow];
			CAA->NSmpl.R = CurBufR[InNow];
			CAA->SmpP ++;
		}
		break;
	case 0x02:	// Copying
		CAA->SmpNext = CAA->SmpP * CAA->SmpRateSrc / CAA->SmpRateDst;
		CAA->StreamUpdate(CAA->SU_DataPtr, Length, CAA->SmplBufs);
		
		for (OutPos = 0; OutPos < Length; OutPos ++)
		{
			RetSample[OutPos].L += CurBufL[OutPos] * CAA->VolumeL;
			RetSample[OutPos].R += CurBufR[OutPos] * CAA->VolumeR;
		}
		CAA->SmpP += Length;
		CAA->SmpLast = CAA->SmpNext;
		break;
	case 0x03:	// Downsampling
		ChipSmpRateFP = FIXPNT_FACT * CAA->SmpRateSrc;
		InPosL = (SLINT)((CAA->SmpP + Length) * ChipSmpRateFP / CAA->SmpRateDst);
		CAA->SmpNext = (UINT32)fp2i_ceil(InPosL);
		
		CurBufL[0] = CAA->LSmpl.L;
		CurBufR[0] = CAA->LSmpl.R;
		StreamPnt[0] = &CurBufL[1];
		StreamPnt[1] = &CurBufR[1];
		CAA->StreamUpdate(CAA->SU_DataPtr, CAA->SmpNext - CAA->SmpLast, StreamPnt);
		
		InPosL = (SLINT)(CAA->SmpP * ChipSmpRateFP / CAA->SmpRateDst);
		// I'm adding 1.0 to avoid negative indexes
		InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->SmpLast * FIXPNT_FACT);
		InPosNext = InBase;
		for (OutPos = 0; OutPos < Length; OutPos ++)
		{
			//InPos = InBase + (UINT32)(OutPos * ChipSmpRateFP / CAA->SmpRateDst);
			InPos = InPosNext;
			InPosNext = InBase + (UINT32)((OutPos+1) * ChipSmpRateFP / CAA->SmpRateDst);
			
			// first fractional Sample
			SmpFrc = getnfraction(InPos);
			if (SmpFrc)
			{
				InPre = fp2i_floor(InPos);
				TempSmpL = (INT64)CurBufL[InPre] * SmpFrc;
				TempSmpR = (INT64)CurBufR[InPre] * SmpFrc;
			}
			else
			{
				TempSmpL = TempSmpR = 0;
			}
			SmpCnt = SmpFrc;
			
			// last fractional Sample
			SmpFrc = getfraction(InPosNext);
			InPre = fp2i_floor(InPosNext);
			if (SmpFrc)
			{
				TempSmpL += (INT64)CurBufL[InPre] * SmpFrc;
				TempSmpR += (INT64)CurBufR[InPre] * SmpFrc;
				SmpCnt += SmpFrc;
			}
			
			// whole Samples in between
			//InPre = fp2i_floor(InPosNext);
			InNow = fp2i_ceil(InPos);
			SmpCnt += (InPre - InNow) * FIXPNT_FACT;	// this is faster
			while(InNow < InPre)
			{
				TempSmpL += (INT64)CurBufL[InNow] * FIXPNT_FACT;
				TempSmpR += (INT64)CurBufR[InNow] * FIXPNT_FACT;
				//SmpCnt ++;
				InNow ++;
			}
			
			RetSample[OutPos].L += (INT32)(TempSmpL * CAA->VolumeL / SmpCnt);
			RetSample[OutPos].R += (INT32)(TempSmpR * CAA->VolumeR / SmpCnt);
		}
		
		CAA->LSmpl.L = CurBufL[InPre];
		CAA->LSmpl.R = CurBufR[InPre];
		CAA->SmpP += Length;
		CAA->SmpLast = CAA->SmpNext;
		break;
	default:
		CAA->SmpP += CAA->SmpRateDst;
		break;	// do absolutely nothing
	}
	
	if (CAA->SmpLast >= CAA->SmpRateSrc)
	{
		CAA->SmpLast -= CAA->SmpRateSrc;
		CAA->SmpNext -= CAA->SmpRateSrc;
		CAA->SmpP -= CAA->SmpRateDst;
	}
	
	return;
}
