#include <stdlib.h>
#include <string.h>

#include "../common_def.h"
#include "dblk_compr.h"

// integer types for fast integer calculation
// The bit number defines how many bits are required, but the types can be larger for increased speed.
typedef UINT16	FUINT8;
typedef UINT32	FUINT16;


static UINT8 Decompress_BitPacking_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams);
static UINT8 Decompress_BitPacking_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams);
static UINT8 Decompress_DPCM_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams);
static UINT8 Decompress_DPCM_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams);
static UINT8 Compress_BitPacking_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams);
static UINT8 Compress_BitPacking_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams);


INLINE UINT16 ReadLE16(const UINT8* Data)
{
	// read 16-Bit Word (Little Endian/Intel Byte Order)
	return (Data[0x01] << 8) | (Data[0x00] << 0);
}

INLINE UINT32 ReadLE32(const UINT8* Data)
{
	// read 32-Bit Word (Little Endian/Intel Byte Order)
	return	(Data[0x03] << 24) | (Data[0x02] << 16) |
			(Data[0x01] <<  8) | (Data[0x00] <<  0);
}

INLINE void WriteLE16(UINT8* Buffer, UINT16 Value)
{
	Buffer[0x00] = (UINT8)(Value >> 0);
	Buffer[0x01] = (UINT8)(Value >> 8);
	
	return;
}

INLINE void WriteLE32(UINT8* Buffer, UINT32 Value)
{
	Buffer[0x00] = (UINT8)(Value >>  0);
	Buffer[0x01] = (UINT8)(Value >>  8);
	Buffer[0x02] = (UINT8)(Value >> 16);
	Buffer[0x03] = (UINT8)(Value >> 24);
	
	return;
}

// multiply and divide 32-bit integers, use 64-bit result for intermediate multiplication result
#define MUL_DIV(a, b, c)	(UINT32)((UINT64)a * b / c)

// Parameters:
//	inPos - input data pointer
//	inVal - (input data) result value
//	inShift - current bit position
//	bitCnt - bits per values
#define READ_BITS(inPos, inVal, inShift, bitCnt)	\
{	\
	outBit = 0x00;	\
	inVal = 0x0000;	\
	bitsToRead = bitCnt;	\
	while(bitsToRead > 0)	\
	{	\
		bitReadVal = (bitsToRead >= 8) ? 8 : bitsToRead;	\
		bitsToRead -= bitReadVal;	\
		bitMask = (1 << bitReadVal) - 1;	\
		\
		inShift += bitReadVal;	\
		inValB = (*inPos << inShift >> 8) & bitMask;	\
		if (inShift >= 8)	\
		{	\
			inShift -= 8;	\
			inPos ++;	\
			if (inShift)	\
				inValB |= (*inPos << inShift >> 8) & bitMask;	\
		}	\
		\
		inVal |= inValB << outBit;	\
		outBit += bitReadVal;	\
	}	\
}

// Parameters:
//	outPos - output data pointer
//	inVal - input value
//	outShift - current bit position
//	bitCnt - bits per values
// TODO: don't write outData[outLen] = 0x00 when finishing with 0 bits left
#define WRITE_BITS(outPos, inVal, outShift, bitCnt)	\
{	\
	inBit = 0x00;	\
	*outPos &= ~(0xFF >> outShift);	\
	bitsToWrite = bitCnt;	\
	while(bitsToWrite)	\
	{	\
		bitWriteVal = (bitsToWrite >= 8) ? 8 : bitsToWrite;	\
		bitsToWrite -= bitWriteVal;	\
		bitMask = (1 << bitWriteVal) - 1;	\
		\
		outValB = (inVal >> inBit) & bitMask;	\
		outShift += bitWriteVal;	\
		*outPos |= (UINT8)(outValB << 8 >> outShift);	\
		if (outShift >= 8)	\
		{	\
			outShift -= 8;	\
			outPos ++;	\
			*outPos = (UINT8)(outValB << 8 >> outShift);	\
		}	\
		\
		inBit += bitWriteVal;	\
	}	\
}

