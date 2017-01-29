#include <stddef.h>
#include <stdlib.h>	// for malloc/free

#include <stdtype.h>
#include "EmuStructs.h"
#include "Resampler.h"

#define RESALGO_OLD			0x00
#define RESALGO_LINEAR_UP	0x01
#define RESALGO_COPY		0x02
#define RESALGO_LINEAR_DOWN	0x03

static void Resmpl_Exec_Old(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample);
static void Resmpl_Exec_LinearUp(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample);
static void Resmpl_Exec_Copy(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample);
static void Resmpl_Exec_LinearDown(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample);

void Resmpl_DevConnect(RESMPL_STATE* CAA, const DEV_INFO* devInf)
{
	CAA->SmpRateSrc = devInf->sampleRate;
	CAA->StreamUpdate = devInf->devDef->Update;
	CAA->SU_DataPtr = devInf->dataPtr;
	if (devInf->devDef->SetSRateChgCB != NULL)
		devInf->devDef->SetSRateChgCB(CAA->SU_DataPtr, Resmpl_ChangeRate, CAA);
	
	return;
}

void Resmpl_SetVals(RESMPL_STATE* CAA, UINT8 resampleMode, UINT16 volume, UINT32 destSampleRate)
{
	CAA->ResampleMode = resampleMode;
	CAA->SmpRateDst = destSampleRate;
	CAA->VolumeL = volume;	CAA->VolumeR = volume;
	
	return;
}

