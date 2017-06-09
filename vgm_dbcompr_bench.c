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


INLINE UINT32 GetSysTimeMS(void);
UINT8 DecompressDataBlk_Old(UINT32 OutDataLen, UINT8* OutData, UINT32 InDataLen, const UINT8* InData, const PCM_COMPR_TBL* comprTbl);
void CompressDataBlk_Old(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, PCM_CMP_INF* ComprTbl);

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
static UINT32 dblk_benchTime;

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
	PCM_CDB_INF cdbInf8 = {0, 0, {0x00, 0x00, 8, 3, 0x00, &PCMTbl8}};		// 3 -> 8 bits
	PCM_CDB_INF cdbInf16 = {0, 0, {0x00, 0x00, 12, 5, 0x00, &PCMTbl16}};	// 5 -> 12 bits
	PCM_CMP_INF* cmpInf8 = &cdbInf8.cmprInfo;
	PCM_CMP_INF* cmpInf16 = &cdbInf16.cmprInfo;
	
	// DecompressDataBlk Benchmark
	UINT32 dataLen;
	UINT8* data;
	UINT32 dataLenRaw;
	UINT8* dataRaw;
	UINT32 decLen;
	UINT8* decData;
	UINT32 repCntr;
	UINT32 benchTime[12];
	UINT32 startTime;
	
	dataLenRaw = BENCH_SIZE * 1048576;
	cdbInf8.decmpLen = BPACK_SIZE_DEC(dataLenRaw, cmpInf8->bitsCmp, cmpInf8->bitsDec);
	cdbInf16.decmpLen = BPACK_SIZE_DEC(dataLenRaw, cmpInf16->bitsCmp, cmpInf16->bitsDec);
	decLen = (cdbInf8.decmpLen < cdbInf16.decmpLen) ? cdbInf16.decmpLen : cdbInf8.decmpLen;
	
	dataLen = 0x0A + dataLenRaw;	// including VGM data block header
	data = (UINT8*)calloc(dataLen, 1);
	dataRaw = &data[0x0A];
	decData = (UINT8*)malloc(decLen);
	
	for (repCntr = 0; repCntr < 12; repCntr ++)
		benchTime[repCntr] = 0;
	for (repCntr = 0; repCntr < BENCH_WARM_REP + BENCH_REPEAT; repCntr ++)
	{
		printf("---- Pass %u ----\n", 1 + repCntr);
		printf("Bit-Packing (copy/8)\n");
		cmpInf8->comprType = 0x00;	// compression type: 00 - bit packing
		cmpInf8->subType = 0x00;	// compression sub-type: 00 - copy
		
		WriteComprDataBlkHdr(dataLen, data, &cdbInf8);
		DecompressDataBlk_Old(decLen, decData, dataLen, data, cmpInf8->comprTbl);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[0] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		
		startTime = GetSysTimeMS();
		DecompressDataBlk(decLen, decData, dataLenRaw, dataRaw, cmpInf8);
		dblk_benchTime = GetSysTimeMS() - startTime;
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[1] += dblk_benchTime;
		
		CompressDataBlk_Old(dataLen, data, cdbInf8.decmpLen, decData, cmpInf8);
		printf("Compression Time [old]: %u\n", dblk_benchTime);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[8] += dblk_benchTime;
		
		startTime = GetSysTimeMS();
		CompressDataBlk(dataLen, data, cdbInf8.decmpLen, decData, cmpInf8);
		dblk_benchTime = GetSysTimeMS() - startTime;
		printf("Compression Time [new]: %u\n", dblk_benchTime);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[9] += dblk_benchTime;
		
		printf("DPCM (8)\n");
		cmpInf8->comprType = 0x01;	// compression type: 01 - DPCM (subtype ignored)
		WriteComprDataBlkHdr(dataLen, data, &cdbInf8);
		DecompressDataBlk_Old(decLen, decData, dataLen, data, cmpInf8->comprTbl);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[2] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		
		startTime = GetSysTimeMS();
		DecompressDataBlk(decLen, decData, dataLenRaw, dataRaw, cmpInf8);
		dblk_benchTime = GetSysTimeMS() - startTime;
		if (repCntr >= BENCH_WARM_REP)
			benchTime[3] += dblk_benchTime;
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		
		printf("Bit-Packing (copy/16)\n");
		cmpInf16->comprType = 0x00;	// compression type: 00 - bit packing
		cmpInf16->subType = 0x00;	// compression sub-type: 00 - copy
		
		WriteComprDataBlkHdr(dataLen, data, &cdbInf16);
		DecompressDataBlk_Old(decLen, decData, dataLen, data, cmpInf16->comprTbl);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[4] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		
		startTime = GetSysTimeMS();
		DecompressDataBlk(decLen, decData, dataLenRaw, dataRaw, cmpInf16);
		dblk_benchTime = GetSysTimeMS() - startTime;
		if (repCntr >= BENCH_WARM_REP)
			benchTime[5] += dblk_benchTime;
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		
		CompressDataBlk_Old(dataLen, data, cdbInf16.decmpLen, decData, cmpInf16);
		printf("Compression Time [old]: %u\n", dblk_benchTime);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[10] += dblk_benchTime;
		
		startTime = GetSysTimeMS();
		CompressDataBlk(dataLen, data, cdbInf16.decmpLen, decData, cmpInf16);
		dblk_benchTime = GetSysTimeMS() - startTime;
		printf("Compression Time [new]: %u\n", dblk_benchTime);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[11] += dblk_benchTime;
		
		printf("DPCM (16)\n");
		cmpInf16->comprType = 0x01;	// compression type: 01 - DPCM (subtype ignored)
		WriteComprDataBlkHdr(dataLen, data, &cdbInf16);
		DecompressDataBlk_Old(decLen, decData, dataLen, data, cmpInf16->comprTbl);
		if (repCntr >= BENCH_WARM_REP)
			benchTime[6] += dblk_benchTime;
		printf("Decompression Time [old]: %u\n", dblk_benchTime);
		
		startTime = GetSysTimeMS();
		DecompressDataBlk(decLen, decData, dataLenRaw, dataRaw, cmpInf16);
		dblk_benchTime = GetSysTimeMS() - startTime;
		if (repCntr >= BENCH_WARM_REP)
			benchTime[7] += dblk_benchTime;
		printf("Decompression Time [new]: %u\n", dblk_benchTime);
		fflush(stdout);
	}
	free(data);
	free(decData);
	
	for (repCntr = 0; repCntr < 12; repCntr ++)
		benchTime[repCntr] = (benchTime[repCntr] + BENCH_REPEAT / 2) / BENCH_REPEAT;
	printf("\nAverage Times (decompression):\n");
	printf("Bit-Packing (copy/8): old %u, new %u\n", benchTime[0], benchTime[1]);
	printf("DPCM (8): old %u, new %u\n", benchTime[2], benchTime[3]);
	printf("Bit-Packing (copy/16): old %u, new %u\n", benchTime[4], benchTime[5]);
	printf("DPCM (16): old %u, new %u\n", benchTime[6], benchTime[7]);
	printf("\nAverage Times (compression):\n");
	printf("Bit-Packing (copy/8): old %u, new %u\n", benchTime[8], benchTime[9]);
	printf("Bit-Packing (copy/16): old %u, new %u\n", benchTime[10], benchTime[11]);
	
	printf("Done.\n");