static UINT8 Decompress_BitPacking_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams)
{
	FUINT8 bitsCmp;
	FUINT8 addVal;
	const UINT8* inPos;
	UINT8* outPos;
	UINT32 outLenMax;
	const UINT8* outDataEnd;
	FUINT8 inVal;
	FUINT8 outVal;
	FUINT8 inShift;
	FUINT8 outShift;
	const UINT8* ent1B;
	
	// ReadBits Variables
	FUINT8 bitsToRead;
	FUINT8 bitReadVal;
	FUINT8 inValB;
	FUINT8 bitMask;
	FUINT8 outBit;
	
	// --- Bit Packing compression --- (8 bit output)
	bitsCmp = cmpParams->bitsCmp;
	addVal = cmpParams->baseVal & 0xFF;
	ent1B = NULL;
	if (cmpParams->subType == 0x02)
	{
		ent1B = cmpParams->comprTbl->values.d8;
		if (! cmpParams->comprTbl->valueCount)
		{
			return 0x10;	// Error: no table loaded
		}
		else if (cmpParams->bitsDec != cmpParams->comprTbl->bitsDec ||
			cmpParams->bitsCmp != cmpParams->comprTbl->bitsCmp)
		{
			return 0x11;	// Data block and loaded value table incompatible
		}
	}
	
	inShift = 0;
	outShift = cmpParams->bitsDec - bitsCmp;
	outLenMax = MUL_DIV(inLen, 8, cmpParams->bitsCmp);
	if (outLen > outLenMax)
		outLen = outLenMax;
	outDataEnd = outData + outLen;
	
	switch(cmpParams->subType)
	{
	case 0x00:	// Copy
		for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x01)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			outVal = inVal + addVal;
			*outPos = (UINT8)outVal;
		}
		break;
	case 0x01:	// Shift Left
		for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x01)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			outVal = (inVal << outShift) + addVal;
			*outPos = (UINT8)outVal;
		}
		break;
	case 0x02:	// Table
		for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x01)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			*outPos = ent1B[inVal];
		}
		break;
	}
	
	return 0x00;
}

static UINT8 Decompress_BitPacking_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams)
{
	unsigned int cmpSubType;
	FUINT8 bitsCmp;
	FUINT16 addVal;
	const UINT8* inPos;
	UINT8* outPos;
	UINT32 outLenMax;
	const UINT8* outDataEnd;
	FUINT16 inVal;
	FUINT16 outVal;
	FUINT8 inShift;
	FUINT8 outShift;
	const UINT16* ent2B;
	
	// ReadBits Variables
	FUINT8 bitsToRead;
	FUINT8 bitReadVal;
	FUINT8 inValB;
	FUINT8 bitMask;
	FUINT8 outBit;
	
	// --- Bit Packing compression --- (16 bit output)
	cmpSubType = cmpParams->subType;
	bitsCmp = cmpParams->bitsCmp;
	addVal = cmpParams->baseVal;
	ent2B = NULL;
	if (cmpParams->subType == 0x02)
	{
		ent2B = cmpParams->comprTbl->values.d16;
		if (! cmpParams->comprTbl->valueCount)
		{
			return 0x10;	// Error: no table loaded
		}
		else if (cmpParams->bitsDec != cmpParams->comprTbl->bitsDec ||
			cmpParams->bitsCmp != cmpParams->comprTbl->bitsCmp)
		{
			return 0x11;	// Data block and loaded value table incompatible
		}
	}
	
	inShift = 0;
	outShift = cmpParams->bitsDec - bitsCmp;
	outLenMax = MUL_DIV(inLen, 16, cmpParams->bitsCmp);
	if (outLen > outLenMax)
		outLen = outLenMax;
	outDataEnd = outData + outLen;
	
	switch(cmpParams->subType)
	{
	case 0x00:	// Copy
		for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x02)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			outVal = inVal + addVal;
			// save explicitly in Little Endian
			WriteLE16(outPos, (UINT16)outVal);
		}
		break;
	case 0x01:	// Shift Left
		for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x02)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			outVal = (inVal << outShift) + addVal;
			// save explicitly in Little Endian
			WriteLE16(outPos, (UINT16)outVal);
		}
		break;
	case 0x02:	// Table
		for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x02)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			outVal = ent2B[inVal];
			// save explicitly in Little Endian
			WriteLE16(outPos, (UINT16)outVal);
		}
		break;
	}
	
	return 0x00;
}

