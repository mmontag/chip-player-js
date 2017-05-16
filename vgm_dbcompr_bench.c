#ifndef _DEBUG
#define _DEBUG	// always enable time measurements
#endif
#if defined(_DEBUG) && defined(WIN32)
#include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include <common_def.h>
#include "vgm/dblk_compr.h"


UINT8 DecompressDataBlk_Old(UINT32* OutDataLen, UINT8** OutData, UINT32 InDataLen, const UINT8* InData, PCM_COMPR_TBL* comprTbl);

// ---- Benchmarks ----
// 256 MB of input data
//	8-bit version: decompressed from 3 -> 8 bits (fits into UINT8)
//	16-bit version: decompressed from 5 -> 12 bits (fits into UINT16)
//
//										FUINT8/FUINT16 bits
//	Algorithm	Compiler	old 	32/32	16/16	32/16	16/32	8/16
//	--------	--------	----	----	----	----	----	----
// MS VC 2010, 32-bit executable, compiled with /O2
//	BitPack 8	MSVC 2010	10249	 6935	 5366	 6884	 5296	 5386
//	DPCM 8		MSVC 2010	 8186	 5924	 6096	 5792	 5963	 5776
//	BitPack 16	MSVC 2010	 3971	 3105	 4973	 4805	 2979	 5070
//	DPCM 16		MSVC 2010	 3705	 3210	 3674	 3296	 3506	 3190

// GCC 4.8.3, 32-bit executable, compiled with -O2 (-O3 performs slightly worse)
//	BitPack 8	GCC 6		 8908	 7313	 6716	 7141	 6701	 7094
//	DPCM 8		GCC 6		 9957	 9298	 8787	 9141	 8420	 8561
//	BitPack 16	GCC 6		 5086	 4345	 4298	 4734	 3904	 4119
//	DPCM 16		GCC 6		 5449	 4318	 3916	 4762	 4372	 4465

#define BENCH_SIZE		256	// compressed data size in MB
#define BENCH_WARM_REP	1	// number of times for warm up
#define BENCH_REPEAT	4	// number of times the benchmark is repeated
extern UINT32 dblk_benchTime;

int main(int argc, char* argv[])
{
	UINT8 DPCMTbl[0x20] =
	{
		0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
		0x80,-0x01,-0x02,-0x04,-0x08,-0x10,-0x20,-0x40,
		0x00, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04,
		0x80,-0x10,-0x20,-0x40,-0x80,-0x01,-0x02,-0x04
	};
	PCM_COMPR_TBL PCMTbl8 = {0x01, 0, 8, 3, 0x20, {DPCMTbl}};
	PCM_COMPR_TBL PCMTbl16 = {0x01, 0, 12, 5, 0x20, {DPCMTbl}};
	
	// DecompressDataBlk Benchmark
	UINT32 dataLen;
	UINT8* data;
	UINT32 decLen;
	UINT8* decData = NULL;
	UINT32 repCntr;
	UINT32 benchTime[8];
	
	dataLen = 0x0A + BENCH_SIZE * 1048576;
	data = (UINT8*)malloc(dataLen);
	data[0x05] = 8;	// decompressed bits
	data[0x06] = 3;	// compressed bits
	data[0x08] = data[0x09] = 0x00;	// add value
	decLen = (dataLen - 0x0A) * data[0x05] / data[0x06];
	memcpy(&data[0x01], &decLen, 0x04);
	
	for (repCntr = 0; repCntr < 8; repCntr ++)
		benchTime[repCntr] = 0;
	for (repCntr = 0; repCntr < BENCH_WARM_REP + BENCH_REPEAT; repCntr ++)
	{
		printf("---- Pass %u ----\n", 1 + repCntr);
		data[0x05] = 8;	// decompressed bits
		data[0x06] = 3;	// compressed bits
		printf("Bit-Packing (copy/8)\n");
		data[0x00] = 0x00;	// compression type: 00 - bit packing
		data[0x07] = 0x00;	// compression sub-type: 00 - copy
		DecompressDataBlk_Old(&decLen, &decData, dataLen, data, NULL);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[0] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		DecompressDataBlk(&decLen, &decData, dataLen, data, NULL);
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[1] += dblk_benchTime;
		
		printf("DPCM (8)\n");
		data[0x00] = 0x01;	// compression type: 01 - DPCM (subtype ignored)
		DecompressDataBlk_Old(&decLen, &decData, dataLen, data, &PCMTbl8);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[2] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		DecompressDataBlk(&decLen, &decData, dataLen, data, &PCMTbl8);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[3] += dblk_benchTime;
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		
		data[0x05] = 12;	// decompressed bits
		data[0x06] = 5;	// compressed bits
		printf("Bit-Packing (copy/16)\n");
		data[0x00] = 0x00;	// compression type: 00 - bit packing
		data[0x07] = 0x00;	// compression sub-type: 00 - copy
		DecompressDataBlk_Old(&decLen, &decData, dataLen, data, NULL);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[4] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		DecompressDataBlk(&decLen, &decData, dataLen, data, NULL);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[5] += dblk_benchTime;
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		
		printf("DPCM (16)\n");
		data[0x00] = 0x01;	// compression type: 01 - DPCM (subtype ignored)
		DecompressDataBlk_Old(&decLen, &decData, dataLen, data, &PCMTbl16);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[6] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		DecompressDataBlk(&decLen, &decData, dataLen, data, &PCMTbl16);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[7] += dblk_benchTime;
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		fflush(stdout);
	}
	
	for (repCntr = 0; repCntr < 8; repCntr ++)
		benchTime[repCntr] = (benchTime[repCntr] + BENCH_REPEAT / 2) / BENCH_REPEAT;
	printf("\nAverage Times:\n");
	printf("Bit-Packing (copy/8): old %u, new %u\n", benchTime[0], benchTime[1]);
	printf("DPCM (8): old %u, new %u\n", benchTime[2], benchTime[3]);
	printf("Bit-Packing (copy/16): old %u, new %u\n", benchTime[4], benchTime[5]);
	printf("DPCM (16): old %u, new %u\n", benchTime[6], benchTime[7]);
	
	printf("Done.\n");
#ifdef _MSC_VER
	getchar();
#endif
	return 0;
}


