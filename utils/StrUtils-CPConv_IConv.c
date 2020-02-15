// String Utilities: Character Codepage Conversion
// ----------------
// using libiconv

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifdef _MSC_VER
#define strdup	_strdup
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
};

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
		return 0x80;
	}
	
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
	
	iconv(cpc->hIConv, NULL, NULL, NULL, NULL);	// reset conversion state
	
	remBytesIn = inSize ? inSize : strlen(inStr);
	inPtr = (char*)&inStr[0];	// const-cast due to a bug in the API
	if (remBytesIn == 0)
	{
		*outSize = 0;
		return 0x02;	// nothing to convert
	}
	
	if (*outStr == NULL)
	{
		outBufSize = remBytesIn * 3 / 2;
		*outStr = (char*)malloc(outBufSize);
	}
	else
	{
		outBufSize = *outSize;
	}
	remBytesOut = outBufSize;
	outPtr = *outStr;
	
	resVal = 0x00;
	wrtBytes = iconv(cpc->hIConv, &inPtr, &remBytesIn, &outPtr, &remBytesOut);
	while(wrtBytes == (size_t)-1)
	{
		if (errno == EILSEQ || errno == EINVAL)
		{
			// invalid encoding
			resVal = 0x80;
			if (errno == EINVAL && remBytesIn <= 1)
			{
				// assume that the string got truncated
				iconv(cpc->hIConv, NULL, NULL, &outPtr, &remBytesOut);
				resVal = 0x01;
			}
			break;
		}
		// errno == E2BIG
		wrtBytes = outPtr - *outStr;
		outBufSize += remBytesIn * 2;
		*outStr = (char*)realloc(*outStr, outBufSize);
		outPtr = *outStr + wrtBytes;
		remBytesOut = outBufSize - wrtBytes;
		
		wrtBytes = iconv(cpc->hIConv, &inPtr, &remBytesIn, &outPtr, &remBytesOut);
	}
	
	*outSize = outPtr - *outStr;
	return resVal;
}