static UINT8 Decompress_DPCM_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams)
{
	FUINT8 bitsCmp;
	const UINT8* inPos;
	UINT8* outPos;
	UINT32 outLenMax;
	const UINT8* outDataEnd;
	FUINT8 inVal;
	FUINT8 outVal;
	FUINT8 inShift;
	FUINT8 outShift;
	const UINT8* ent1B;
	
	// ReadBits Variables
	FUINT8 bitsToRead;
	FUINT8 bitReadVal;
	FUINT8 inValB;
	FUINT8 bitMask;
	FUINT8 outBit;
	
	// Variables for DPCM
	UINT16 outMask;
	
	// --- Delta-PCM --- (8 bit output)
	bitsCmp = cmpParams->bitsCmp;
	ent1B = cmpParams->comprTbl->values.d8;
	if (! cmpParams->comprTbl->valueCount)
	{
		return 0x10;	// Error: no table loaded
	}
	else if (cmpParams->bitsDec != cmpParams->comprTbl->bitsDec ||
		cmpParams->bitsCmp != cmpParams->comprTbl->bitsCmp)
	{
		return 0x11;	// Data block and loaded value table incompatible
	}
	
	outMask = (1 << cmpParams->bitsDec) - 1;
	inShift = 0;
	outShift = cmpParams->bitsDec - cmpParams->bitsCmp;
	outLenMax = MUL_DIV(inLen, 8, cmpParams->bitsCmp);
	if (outLen > outLenMax)
		outLen = outLenMax;
	outDataEnd = outData + outLen;
	
	outVal = (FUINT8)cmpParams->baseVal;
	for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x01)
	{
		READ_BITS(inPos, inVal, inShift, bitsCmp);
		
		outVal += ent1B[inVal];
		outVal &= outMask;
		*outPos = (UINT8)outVal;
	}
	
	return 0x00;
}

static UINT8 Decompress_DPCM_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams)
{
	FUINT8 bitsCmp;
	const UINT8* inPos;
	UINT8* outPos;
	UINT32 outLenMax;
	const UINT8* outDataEnd;
	FUINT16 inVal;
	FUINT16 outVal;
	FUINT8 inShift;
	FUINT8 outShift;
	const UINT16* ent2B;
	
	// ReadBits Variables
	FUINT8 bitsToRead;
	FUINT8 bitReadVal;
	FUINT8 inValB;
	FUINT8 bitMask;
	FUINT8 outBit;
	
	// Variables for DPCM
	UINT16 outMask;
	
	// --- Delta-PCM --- (8 bit output)
	bitsCmp = cmpParams->bitsCmp;
	ent2B = cmpParams->comprTbl->values.d16;
	if (! cmpParams->comprTbl->valueCount)
	{
		return 0x10;	// Error: no table loaded
	}
	else if (cmpParams->bitsDec != cmpParams->comprTbl->bitsDec ||
		cmpParams->bitsCmp != cmpParams->comprTbl->bitsCmp)
	{
		return 0x11;	// Data block and loaded value table incompatible
	}
	
	outMask = (1 << cmpParams->bitsDec) - 1;
	inShift = 0;
	outShift = cmpParams->bitsDec - cmpParams->bitsCmp;
	outLenMax = MUL_DIV(inLen, 16, cmpParams->bitsCmp);
	if (outLen > outLenMax)
		outLen = outLenMax;
	outDataEnd = outData + outLen;
	
	outVal = cmpParams->baseVal;
	for (inPos = inData, outPos = outData; outPos < outDataEnd; outPos += 0x02)
	{
		READ_BITS(inPos, inVal, inShift, bitsCmp);
		
		outVal += ent2B[inVal];
		outVal &= outMask;
		// save explicitly in Little Endian
		WriteLE16(outPos, (UINT16)outVal);
	}
	
	return 0x00;
}

