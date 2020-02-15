#include <stddef.h>
#include <stdlib.h>	// for malloc/free

#include "../stdtype.h"
#include "EmuStructs.h"
#include "Resampler.h"

#define RESALGO_OLD			0x00
#define RESALGO_LINEAR_UP	0x01
#define RESALGO_COPY		0x02
#define RESALGO_LINEAR_DOWN	0x03

static void Resmpl_Exec_Old(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample);
static void Resmpl_Exec_LinearUp(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample);
static void Resmpl_Exec_Copy(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample);
static void Resmpl_Exec_LinearDown(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample);

void Resmpl_DevConnect(RESMPL_STATE* CAA, const DEV_INFO* devInf)
{
	CAA->smpRateSrc = devInf->sampleRate;
	CAA->StreamUpdate = devInf->devDef->Update;
	CAA->su_DataPtr = devInf->dataPtr;
	if (devInf->devDef->SetSRateChgCB != NULL)
		devInf->devDef->SetSRateChgCB(CAA->su_DataPtr, Resmpl_ChangeRate, CAA);
	
	return;
}

void Resmpl_SetVals(RESMPL_STATE* CAA, UINT8 resampleMode, UINT16 volume, UINT32 destSampleRate)
{
	CAA->resampleMode = resampleMode;
	CAA->smpRateDst = destSampleRate;
	CAA->volumeL = volume;	CAA->volumeR = volume;
	
	return;
}

void Resmpl_Init(RESMPL_STATE* CAA)
{
	if (! CAA->smpRateSrc)
	{
		CAA->resampler = 0xFF;
		return;
	}
	
	if (CAA->resampleMode == 0xFF)
	{
		if (CAA->smpRateSrc < CAA->smpRateDst)
			CAA->resampler = RESALGO_LINEAR_UP;
		else if (CAA->smpRateSrc == CAA->smpRateDst)
			CAA->resampler = RESALGO_COPY;
		else if (CAA->smpRateSrc > CAA->smpRateDst)
			CAA->resampler = RESALGO_LINEAR_DOWN;
	}
	/*if (CAA->resampler == RESALGO_LINEAR_UP || CAA->resampler == RESALGO_LINEAR_DOWN)
	{
		if (CAA->resampleMode == 0x02 || (CAA->resampleMode == 0x01 && CAA->resampler == RESALGO_LINEAR_DOWN))
			CAA->resampler = RESALGO_OLD;
	}*/
	
	CAA->smplBufSize = CAA->smpRateSrc / 10;
	CAA->smplBufs[0] = (DEV_SMPL*)malloc(CAA->smplBufSize * 2 * sizeof(DEV_SMPL));
	CAA->smplBufs[1] = &CAA->smplBufs[0][CAA->smplBufSize];
	
	CAA->smpP = 0x00;
	CAA->smpLast = 0x00;
	CAA->smpNext = 0x00;
	CAA->lSmpl.L = 0x00;
	CAA->lSmpl.R = 0x00;
	if (CAA->resampler == RESALGO_LINEAR_UP)
	{
		// Pregenerate first Sample (the upsampler is always one too late)
		CAA->StreamUpdate(CAA->su_DataPtr, 1, CAA->smplBufs);
		CAA->nSmpl.L = CAA->smplBufs[0][0];
		CAA->nSmpl.R = CAA->smplBufs[1][0];
	}
	else
	{
		CAA->nSmpl.L = 0x00;
		CAA->nSmpl.R = 0x00;
	}
	
	return;
}

void Resmpl_Deinit(RESMPL_STATE* CAA)
{
	free(CAA->smplBufs[0]);
	CAA->smplBufs[0] = NULL;
	CAA->smplBufs[1] = NULL;
	
	return;
}