void Resmpl_Init(RESMPL_STATE* CAA)
{
	if (! CAA->SmpRateSrc)
	{
		CAA->Resampler = 0xFF;
		return;
	}
	
	if (CAA->ResampleMode == 0xFF)
	{
		if (CAA->SmpRateSrc < CAA->SmpRateDst)
			CAA->Resampler = RESALGO_LINEAR_UP;
		else if (CAA->SmpRateSrc == CAA->SmpRateDst)
			CAA->Resampler = RESALGO_COPY;
		else if (CAA->SmpRateSrc > CAA->SmpRateDst)
			CAA->Resampler = RESALGO_LINEAR_DOWN;
	}
	/*if (CAA->Resampler == RESALGO_LINEAR_UP || CAA->Resampler == RESALGO_LINEAR_DOWN)
	{
		if (CAA->ResampleMode == 0x02 || (CAA->ResampleMode == 0x01 && CAA->Resampler == RESALGO_LINEAR_DOWN))
			CAA->Resampler = RESALGO_OLD;
	}*/
	
	CAA->SmplBufSize = CAA->SmpRateSrc / 10;
	CAA->SmplBufs[0] = (DEV_SMPL*)malloc(CAA->SmplBufSize * 2 * sizeof(DEV_SMPL));
	CAA->SmplBufs[1] = &CAA->SmplBufs[0][CAA->SmplBufSize];
	
	CAA->SmpP = 0x00;
	CAA->SmpLast = 0x00;
	CAA->SmpNext = 0x00;
	CAA->LSmpl.L = 0x00;
	CAA->LSmpl.R = 0x00;
	if (CAA->Resampler == RESALGO_LINEAR_UP)
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

void Resmpl_Deinit(RESMPL_STATE* CAA)
{
	free(CAA->SmplBufs[0]);
	CAA->SmplBufs[0] = NULL;
	CAA->SmplBufs[1] = NULL;
	
	return;
}

void Resmpl_ChangeRate(void* DataPtr, UINT32 NewSmplRate)
{
	RESMPL_STATE* CAA = (RESMPL_STATE*)DataPtr;
	
	if (CAA->SmpRateSrc == NewSmplRate)
		return;
	
	// quick and dirty hack to make sample rate changes work
	CAA->SmpRateSrc = NewSmplRate;
	if (CAA->ResampleMode == 0xFF)
	{
		if (CAA->SmpRateSrc < CAA->SmpRateDst)
			CAA->Resampler = RESALGO_LINEAR_UP;
		else if (CAA->SmpRateSrc == CAA->SmpRateDst)
			CAA->Resampler = RESALGO_COPY;
		else if (CAA->SmpRateSrc > CAA->SmpRateDst)
			CAA->Resampler = RESALGO_LINEAR_DOWN;
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

static void Resmpl_Exec_Old(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample)
{
	// RESALGO_OLD: old, but very fast resampler
	DEV_SMPL* CurBufL;
	DEV_SMPL* CurBufR;
	UINT32 OutPos;
	INT32 TempS32L;
	INT32 TempS32R;
	INT32 SmpCnt;	// must be signed, else I'm getting calculation errors
	INT32 CurSmpl;
	
	CurBufL = CAA->SmplBufs[0];
	CurBufR = CAA->SmplBufs[1];
	
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
	
	if (CAA->SmpLast >= CAA->SmpRateSrc)
	{
		CAA->SmpLast -= CAA->SmpRateSrc;
		CAA->SmpNext -= CAA->SmpRateSrc;
		CAA->SmpP -= CAA->SmpRateDst;
	}
	
	return;
}

static void Resmpl_Exec_LinearUp(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample)
{
	// RESALGO_LINEAR_UP: Linear Upsampling
	DEV_SMPL* CurBufL;
	DEV_SMPL* CurBufR;
	DEV_SMPL* StreamPnt[0x02];
	UINT32 InBase;
	UINT32 InPos;
	UINT32 OutPos;
	UINT32 SmpFrc;	// Sample Fraction
	UINT32 InPre;
	UINT32 InNow;
	SLINT InPosL;
	INT64 TempSmpL;
	INT64 TempSmpR;
	INT32 SmpCnt;	// must be signed, else I'm getting calculation errors
	UINT64 ChipSmpRateFP;
	
	CurBufL = CAA->SmplBufs[0];
	CurBufR = CAA->SmplBufs[1];
	
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
	
	if (CAA->SmpLast >= CAA->SmpRateSrc)
	{
		CAA->SmpLast -= CAA->SmpRateSrc;
		CAA->SmpNext -= CAA->SmpRateSrc;
		CAA->SmpP -= CAA->SmpRateDst;
	}
	
	return;
}

static void Resmpl_Exec_Copy(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample)
{
	// RESALGO_COPY: Copying
	UINT32 OutPos;
	
	CAA->SmpNext = CAA->SmpP * CAA->SmpRateSrc / CAA->SmpRateDst;
	CAA->StreamUpdate(CAA->SU_DataPtr, Length, CAA->SmplBufs);
	
	for (OutPos = 0; OutPos < Length; OutPos ++)
	{
		RetSample[OutPos].L += CAA->SmplBufs[0][OutPos] * CAA->VolumeL;
		RetSample[OutPos].R += CAA->SmplBufs[1][OutPos] * CAA->VolumeR;
	}
	CAA->SmpP += Length;
	CAA->SmpLast = CAA->SmpNext;
	
	if (CAA->SmpLast >= CAA->SmpRateSrc)
	{
		CAA->SmpLast -= CAA->SmpRateSrc;
		CAA->SmpNext -= CAA->SmpRateSrc;
		CAA->SmpP -= CAA->SmpRateDst;
	}
	
	return;
}

static void Resmpl_Exec_LinearDown(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample)
{
	// RESALGO_LINEAR_DOWN: Linear Downsampling
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
	INT32 SmpCnt;	// must be signed, else I'm getting calculation errors
	UINT64 ChipSmpRateFP;
	
	CurBufL = CAA->SmplBufs[0];
	CurBufR = CAA->SmplBufs[1];
	
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
	
	if (CAA->SmpLast >= CAA->SmpRateSrc)
	{
		CAA->SmpLast -= CAA->SmpRateSrc;
		CAA->SmpNext -= CAA->SmpRateSrc;
		CAA->SmpP -= CAA->SmpRateDst;
	}
	
	return;
}

void Resmpl_Execute(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample)
{
	if (! Length)
		return;
	
	switch(CAA->Resampler)
	{
	case RESALGO_OLD:	// old, but very fast resampler
		Resmpl_Exec_Old(CAA, Length, RetSample);
		break;
	case RESALGO_LINEAR_UP:	// Upsampling
		Resmpl_Exec_LinearUp(CAA, Length, RetSample);
		break;
	case RESALGO_COPY:	// Copying
		Resmpl_Exec_Copy(CAA, Length, RetSample);
		break;
	case RESALGO_LINEAR_DOWN:	// Downsampling
		Resmpl_Exec_LinearDown(CAA, Length, RetSample);
		break;
	default:
		CAA->SmpP += CAA->SmpRateDst;
		break;	// do absolutely nothing
	}
	
	return;
}
