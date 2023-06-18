// String Utilities: Character Codepage Conversion
// ----------------
// using libiconv

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifdef _MSC_VER
#define strdup		_strdup
#define stricmp		_stricmp
#define strnicmp	_strnicmp
#else
#ifdef _POSIX_C_SOURCE
#include <strings.h>
#endif
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#include <iconv.h>
#include <errno.h>

#include "../stdtype.h"
#include "StrUtils.h"

//typedef struct _codepage_conversion CPCONV;
struct _codepage_conversion
{
	char* cpFrom;
	char* cpTo;
	iconv_t hIConv;
	size_t cpfCharSize;
	size_t cptCharSize;
};


static size_t GetEncodingCharSize(const char* encoding)
{
	if (! strnicmp(encoding, "UCS-2", 5) || ! strnicmp(encoding, "UTF-16", 6))
		return sizeof(UINT16);
	else if (! strnicmp(encoding, "UCS-4", 5) || ! strnicmp(encoding, "UTF-32", 6))
		return sizeof(UINT32);
	else
		return sizeof(char);
}

static size_t GetStrSize(const char* str, size_t charBytes)
{
	if (charBytes == 1)
	{
		return strlen(str) * charBytes;
	}
	else if (charBytes == 2)
	{
		const UINT16* u16str = (const UINT16*)str;
		const UINT16* ptr;
		for (ptr = u16str; *ptr != 0; ptr ++)
			;
		return (ptr - u16str) * charBytes;
	}
	else if (charBytes == 4)
	{
		const UINT32* u32str = (const UINT32*)str;
		const UINT32* ptr;
		for (ptr = u32str; *ptr != 0; ptr ++)
			;
		return (ptr - u32str) * charBytes;
	}
	return 0;
}

UINT8 CPConv_Init(CPCONV** retCPC, const char* cpFrom, const char* cpTo)
{
	CPCONV* cpc;
	
	cpc = (CPCONV*)calloc(1, sizeof(CPCONV));
	if (cpc == NULL)
		return 0xFF;
	
	cpc->cpFrom = strdup(cpFrom);
	cpc->cpTo = strdup(cpTo);
	cpc->hIConv = iconv_open(cpc->cpTo, cpc->cpFrom);
	if (cpc->hIConv == (iconv_t)-1)
	{
		free(cpc->cpFrom);
		free(cpc->cpTo);
		free(cpc);
		return 0x80;
	}
	cpc->cpfCharSize = GetEncodingCharSize(cpc->cpFrom);
	cpc->cptCharSize = GetEncodingCharSize(cpc->cpTo);
	
	*retCPC = cpc;
	return 0x00;
}

void CPConv_Deinit(CPCONV* cpc)
{
	iconv_close(cpc->hIConv);
	free(cpc->cpFrom);
	free(cpc->cpTo);
	
	free(cpc);
	
	return;
}

UINT8 CPConv_StrConvert(CPCONV* cpc, size_t* outSize, char** outStr, size_t inSize, const char* inStr)
{
	size_t outBufSize;
	size_t remBytesIn;
	size_t remBytesOut;
	char* inPtr;
	char* outPtr;
	size_t wrtBytes;
	char resVal;
	UINT8 canRealloc;
	char* outPtrPrev;
	
	iconv(cpc->hIConv, NULL, NULL, NULL, NULL);	// reset conversion state
	
	if (! inSize)
		inSize = GetStrSize(inStr, cpc->cpfCharSize) + cpc->cpfCharSize;	// include \0 terminator
	remBytesIn = inSize;
	inPtr = (char*)&inStr[0];	// const-cast due to a bug in the iconv API
	if (remBytesIn == 0)
	{
		*outSize = 0;
		return 0x00;	// nothing to convert
	}
	
	if (*outStr == NULL)
	{
		outBufSize = remBytesIn * cpc->cptCharSize * 3 / 2 / cpc->cpfCharSize;
		*outStr = (char*)malloc(outBufSize);
		canRealloc = 1;
	}
	else
	{
		outBufSize = *outSize;
		canRealloc = 0;
	}
	remBytesOut = outBufSize;
	outPtr = *outStr;
	
	resVal = 0x00;	// default: conversion successfull
	outPtrPrev = NULL;
	wrtBytes = iconv(cpc->hIConv, &inPtr, &remBytesIn, &outPtr, &remBytesOut);
	while(wrtBytes == (size_t)-1)
	{
		int err = errno;
		if (err == EILSEQ || err == EINVAL)
		{
			// invalid encoding
			resVal = 0x80;
			if (err == EINVAL && remBytesIn <= 1)
			{
				// assume that the string got truncated
				iconv(cpc->hIConv, NULL, NULL, &outPtr, &remBytesOut);
				resVal = 0x01;	// conversion incomplete due to input error
			}
			break;
		}
		// err == E2BIG
		if (! canRealloc)
		{
			resVal = 0x10;	// conversion incomplete due to lack of buffer space
			break;
		}
		if (outPtrPrev == outPtr)
		{
			resVal = 0xF0;	// unexpected conversion error
			break;
		}
		wrtBytes = outPtr - *outStr;
		outBufSize += remBytesIn * 2;
		*outStr = (char*)realloc(*outStr, outBufSize);
		outPtr = *outStr + wrtBytes;
		remBytesOut = outBufSize - wrtBytes;
		
		outPtrPrev = outPtr;
		wrtBytes = iconv(cpc->hIConv, &inPtr, &remBytesIn, &outPtr, &remBytesOut);
	}
	
	*outSize = outPtr - *outStr;
	return resVal;
}