#define FUINT8	unsigned int
#define FUINT16	unsigned int

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

UINT8 DecompressDataBlk_Old(UINT32* outLen, UINT8** retOutData, UINT32 inLen, const UINT8* inData, PCM_COMPR_TBL* comprTbl)
{
	UINT8* outData;
	UINT8 comprType;
	UINT8 bitsDec;
	FUINT8 bitsCmp;
	UINT8 cmpSubType;
	UINT16 addVal;
	const UINT8* inPos;
	const UINT8* inDataEnd;
	UINT8* outPos;
	const UINT8* outDataEnd;
	FUINT16 inVal;
	FUINT16 outVal;
	FUINT8 valSize;
	FUINT8 inShift;
	FUINT8 outShift;
	UINT8* ent1B;
	UINT16* ent2B;
#if defined(_DEBUG) && defined(WIN32)
	UINT32 Time;
#endif
	
	// ReadBits Variables
	FUINT8 bitsToRead;
	FUINT8 bitReadVal;
	FUINT8 inValB;
	FUINT8 bitMask;
	FUINT8 outBit;
	
	// Variables for DPCM
	UINT16 OutMask;
	
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
		bitsDec = inData[0x05];
		bitsCmp = inData[0x06];
		cmpSubType = inData[0x07];
		addVal = ReadLE16(&inData[0x08]);
		ent1B = NULL;
		ent2B = NULL;
		
		if (cmpSubType == 0x02)
		{
			PCM_COMPR_TBL PCMTbl = *comprTbl;
			ent1B = PCMTbl.values.d8;	// Big Endian note: Those are stored in LE and converted when reading.
			ent2B = PCMTbl.values.d16;
			if (! PCMTbl.valueCount)
			{
				*outLen = 0x00;
				printf("Error loading table-compressed data block! No table loaded!\n");
				return 0x10;
			}
			else if (bitsDec != PCMTbl.bitsDec || bitsCmp != PCMTbl.bitsCmp)
			{
				*outLen = 0x00;
				printf("Warning! Data block and loaded value table incompatible!\n");
				return 0x11;
			}
		}
		
		valSize = (bitsDec + 7) / 8;
		inPos = inData + 0x0A;
		inDataEnd = inData + inLen;
		inShift = 0;
		outShift = bitsDec - bitsCmp;
		outDataEnd = outData + *outLen;
		outVal = 0x0000;
		
		for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += valSize)
		{
			//inVal = ReadBits(inData, inPos, &inShift, bitsCmp);
			// inlined - is 30% faster
			outBit = 0x00;
			inVal = 0x0000;
			bitsToRead = bitsCmp;
			while(bitsToRead)
			{
				bitReadVal = (bitsToRead >= 8) ? 8 : bitsToRead;
				bitsToRead -= bitReadVal;
				bitMask = (1 << bitReadVal) - 1;
				
				inShift += bitReadVal;
				inValB = (*inPos << inShift >> 8) & bitMask;
				if (inShift >= 8)
				{
					inShift -= 8;
					inPos ++;
					if (inShift)
						inValB |= (*inPos << inShift >> 8) & bitMask;
				}
				
				inVal |= inValB << outBit;
				outBit += bitReadVal;
			}
			
			switch(cmpSubType)
			{
			case 0x00:	// Copy
				outVal = inVal + addVal;
				break;
			case 0x01:	// Shift Left
				outVal = (inVal << outShift) + addVal;
				break;
			case 0x02:	// Table
				switch(valSize)
				{
				case 0x01:
					outVal = ent1B[inVal];
					break;
				case 0x02:
#ifdef VGM_LITTLE_ENDIAN
					outVal = ent2B[inVal];
#else
					outVal = ReadLE16((UINT8*)&ent2B[inVal]);
#endif
					break;
				}
				break;
			}
			
#ifdef VGM_LITTLE_ENDIAN
			//memcpy(outPos, &outVal, valSize);
			if (valSize == 0x01)
				*((UINT8*)outPos) = (UINT8)outVal;
			else //if (valSize == 0x02)
				*((UINT16*)outPos) = (UINT16)outVal;
#else
			if (valSize == 0x01)
			{
				*outPos = (UINT8)outVal;
			}
			else //if (valSize == 0x02)
			{
				// save explicitly in Little Endian
				outPos[0x00] = (UINT8)((outVal & 0x00FF) >> 0);
				outPos[0x01] = (UINT8)((outVal & 0xFF00) >> 8);
			}
#endif
		}
		break;
	case 0x01:	// Delta-PCM
		bitsDec = inData[0x05];
		bitsCmp = inData[0x06];
		outVal = ReadLE16(&inData[0x08]);
		
		{
			PCM_COMPR_TBL PCMTbl = *comprTbl;
			ent1B = PCMTbl.values.d8;
			ent2B = PCMTbl.values.d16;
			if (! PCMTbl.valueCount)
			{
				*outLen = 0x00;
				printf("Error loading table-compressed data block! No table loaded!\n");
				return 0x10;
			}
			else if (bitsDec != PCMTbl.bitsDec || bitsCmp != PCMTbl.bitsCmp)
			{
				*outLen = 0x00;
				printf("Warning! Data block and loaded value table incompatible!\n");
				return 0x11;
			}
		}
		
		valSize = (bitsDec + 7) / 8;
		OutMask = (1 << bitsDec) - 1;
		inPos = inData + 0x0A;
		inDataEnd = inData + inLen;
		inShift = 0;
		outShift = bitsDec - bitsCmp;
		outDataEnd = outData + *outLen;
		addVal = 0x0000;
		
		for (outPos = outData; outPos < outDataEnd && inPos < inDataEnd; outPos += valSize)
		{
			//inVal = ReadBits(inData, inPos, &inShift, bitsCmp);
			// inlined - is 30% faster
			outBit = 0x00;
			inVal = 0x0000;
			bitsToRead = bitsCmp;
			while(bitsToRead)
			{
				bitReadVal = (bitsToRead >= 8) ? 8 : bitsToRead;
				bitsToRead -= bitReadVal;
				bitMask = (1 << bitReadVal) - 1;
				
				inShift += bitReadVal;
				inValB = (*inPos << inShift >> 8) & bitMask;
				if (inShift >= 8)
				{
					inShift -= 8;
					inPos ++;
					if (inShift)
						inValB |= (*inPos << inShift >> 8) & bitMask;
				}
				
				inVal |= inValB << outBit;
				outBit += bitReadVal;
			}
			
			switch(valSize)
			{
			case 0x01:
				addVal = ent1B[inVal];
				outVal += addVal;
				outVal &= OutMask;
				*((UINT8*)outPos) = (UINT8)outVal;
				break;
			case 0x02:
#ifdef VGM_LITTLE_ENDIAN
				addVal = ent2B[inVal];
				outVal += addVal;
				outVal &= OutMask;
				*((UINT16*)outPos) = (UINT16)outVal;
#else
				addVal = ReadLE16((UINT8*)&ent2B[inVal]);
				outVal += addVal;
				outVal &= OutMask;
				outPos[0x00] = (UINT8)((outVal & 0x00FF) >> 0);
				outPos[0x01] = (UINT8)((outVal & 0xFF00) >> 8);
#endif
				break;
			}
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
