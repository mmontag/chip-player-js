#ifndef _DEBUG
#define _DEBUG	// always enable time measurements
#endif
#if defined(_DEBUG) && defined(WIN32)
#include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include <common_def.h>
#include "dblk_compr.h"

// integer types for fast integer calculation
// The bit number defines how many bits are required, but the types can be larger for increased speed.
typedef UINT16	FUINT8;
typedef UINT32	FUINT16;

typedef struct compression_parameters
{
	// Compression Types:
	//	00 bit packing
	//	01 DPCM
	UINT8 subType;
	UINT8 bitsDec;
	UINT8 bitsCmp;
	UINT16 baseVal;
	const PCM_COMPR_TBL* pcmComprTbl;
} CMP_PARAM;

#if defined(_DEBUG) && defined(WIN32)
UINT32 dblk_benchTime;
#endif


static UINT8 Decompress_BitPacking_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams);
static UINT8 Decompress_BitPacking_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams);
static UINT8 Decompress_DPCM_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams);
static UINT8 Decompress_DPCM_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams);


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

static UINT8 Decompress_BitPacking_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams)
{
	FUINT8 bitsCmp;
	FUINT8 addVal;
	const UINT8* inPos;
	const UINT8* inDataEnd;
	UINT8* outPos;
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
		ent1B = cmpParams->pcmComprTbl->values.d8;
		if (! cmpParams->pcmComprTbl->valueCount)
		{
			printf("Error loading table-compressed data block! No table loaded!\n");
			return 0x10;
		}
		else if (cmpParams->bitsDec != cmpParams->pcmComprTbl->bitsDec ||
			cmpParams->bitsCmp != cmpParams->pcmComprTbl->bitsCmp)
		{
			printf("Warning! Data block and loaded value table incompatible!\n");
			return 0x11;
		}
	}
	
	inPos = inData;
	inDataEnd = inData + inLen;
	inShift = 0;
	outShift = cmpParams->bitsDec - bitsCmp;
	outDataEnd = outData + outLen;
	
	switch(cmpParams->subType)
	{
	case 0x00:	// Copy
		for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += 1)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			outVal = inVal + addVal;
			*outPos = (UINT8)outVal;
		}
		break;
	case 0x01:	// Shift Left
		for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += 1)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			outVal = (inVal << outShift) + addVal;
			*outPos = (UINT8)outVal;
		}
		break;
	case 0x02:	// Table
		for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += 1)
		{
			READ_BITS(inPos, inVal, inShift, bitsCmp);
			
			*outPos = ent1B[inVal];
		}
		break;
	}
	
	return 0x00;
}

static UINT8 Decompress_BitPacking_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams)
{
	unsigned int cmpSubType;
	FUINT8 bitsCmp;
	FUINT16 addVal;
	const UINT8* inPos;
	const UINT8* inDataEnd;
	UINT8* outPos;
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
		ent2B = cmpParams->pcmComprTbl->values.d16;
		if (! cmpParams->pcmComprTbl->valueCount)
		{
			printf("Error loading table-compressed data block! No table loaded!\n");
			return 0x10;
		}
		else if (cmpParams->bitsDec != cmpParams->pcmComprTbl->bitsDec ||
			cmpParams->bitsCmp != cmpParams->pcmComprTbl->bitsCmp)
		{
			printf("Warning! Data block and loaded value table incompatible!\n");
			return 0x11;
		}
	}
	
	inPos = inData;
	inDataEnd = inData + inLen;
	inShift = 0;
	outShift = cmpParams->bitsDec - bitsCmp;
	outDataEnd = outData + outLen;
	
	for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += 2)
	{
		READ_BITS(inPos, inVal, inShift, bitsCmp);
		
		// For whatever reason, pulling the switch() out of the for()-loop makes it SLOWER,
		// at least in MSVC 2010.
		switch(cmpSubType)
		{
		case 0x00:	// Copy
			outVal = inVal + addVal;
			break;
		case 0x01:	// Shift Left
			outVal = (inVal << outShift) + addVal;
			break;
		case 0x02:	// Table
			outVal = ent2B[inVal];
			break;
		default:
			outVal = 0x0000;
			break;
		}
		
		// save explicitly in Little Endian
		WriteLE16(outPos, (UINT16)outVal);
	}
	
	return 0x00;
}