void Resmpl_ChangeRate(void* DataPtr, UINT32 newSmplRate)
{
	RESMPL_STATE* CAA = (RESMPL_STATE*)DataPtr;
	
	if (CAA->smpRateSrc == newSmplRate)
		return;
	
	// quick and dirty hack to make sample rate changes work
	CAA->smpRateSrc = newSmplRate;
	if (CAA->resampleMode == 0xFF)
	{
		if (CAA->smpRateSrc < CAA->smpRateDst)
			CAA->resampler = RESALGO_LINEAR_UP;
		else if (CAA->smpRateSrc == CAA->smpRateDst)
			CAA->resampler = RESALGO_COPY;
		else if (CAA->smpRateSrc > CAA->smpRateDst)
			CAA->resampler = RESALGO_LINEAR_DOWN;
	}
	CAA->smpP = 1;
	CAA->smpNext -= CAA->smpLast;
	CAA->smpLast = 0x00;
	
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

static void Resmpl_Exec_Old(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample)
{
	// RESALGO_OLD: old, but very fast resampler
	DEV_SMPL* CurBufL;
	DEV_SMPL* CurBufR;
	UINT32 OutPos;
	INT32 TempS32L;
	INT32 TempS32R;
	INT32 SmpCnt;	// must be signed, else I'm getting calculation errors
	INT32 CurSmpl;
	
	CurBufL = CAA->smplBufs[0];
	CurBufR = CAA->smplBufs[1];
	
	for (OutPos = 0; OutPos < length; OutPos ++)
	{
		CAA->smpLast = CAA->smpNext;
		CAA->smpP ++;
		CAA->smpNext = (UINT32)((UINT64)CAA->smpP * CAA->smpRateSrc / CAA->smpRateDst);
		if (CAA->smpLast >= CAA->smpNext)
		{
			retSample[OutPos].L += CAA->lSmpl.L * CAA->volumeL;
			retSample[OutPos].R += CAA->lSmpl.R * CAA->volumeR;
		}
		else //if (CAA->smpLast < CAA->smpNext)
		{
			SmpCnt = CAA->smpNext - CAA->smpLast;
			
			CAA->StreamUpdate(CAA->su_DataPtr, SmpCnt, CAA->smplBufs);
			
			if (SmpCnt == 1)
			{
				retSample[OutPos].L += CurBufL[0] * CAA->volumeL;
				retSample[OutPos].R += CurBufR[0] * CAA->volumeR;
				CAA->lSmpl.L = CurBufL[0];
				CAA->lSmpl.R = CurBufR[0];
			}
			else if (SmpCnt == 2)
			{
				retSample[OutPos].L += (CurBufL[0] + CurBufL[1]) * CAA->volumeL >> 1;
				retSample[OutPos].R += (CurBufR[0] + CurBufR[1]) * CAA->volumeR >> 1;
				CAA->lSmpl.L = CurBufL[1];
				CAA->lSmpl.R = CurBufR[1];
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
				retSample[OutPos].L += TempS32L * CAA->volumeL / SmpCnt;
				retSample[OutPos].R += TempS32R * CAA->volumeR / SmpCnt;
				CAA->lSmpl.L = CurBufL[SmpCnt - 1];
				CAA->lSmpl.R = CurBufR[SmpCnt - 1];
			}
		}
	}
	
	if (CAA->smpLast >= CAA->smpRateSrc)
	{
		CAA->smpLast -= CAA->smpRateSrc;
		CAA->smpNext -= CAA->smpRateSrc;
		CAA->smpP -= CAA->smpRateDst;
	}
	
	return;
}

static void Resmpl_Exec_LinearUp(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample)
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
	
	CurBufL = CAA->smplBufs[0];
	CurBufR = CAA->smplBufs[1];
	
	ChipSmpRateFP = FIXPNT_FACT * CAA->smpRateSrc;
	StreamPnt[0] = &CurBufL[2];
	StreamPnt[1] = &CurBufR[2];
	// TODO: Make it work *properly* with large blocks. (this is very hackish)
	for (OutPos = 0; OutPos < length; OutPos ++)
	{
		InPosL = (SLINT)(CAA->smpP * ChipSmpRateFP / CAA->smpRateDst);
		InPre = (UINT32)fp2i_floor(InPosL);
		InNow = (UINT32)fp2i_ceil(InPosL);
		
		CurBufL[0] = CAA->lSmpl.L;
		CurBufR[0] = CAA->lSmpl.R;
		CurBufL[1] = CAA->nSmpl.L;
		CurBufR[1] = CAA->nSmpl.R;
		if (InNow != CAA->smpNext)
			CAA->StreamUpdate(CAA->su_DataPtr, InNow - CAA->smpNext, StreamPnt);
		
		InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->smpNext * FIXPNT_FACT);
		SmpCnt = FIXPNT_FACT;
		CAA->smpLast = InPre;
		CAA->smpNext = InNow;
		//for (OutPos = 0; OutPos < length; OutPos ++)
		//{
		//InPos = InBase + (UINT32)(OutPos * ChipSmpRateFP / CAA->smpRateDst);
		InPos = InBase;
		
		InPre = fp2i_floor(InPos);
		InNow = fp2i_ceil(InPos);
		SmpFrc = getfraction(InPos);
		
		// Linear interpolation
		TempSmpL = ((INT64)CurBufL[InPre] * (FIXPNT_FACT - SmpFrc)) +
					((INT64)CurBufL[InNow] * SmpFrc);
		TempSmpR = ((INT64)CurBufR[InPre] * (FIXPNT_FACT - SmpFrc)) +
					((INT64)CurBufR[InNow] * SmpFrc);
		retSample[OutPos].L += (INT32)(TempSmpL * CAA->volumeL / SmpCnt);
		retSample[OutPos].R += (INT32)(TempSmpR * CAA->volumeR / SmpCnt);
		//}
		CAA->lSmpl.L = CurBufL[InPre];
		CAA->lSmpl.R = CurBufR[InPre];
		CAA->nSmpl.L = CurBufL[InNow];
		CAA->nSmpl.R = CurBufR[InNow];
		CAA->smpP ++;
	}
	
	if (CAA->smpLast >= CAA->smpRateSrc)
	{
		CAA->smpLast -= CAA->smpRateSrc;
		CAA->smpNext -= CAA->smpRateSrc;
		CAA->smpP -= CAA->smpRateDst;
	}
	
	return;
}