static UINT8 Compress_BitPacking_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams)
{
	FUINT8 bitsCmp;
	FUINT8 addVal;
	const UINT8* inPos;
	UINT8* outPos;
	UINT32 inLenMax;
	const UINT8* inDataEnd;
	FUINT8 inVal;
	FUINT8 inShift;
	FUINT8 outShift;
	UINT16 ent1Count;
	UINT8* ent1B;
	FUINT8 ent1Mask;
	UINT16 curVal;
	
	// ReadBits Variables
	FUINT8 bitsToWrite;
	FUINT8 bitWriteVal;
	FUINT8 outValB;
	FUINT8 bitMask;
	FUINT8 inBit;
	
	// --- Bit Packing compression --- (8 bit input)
	bitsCmp = cmpParams->bitsCmp;
	addVal = cmpParams->baseVal & 0xFF;
	ent1Count = 1 << cmpParams->bitsDec;
	ent1Mask = (FUINT8)(ent1Count - 1);
	ent1B = NULL;
	if (cmpParams->subType == 0x02)
	{
		ent1B = cmpParams->comprTbl->values.d8;
		if (! cmpParams->comprTbl->valueCount)
		{
			return 0x10;	// Error: no table loaded
		}
		else if (cmpParams->bitsDec != cmpParams->comprTbl->bitsDec ||
			cmpParams->bitsCmp != cmpParams->comprTbl->bitsCmp)
		{
			return 0x11;	// Data block and loaded value table incompatible
		}
		ent1B = (UINT8*)malloc(ent1Count * sizeof(UINT8));
		GenerateReverseLUT_8(ent1Count, ent1B, cmpParams->comprTbl->valueCount, cmpParams->comprTbl->values.d8);
	}
	
	outShift = 0;
	inShift = cmpParams->bitsDec - bitsCmp;
	inLenMax = MUL_DIV(outLen, 8, bitsCmp);
	if (inLen > inLenMax)
		inLen = inLenMax;
	inDataEnd = inData + inLen;
	
	switch(cmpParams->subType)
	{
	case 0x00:	// Copy
		for (inPos = inData, outPos = outData; inPos < inDataEnd; inPos += 0x01)
		{
			inVal = *inPos - addVal;
			WRITE_BITS(outPos, inVal, outShift, bitsCmp);
		}
		break;
	case 0x01:	// Shift Left
		for (inPos = inData, outPos = outData; inPos < inDataEnd; inPos += 0x01)
		{
			inVal = (*inPos - addVal) >> inShift;
			WRITE_BITS(outPos, inVal, outShift, bitsCmp);
		}
		break;
	case 0x02:	// Table
		for (inPos = inData, outPos = outData; inPos < inDataEnd; inPos += 0x01)
		{
#if 1
			for (curVal = 0x00; curVal < cmpParams->comprTbl->valueCount; curVal ++)
			{
				if (*inPos == cmpParams->comprTbl->values.d8[curVal])
					break;
			}
			inVal = (FUINT8)curVal;
#else
			inVal = ent1B[*inPos & ent1Mask];
#endif
			WRITE_BITS(outPos, inVal, outShift, bitsCmp);
		}
		break;
	}
	if (ent1B != NULL)
		free(ent1B);
	
	return 0x00;
}