static UINT8 Decompress_DPCM_8(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams)
{
	FUINT8 bitsCmp;
	const UINT8* inPos;
	const UINT8* inDataEnd;
	UINT8* outPos;
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
	ent1B = cmpParams->pcmComprTbl->values.d8;
	if (! cmpParams->pcmComprTbl->valueCount)
	{
		printf("Error loading table-compressed data block! No table loaded!\n");
		return 0x10;
	}
	else if (cmpParams->bitsDec != cmpParams->pcmComprTbl->bitsDec ||
		cmpParams->bitsCmp != cmpParams->pcmComprTbl->bitsCmp)
	{
		printf("Warning! Data block and loaded value table incompatible!\n");
		return 0x11;
	}
	
	outMask = (1 << cmpParams->bitsDec) - 1;
	inPos = inData;
	inDataEnd = inData + inLen;
	inShift = 0;
	outShift = cmpParams->bitsDec - cmpParams->bitsCmp;
	outDataEnd = outData + outLen;
	
	outVal = (FUINT8)cmpParams->baseVal;
	for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += 1)
	{
		READ_BITS(inPos, inVal, inShift, bitsCmp);
		
		outVal += ent1B[inVal];
		outVal &= outMask;
		*outPos = (UINT8)outVal;
	}
	
	return 0x00;
}

static UINT8 Decompress_DPCM_16(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const CMP_PARAM* cmpParams)
{
	FUINT8 bitsCmp;
	const UINT8* inPos;
	const UINT8* inDataEnd;
	UINT8* outPos;
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
	ent2B = cmpParams->pcmComprTbl->values.d16;
	if (! cmpParams->pcmComprTbl->valueCount)
	{
		printf("Error loading table-compressed data block! No table loaded!\n");
		return 0x10;
	}
	else if (cmpParams->bitsDec != cmpParams->pcmComprTbl->bitsDec ||
		cmpParams->bitsCmp != cmpParams->pcmComprTbl->bitsCmp)
	{
		printf("Warning! Data block and loaded value table incompatible!\n");
		return 0x11;
	}
	
	outMask = (1 << cmpParams->bitsDec) - 1;
	inPos = inData;
	inDataEnd = inData + inLen;
	inShift = 0;
	outShift = cmpParams->bitsDec - cmpParams->bitsCmp;
	outDataEnd = outData + outLen;
	
	outVal = cmpParams->baseVal;
	for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += 2)
	{
		READ_BITS(inPos, inVal, inShift, bitsCmp);
		
		outVal += ent2B[inVal];
		outVal &= outMask;
		// save explicitly in Little Endian
		WriteLE16(outPos, (UINT16)outVal);
	}
	
	return 0x00;
}

UINT8 DecompressDataBlk(UINT32* outLen, UINT8** retOutData, UINT32 inLen, const UINT8* inData, const PCM_COMPR_TBL* comprTbl)
{
	UINT8* outData;
	UINT8 comprType;
	UINT8 valSize;
#if defined(_DEBUG) && defined(WIN32)
	UINT32 Time;
#endif
	UINT8 retVal;
	CMP_PARAM cParam;
	
	comprType = inData[0x00];
	*outLen = ReadLE32(&inData[0x01]);
	*retOutData = (UINT8*)realloc(*retOutData, *outLen);
	outData = *retOutData;
	
#if defined(_DEBUG) && defined(WIN32)
	Time = GetTickCount();
#endif
	switch(comprType)
	{
	case 0x00:	// Bit Packing compression
		cParam.bitsDec = inData[0x05];
		cParam.bitsCmp = inData[0x06];
		cParam.subType = inData[0x07];
		cParam.baseVal = ReadLE16(&inData[0x08]);
		cParam.pcmComprTbl = comprTbl;
		valSize = (cParam.bitsDec + 7) / 8;
		if (valSize == 0x01)
			retVal = Decompress_BitPacking_8(*outLen, outData, inLen - 0x0A, &inData[0x0A], &cParam);
		else if (valSize == 0x02)
			retVal = Decompress_BitPacking_16(*outLen, outData, inLen - 0x0A, &inData[0x0A], &cParam);
		else
			retVal = 0x20;	// invalid number of decompressed bits
		if (retVal)
		{
			*outLen = 0x00;
			return retVal;
		}
		break;
	case 0x01:	// Delta-PCM
		cParam.bitsDec = inData[0x05];
		cParam.bitsCmp = inData[0x06];
		cParam.subType = inData[0x07];	// ignored for DPCM
		cParam.baseVal = ReadLE16(&inData[0x08]);
		cParam.pcmComprTbl = comprTbl;
		valSize = (cParam.bitsDec + 7) / 8;
		if (valSize == 0x01)
			retVal = Decompress_DPCM_8(*outLen, outData, inLen - 0x0A, &inData[0x0A], &cParam);
		else if (valSize == 0x02)
			retVal = Decompress_DPCM_16(*outLen, outData, inLen - 0x0A, &inData[0x0A], &cParam);
		else
			retVal = 0x20;	// invalid number of decompressed bits
		if (retVal)
		{
			*outLen = 0x00;
			return retVal;
		}
		break;
	default:
		printf("Error: Unknown data block compression!\n");
		return 0x80;
	}
	
#if defined(_DEBUG) && defined(WIN32)
	dblk_benchTime = GetTickCount() - Time;
#endif
	
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
		printf("Warning! Bad PCM Table Length!\n");
	
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