#ifdef _MSC_VER
	getchar();
#endif
	return 0;
}

INLINE UINT32 GetSysTimeMS(void)
{
#ifdef WIN32
	return GetTickCount();
#else
	return 0;
#endif
}

void compression_test(void)
{
	PCM_CDB_INF cdbInf8 = {0, 0, {0x00, 0x01, 8, 4, 0x00, NULL}};		// 8 -> 4 bits
	PCM_CDB_INF cdbInf16 = {0, 0, {0x00, 0x00, 12, 8, 0x180, NULL}};	// 12 -> 8 bits
	PCM_CMP_INF* cmpInf8 = &cdbInf8.cmprInfo;
	PCM_CMP_INF* cmpInf16 = &cdbInf16.cmprInfo;
	
	UINT32 uncLen;
	UINT8* uncData;
	UINT32 cmpLen;
	UINT8* cmpData;
	FILE* hFile;
	
	hFile = fopen("demofiles/32XSample_Celtic.raw", "rb");
	fseek(hFile, 0, SEEK_END);
	uncLen = ftell(hFile);
	fseek(hFile, 0, SEEK_SET);
	uncData = (UINT8*)malloc(uncLen);
	fread(uncData, 1, uncLen, hFile);
	fclose(hFile);	hFile = NULL;
	
	cmpLen = BPACK_SIZE_CMP(uncLen, cmpInf16->bitsCmp, cmpInf16->bitsDec) * 2;
	cmpData = (UINT8*)malloc(cmpLen);
	
	CompressDataBlk_Old(cmpLen, cmpData, uncLen, uncData, cmpInf16);
	hFile = fopen("demofiles/32XSample_Celtic_old.cmp", "wb");
	fwrite(cmpData, 1, cmpLen, hFile);
	fclose(hFile);	hFile = NULL;
	free(cmpData);
	
	cmpData = (UINT8*)malloc(cmpLen);
	CompressDataBlk(cmpLen, cmpData, uncLen, uncData, cmpInf16);
	hFile = fopen("demofiles/32XSample_Celtic_new.cmp", "wb");
	fwrite(cmpData, 1, cmpLen, hFile);
	fclose(hFile);	hFile = NULL;
	free(cmpData);
	
	return;
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

UINT8 DecompressDataBlk_Old(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, const PCM_COMPR_TBL* comprTbl)
{
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
	UINT32 Time;
	
	// ReadBits Variables
	FUINT8 bitsToRead;
	FUINT8 bitReadVal;
	FUINT8 inValB;
	FUINT8 bitMask;
	FUINT8 outBit;
	
	// Variables for DPCM
	UINT16 OutMask;
	
	comprType = inData[0x00];
	//*outLen = ReadLE32(&inData[0x01]);
	//*retOutData = (UINT8*)realloc(*retOutData, *outLen);
	//outData = *retOutData;
	
	Time = GetSysTimeMS();
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
				printf("Error loading table-compressed data block! No table loaded!\n");
				return 0x10;
			}
			else if (bitsDec != PCMTbl.bitsDec || bitsCmp != PCMTbl.bitsCmp)
			{
				printf("Warning! Data block and loaded value table incompatible!\n");
				return 0x11;
			}
		}
		
		valSize = (bitsDec + 7) / 8;
		inPos = inData + 0x0A;
		inDataEnd = inData + inLen;
		inShift = 0;
		outShift = bitsDec - bitsCmp;
		outDataEnd = outData + outLen;
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
				printf("Error loading table-compressed data block! No table loaded!\n");
				return 0x10;
			}
			else if (bitsDec != PCMTbl.bitsDec || bitsCmp != PCMTbl.bitsCmp)
			{
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
		outDataEnd = outData + outLen;
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
	dblk_benchTime = GetSysTimeMS() - Time;
	
	return 0x00;
}