static UINT8 Compress_BitPacking_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmpParams)
{
	FUINT8 bitsCmp;
	FUINT16 addVal;
	const UINT8* inPos;
	UINT8* outPos;
	UINT32 inLenMax;
	const UINT8* inDataEnd;
	FUINT16 inVal;
	FUINT8 inShift;
	FUINT8 outShift;
	UINT32 ent2Count;
	FUINT16 ent2Mask;
	UINT16* ent2B;
	//UINT16 curVal;
	
	// ReadBits Variables
	FUINT8 bitsToWrite;
	FUINT8 bitWriteVal;
	FUINT8 outValB;
	FUINT8 bitMask;
	FUINT8 inBit;
	
	// --- Bit Packing compression --- (8 bit input)
	bitsCmp = cmpParams->bitsCmp;
	addVal = cmpParams->baseVal;
	ent2Count = 1 << cmpParams->bitsDec;
	ent2Mask = (FUINT16)(ent2Count - 1);
	ent2B = NULL;
	if (cmpParams->subType == 0x02)
	{
		ent2B = cmpParams->comprTbl->values.d16;
		if (! cmpParams->comprTbl->valueCount)
		{
			return 0x10;	// Error: no table loaded
		}
		else if (cmpParams->bitsDec != cmpParams->comprTbl->bitsDec ||
			cmpParams->bitsCmp != cmpParams->comprTbl->bitsCmp)
		{
			return 0x11;	// Data block and loaded value table incompatible
		}
		ent2B = (UINT16*)malloc(ent2Count * sizeof(UINT16));
		GenerateReverseLUT_16(ent2Count, ent2B, cmpParams->comprTbl->valueCount, cmpParams->comprTbl->values.d16);
	}
	
	outShift = 0;
	inShift = cmpParams->bitsDec - bitsCmp;
	inLenMax = MUL_DIV(outLen, 16, cmpParams->bitsCmp);
	if (inLen > inLenMax)
		inLen = inLenMax;
	inDataEnd = inData + inLen;
	
	switch(cmpParams->subType)
	{
	case 0x00:	// Copy
		for (inPos = inData, outPos = outData; inPos < inDataEnd; inPos += 0x02)
		{
			inVal = ReadLE16(inPos);
			inVal = inVal - addVal;
			WRITE_BITS(outPos, inVal, outShift, bitsCmp);
		}
		break;
	case 0x01:	// Shift Left
		for (inPos = inData, outPos = outData; inPos < inDataEnd; inPos += 0x02)
		{
			inVal = ReadLE16(inPos);
			inVal = (inVal - addVal) >> inShift;
			WRITE_BITS(outPos, inVal, outShift, bitsCmp);
		}
		break;
	case 0x02:	// Table
		for (inPos = inData, outPos = outData; inPos < inDataEnd; inPos += 0x02)
		{
#if 0
			inVal = ReadLE16(inPos);
			for (curVal = 0x00; curVal < cmpParams->comprTbl->valueCount; curVal ++)
			{
				if (inVal == cmpParams->comprTbl->values.d16[curVal])
					break;
			}
			inVal = (FUINT16)curVal;
#else
			inVal = ent2B[ReadLE16(inPos) & ent2Mask];
#endif
			WRITE_BITS(outPos, inVal, outShift, bitsCmp);
		}
		break;
	}
	if (ent2B != NULL)
		free(ent2B);
	
	return 0x00;
}

UINT8 ReadComprDataBlkHdr(UINT32 inLen, const UINT8* inData, PCM_CDB_INF* retCdbInf)
{
	PCM_CMP_INF* cParam;
	UINT32 curPos;
	
	if (inLen < 0x05)
		return 0x10;	// not enough data
	
	cParam = &retCdbInf->cmprInfo;
	cParam->comprType = inData[0x00];
	retCdbInf->decmpLen = ReadLE32(&inData[0x01]);
	retCdbInf->hdrSize = 0x00;
	curPos = 0x05;
	
	switch(cParam->comprType)
	{
	case 0x00:	// Bit Packing compression
	case 0x01:	// Delta-PCM
		if (inLen < curPos + 0x05)
			return 0x10;	// not enough data
		cParam->bitsDec = inData[curPos + 0x00];
		cParam->bitsCmp = inData[curPos + 0x01];
		cParam->subType = inData[curPos + 0x02];	// ignored for DPCM
		cParam->baseVal = ReadLE16(&inData[curPos + 0x03]);
		//cParam->comprTbl = NULL;	// keep the value
		curPos += 0x05;
		break;
	default:
		return 0x80;	// Error: unknown data block compression
	}
	
	retCdbInf->hdrSize = curPos;
	return 0x00;
}

UINT8 WriteComprDataBlkHdr(UINT32 outLen, UINT8* outData, PCM_CDB_INF* cdbInf)
{
	PCM_CMP_INF* cParam;
	UINT32 curPos;
	
	if (outLen < 0x05)
		return 0x10;	// not enough data
	
	cParam = &cdbInf->cmprInfo;
	outData[0x00] = cParam->comprType;
	WriteLE32(&outData[0x01], cdbInf->decmpLen);
	cdbInf->hdrSize = 0x00;
	curPos = 0x05;
	
	switch(cParam->comprType)
	{
	case 0x00:	// Bit Packing compression
	case 0x01:	// Delta-PCM
		if (outLen < curPos + 0x05)
			return 0x10;	// not enough data
		outData[curPos + 0x00] = cParam->bitsDec;
		outData[curPos + 0x01] = cParam->bitsCmp;
		outData[curPos + 0x02] = cParam->subType;
		WriteLE16(&outData[curPos + 0x03], cParam->baseVal);
		curPos += 0x05;
		break;
	default:
		return 0x80;	// Error: unknown data block compression
	}
	
	cdbInf->hdrSize = curPos;
	return 0x00;
}