static void Resmpl_Exec_Copy(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample)
{
	// RESALGO_COPY: Copying
	UINT32 OutPos;
	
	CAA->smpNext = CAA->smpP * CAA->smpRateSrc / CAA->smpRateDst;
	CAA->StreamUpdate(CAA->su_DataPtr, length, CAA->smplBufs);
	
	for (OutPos = 0; OutPos < length; OutPos ++)
	{
		retSample[OutPos].L += CAA->smplBufs[0][OutPos] * CAA->volumeL;
		retSample[OutPos].R += CAA->smplBufs[1][OutPos] * CAA->volumeR;
	}
	CAA->smpP += length;
	CAA->smpLast = CAA->smpNext;
	
	if (CAA->smpLast >= CAA->smpRateSrc)
	{
		CAA->smpLast -= CAA->smpRateSrc;
		CAA->smpNext -= CAA->smpRateSrc;
		CAA->smpP -= CAA->smpRateDst;
	}
	
	return;
}

// TODO: The resample is not completely stable.
//	Resampling tiny blocks (1 resulting sample) sometimes causes values to be off-by-one,
//	compared to resampling large blocks.
static void Resmpl_Exec_LinearDown(RESMPL_STATE* CAA, UINT32 length, WAVE_32BS* retSample)
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
	
	CurBufL = CAA->smplBufs[0];
	CurBufR = CAA->smplBufs[1];
	
	ChipSmpRateFP = FIXPNT_FACT * CAA->smpRateSrc;
	InPosL = (SLINT)((CAA->smpP + length) * ChipSmpRateFP / CAA->smpRateDst);
	CAA->smpNext = (UINT32)fp2i_ceil(InPosL);
	
	CurBufL[0] = CAA->lSmpl.L;
	CurBufR[0] = CAA->lSmpl.R;
	StreamPnt[0] = &CurBufL[1];
	StreamPnt[1] = &CurBufR[1];
	CAA->StreamUpdate(CAA->su_DataPtr, CAA->smpNext - CAA->smpLast, StreamPnt);
	
	InPosL = (SLINT)(CAA->smpP * ChipSmpRateFP / CAA->smpRateDst);
	// I'm adding 1.0 to avoid negative indexes
	InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->smpLast * FIXPNT_FACT);
	InPosNext = InBase;
	for (OutPos = 0; OutPos < length; OutPos ++)
	{
		//InPos = InBase + (UINT32)(OutPos * ChipSmpRateFP / CAA->smpRateDst);
		InPos = InPosNext;
		InPosNext = InBase + (UINT32)((OutPos+1) * ChipSmpRateFP / CAA->smpRateDst);
		
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
		
		retSample[OutPos].L += (INT32)(TempSmpL * CAA->volumeL / SmpCnt);
		retSample[OutPos].R += (INT32)(TempSmpR * CAA->volumeR / SmpCnt);
	}
	
	CAA->lSmpl.L = CurBufL[InPre];
	CAA->lSmpl.R = CurBufR[InPre];
	CAA->smpP += length;
	CAA->smpLast = CAA->smpNext;
	
	if (CAA->smpLast >= CAA->smpRateSrc)
	{
		CAA->smpLast -= CAA->smpRateSrc;
		CAA->smpNext -= CAA->smpRateSrc;
		CAA->smpP -= CAA->smpRateDst;
	}
	
	return;
}

void Resmpl_Execute(RESMPL_STATE* CAA, UINT32 smplCount, WAVE_32BS* smplBuffer)
{
	if (! smplCount)
		return;
	
	switch(CAA->resampler)
	{
	case RESALGO_OLD:	// old, but very fast resampler
		Resmpl_Exec_Old(CAA, smplCount, smplBuffer);
		break;
	case RESALGO_LINEAR_UP:	// Upsampling
		Resmpl_Exec_LinearUp(CAA, smplCount, smplBuffer);
		break;
	case RESALGO_COPY:	// Copying
		Resmpl_Exec_Copy(CAA, smplCount, smplBuffer);
		break;
	case RESALGO_LINEAR_DOWN:	// Downsampling
		Resmpl_Exec_LinearDown(CAA, smplCount, smplBuffer);
		break;
	default:
		CAA->smpP += CAA->smpRateDst;
		break;	// do absolutely nothing
	}
	
	return;
}