// Notes: writes outData[outLen] = 0x00 when finishing with 0 bits left
INLINE void WriteBits(UINT8* Data, UINT32* Pos, UINT8* BitPos, UINT16 Value, UINT8 BitsToWrite)
{
	UINT8 BitWriteVal;
	UINT32 OutPos;
	UINT8 InVal;
	UINT8 BitMask;
	UINT8 OutShift;
	UINT8 InBit;
	
	OutPos = *Pos;
	OutShift = *BitPos;
	InBit = 0x00;
	
	Data[OutPos] &= ~(0xFF >> OutShift);
	while(BitsToWrite)
	{
		BitWriteVal = (BitsToWrite >= 8) ? 8 : BitsToWrite;
		BitsToWrite -= BitWriteVal;
		BitMask = (1 << BitWriteVal) - 1;
		
		InVal = (Value >> InBit) & BitMask;
		OutShift += BitWriteVal;
		Data[OutPos] |= InVal << 8 >> OutShift;
		if (OutShift >= 8)
		{
			OutShift -= 8;
			OutPos ++;
			Data[OutPos] = InVal << 8 >> OutShift;
		}
		
		InBit += BitWriteVal;
	}
	
	*Pos = OutPos;
	*BitPos = OutShift;
	return;
}

void CompressDataBlk_Old(UINT32 outLen, UINT8* outData, UINT32 inLen, const UINT8* inData, PCM_CMP_INF* ComprTbl)
{
	UINT8 valSize;
	
	UINT32 CurPos;
	UINT8* DstBuf;
	UINT32 DstPos;
	UINT16 SrcVal;
	UINT16 DstVal;
	UINT32 CurEnt;
	UINT16 BitMask;
	UINT16 AddVal;
	UINT8 BitShift;
	const UINT8* Ent1B;
	const UINT16* Ent2B;
	UINT8 DstShift;
	UINT32 Time;
	
	valSize = (ComprTbl->bitsDec + 7) / 8;
	
	DstBuf = outData;
	
	BitMask = (1 << ComprTbl->bitsCmp) - 1;
	AddVal = ComprTbl->baseVal;
	BitShift = ComprTbl->bitsDec - ComprTbl->bitsCmp;
	if (ComprTbl->comprTbl == NULL)
	{
		Ent1B = NULL;
		Ent2B = NULL;
	}
	else
	{
		Ent1B = ComprTbl->comprTbl->values.d8;
		Ent2B = ComprTbl->comprTbl->values.d16;
	}
	
	Time = GetSysTimeMS();
	SrcVal = 0x0000;	// must be initialized (else 8-bit values don't fill it completely)
	DstPos = 0x00;
	DstShift = 0;
	for (CurPos = 0x00; CurPos < inLen; CurPos += valSize)
	{
		memcpy(&SrcVal, &inData[CurPos], valSize);
		switch(ComprTbl->comprType)
		{
		case 0x00:	// Copy
			DstVal = SrcVal - AddVal;
			break;
		case 0x01:	// Shift Left
			DstVal = (SrcVal - AddVal) >> BitShift;
			break;
		case 0x02:	// Table
			switch(valSize)
			{
			case 0x01:
				for (CurEnt = 0x00; CurEnt < ComprTbl->comprTbl->valueCount; CurEnt ++)
				{
					if (SrcVal == Ent1B[CurEnt])
						break;
				}
				break;
			case 0x02:
				for (CurEnt = 0x00; CurEnt < ComprTbl->comprTbl->valueCount; CurEnt ++)
				{
					if (SrcVal == Ent2B[CurEnt])
						break;
				}
				break;
			}
			DstVal = (UINT16)CurEnt;
			break;
		}
		
		WriteBits(DstBuf, &DstPos, &DstShift, DstVal, ComprTbl->bitsCmp);
	}
	if (DstShift)
		DstPos ++;
	dblk_benchTime = GetSysTimeMS() - Time;
	
	return;
}