UINT8 DecompressDataBlk(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmprInfo)
{
	UINT8 valSize;
	UINT8 retVal;
	
	switch(cmprInfo->comprType)
	{
	case 0x00:	// Bit Packing compression
		valSize = (cmprInfo->bitsDec + 7) / 8;
		if (valSize == 0x01)
			retVal = Decompress_BitPacking_8(outLen, outData, inLen, inData, cmprInfo);
		else if (valSize == 0x02)
			retVal = Decompress_BitPacking_16(outLen, outData, inLen, inData, cmprInfo);
		else
			retVal = 0x20;	// invalid number of decompressed bits
		if (retVal)
			return retVal;
		break;
	case 0x01:	// Delta-PCM
		valSize = (cmprInfo->bitsDec + 7) / 8;
		if (valSize == 0x01)
			retVal = Decompress_DPCM_8(outLen, outData, inLen, inData, cmprInfo);
		else if (valSize == 0x02)
			retVal = Decompress_DPCM_16(outLen, outData, inLen, inData, cmprInfo);
		else
			retVal = 0x20;	// invalid number of decompressed bits
		if (retVal)
			return retVal;
		break;
	default:
		return 0x80;
	}
	
	return 0x00;
}

UINT8 DecompressDataBlk_VGM(UINT32* outLen, UINT8** retOutData, UINT32 inLen, const UINT8* inData, const PCM_COMPR_TBL* comprTbl)
{
	UINT8 retVal;
	PCM_CDB_INF comprInf;
	
	retVal = ReadComprDataBlkHdr(inLen, inData, &comprInf);
	if (retVal)
		return retVal;
	
	*outLen = comprInf.decmpLen;
	*retOutData = (UINT8*)realloc(*retOutData, *outLen);
	comprInf.cmprInfo.comprTbl = comprTbl;
	
	return DecompressDataBlk(*outLen, *retOutData, inLen - comprInf.hdrSize, &inData[comprInf.hdrSize], &comprInf.cmprInfo);
}

UINT8 CompressDataBlk(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_CMP_INF* cmprInfo)
{
	UINT8 valSize;
	UINT8 retVal;
	
	switch(cmprInfo->comprType)
	{
	case 0x00:	// Bit Packing compression
		valSize = (cmprInfo->bitsDec + 7) / 8;
		if (valSize == 0x01)
			retVal = Compress_BitPacking_8(outLen, outData, inLen, inData, cmprInfo);
		else if (valSize == 0x02)
			retVal = Compress_BitPacking_16(outLen, outData, inLen, inData, cmprInfo);
		else
			retVal = 0x20;	// invalid number of decompressed bits
		if (retVal)
			return retVal;
		break;
	default:
		return 0x80;
	}
	
	return 0x00;
}

void ReadPCMComprTable(UINT32 dataSize, const UINT8* data, PCM_COMPR_TBL* comprTbl)
{
	UINT8 valSize;
	UINT32 tblSize;
	
	comprTbl->comprType = data[0x00];
	comprTbl->cmpSubType = data[0x01];
	comprTbl->bitsDec = data[0x02];
	comprTbl->bitsCmp = data[0x03];
	comprTbl->valueCount = ReadLE16(&data[0x04]);
	
	valSize = (comprTbl->bitsDec + 7) / 8;
	tblSize = comprTbl->valueCount * valSize;
	
	if (dataSize < 0x06 + tblSize)
	{
		//printf("Warning! Bad PCM Table Length!\n");
		tblSize = dataSize - 0x06;
		comprTbl->valueCount = tblSize / valSize;
	}
	
	comprTbl->values.d8 = (UINT8*)realloc(comprTbl->values.d8, tblSize);
	if (valSize < 0x02)
	{
		memcpy(comprTbl->values.d8, &data[0x06], tblSize);
	}
	else
	{
#ifndef VGM_LITTLE_ENDIAN
		UINT16 curVal;
		
		for (curVal = 0x00; curVal < comprTbl->valueCount; curVal ++)
			comprTbl->values.d16[curVal] = ReadLE16(&data[0x06 + curVal * 0x02]);
#else
		memcpy(comprTbl->values.d16, &data[0x06], tblSize);
#endif
	}
	
	return;
}

