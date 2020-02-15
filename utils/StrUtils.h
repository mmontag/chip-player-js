#ifndef __STRUTILS_H__
#define __STRUTILS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"

typedef struct _codepage_conversion CPCONV;

UINT8 CPConv_Init(CPCONV** retCPC, const char* cpFrom, const char* cpTo);
void CPConv_Deinit(CPCONV* cpc);
// parameters:
//	cpc: codepage conversion object
//	outSize: [input] size of output buffer, [output] size of converted string
//	outStr: [input/output] pointer to output buffer, if *outStr is NULL, it will be allocated automatically
//	inSize: [input] length of input string, may be 0 (in that case, strlen() is used to determine the string's length)
//	inStr: [input] input string
UINT8 CPConv_StrConvert(CPCONV* cpc, size_t* outSize, char** outStr, size_t inSize, const char* inStr);

#ifdef __cplusplus
}
#endif

#endif	// __STRUTILS_H__
