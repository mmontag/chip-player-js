#ifndef __DBLK_COMPR_H__
#define __DBLK_COMPR_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdtype.h>

typedef struct pcm_decompression_table
{
	UINT8 comprType;
	UINT8 cmpSubType;
	UINT8 bitsDec;
	UINT8 bitsCmp;
	UINT16 valueCount;
	union
	{
		UINT8* d8;
		UINT16* d16;	// note: stored in Native Endian
	} values;
} PCM_COMPR_TBL;

UINT8 DecompressDataBlk(UINT32* outLen, UINT8** retOutData, UINT32 inLen, const UINT8* inData, const PCM_COMPR_TBL* comprTbl);
void ReadPCMComprTable(UINT32 dataSize, const UINT8* data, PCM_COMPR_TBL* comprTbl);

#ifdef __cplusplus
}
#endif

#endif	// __DBLK_COMPR_H__