UINT32 WriteCompressionTable(UINT32 dataSize, UINT8* data, PCM_COMPR_TBL* comprTbl)
{
	UINT8 valSize;
	UINT32 tblSize;
	
	valSize = (comprTbl->bitsDec + 7) / 8;
	tblSize = comprTbl->valueCount * valSize;
	
	if (dataSize < 0x06 + tblSize)
	{
		//printf("Warning! Bad PCM Table Length!\n");
		return (UINT32)-1;
	}
	
	data[0x00] = comprTbl->comprType;
	data[0x01] = comprTbl->cmpSubType = data[0x01];
	data[0x02] = comprTbl->bitsDec = data[0x02];
	data[0x03] = comprTbl->bitsCmp = data[0x03];
	WriteLE16(&data[0x04], comprTbl->valueCount);
	
	comprTbl->values.d8 = (UINT8*)realloc(comprTbl->values.d8, tblSize);
	if (valSize < 0x02)
	{
		memcpy(&data[0x06], comprTbl->values.d8, tblSize);
	}
	else
	{
#ifndef VGM_LITTLE_ENDIAN
		UINT16 curVal;
		
		for (curVal = 0x00; curVal < comprTbl->valueCount; curVal ++)
			WriteLE16(&data[0x06 + curVal * 0x02], comprTbl->values.d16[curVal]);
#else
		memcpy(&data[0x06], comprTbl->values.d16, tblSize);
#endif
	}
	
	return 0x06 + tblSize;
}

void GenerateReverseLUT_8(UINT16 dstLen, UINT8* dstLUT, UINT16 srcLen, const UINT8* srcLUT)
{
	UINT16 curSrc;
	UINT16 curDst;
	FUINT8 dist;
	UINT16 minIdx;
	FUINT8 minDist;
	
	memset(dstLUT, 0x00, dstLen * 0x01);
	for (curSrc = 0x00; curSrc < srcLen; curSrc ++)
	{
		if (srcLUT[curSrc] < dstLen)
			dstLUT[srcLUT[curSrc]] = (UINT8)curSrc;
	}
	
	for (curDst = 0x00; curDst < dstLen; curDst ++)
	{
		if (! dstLUT[curDst] && srcLUT[0] != curDst)
		{
			minDist = 0xFF;
			minIdx = 0x00;
			for (curSrc = 0x00; curSrc < srcLen; curSrc ++)
			{
				dist = (curDst > srcLUT[curSrc]) ? (curDst - srcLUT[curSrc]) : (srcLUT[curSrc] - curDst);
				// The second condition results in more algorithm-like rounding.
				if (minDist > dist || (minDist == dist && (curDst < srcLUT[curSrc])))
				{
					minDist = dist;
					minIdx = curSrc;
				}
			}
			dstLUT[curDst] = (UINT8)minIdx;
		}
	}
	
	return;
}

void GenerateReverseLUT_16(UINT32 dstLen, UINT16* dstLUT, UINT32 srcLen, const UINT16* srcLUT)
{
	UINT32 curSrc;
	UINT32 curDst;
	FUINT16 dist;
	UINT32 minIdx;
	FUINT16 minDist;
	
	memset(dstLUT, 0x00, dstLen * 0x02);
	for (curSrc = 0x00; curSrc < srcLen; curSrc ++)
	{
		if (srcLUT[curSrc] < dstLen)
			dstLUT[srcLUT[curSrc]] = (UINT16)curSrc;
	}
	
	for (curDst = 0x00; curDst < dstLen; curDst ++)
	{
		if (! dstLUT[curDst] && srcLUT[0] != curDst)
		{
			minDist = 0xFFFF;
			minIdx = 0x00;
			for (curSrc = 0x00; curSrc < srcLen; curSrc ++)
			{
				dist = (curDst > srcLUT[curSrc]) ? (curDst - srcLUT[curSrc]) : (srcLUT[curSrc] - curDst);
				// The second condition results in more algorithm-like rounding.
				if (minDist > dist || (minDist == dist && (curDst < srcLUT[curSrc])))
				{
					minDist = dist;
					minIdx = curSrc;
				}
			}
			dstLUT[curDst] = (UINT16)minIdx;
		}
	}
	
	return;
}

UINT16 DataBlkCompr_GetIntSize(void)
{
	return (sizeof(FUINT8) << 0) | (sizeof(FUINT16) << 8);
}
