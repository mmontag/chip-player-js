// String Utilities: Character Codepage Conversion
// ----------------
// using Windows API

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <wchar.h>
#include <windows.h>

#ifdef _MSC_VER
#define strdup		_strdup
#define stricmp		_stricmp
#define strnicmp	_strnicmp
#define snprintf	_snprintf
#endif

#ifndef LSTATUS	// for MS VC6
#define	LSTATUS	LONG
#endif

#include "../stdtype.h"
#include "StrUtils.h"

//typedef struct _codepage_conversion CPCONV;
struct _codepage_conversion
{
	// codpage name strings
	char* cpsFrom;
	char* cpsTo;
	// codepage IDs
	UINT cpiFrom;
	UINT cpiTo;
};

static const char* REGKEY_CP_LIST = "SOFTWARE\\Classes\\MIME\\Database\\Charset";

#define CP_UTF16_NE	1200	// UTF-16 LE ("native endian")
#define CP_UTF16_OE	1201	// UTF-16 BE ("opposite endian")

static UINT GetCodepageFromStr(const char* codepageName)
{
	char fullKeyPath[MAX_PATH];
	char cpAlias[0x80];
	HKEY hKey;
	DWORD keyType;
	DWORD keyValSize;
	DWORD codepageID;
	int retI;
	LSTATUS retS;

	// catch a few encodings that Windows calls differently from iconv
	if (! stricmp(codepageName, "UTF-16LE"))
		codepageName = "unicode";
	else if (! stricmp(codepageName, "UTF-16BE"))
		codepageName = "unicodeFFFE";
	// go the quick route for "CPxxx"
	if (! strnicmp(codepageName, "CP", 2) && isdigit((unsigned char)codepageName[2]))
		return (UINT)atoi(&codepageName[2]);
	
	retI = snprintf(fullKeyPath, MAX_PATH, "%s\\%s", REGKEY_CP_LIST, codepageName);
	if (retI <= 0 || retI >= MAX_PATH)
		return 0;
	codepageID = 0;
	while(1)
	{
		retS = RegOpenKeyExA(HKEY_LOCAL_MACHINE, fullKeyPath, 0x00, KEY_QUERY_VALUE, &hKey);
		if (retS != ERROR_SUCCESS)
			return 0;
		
		// At first, try the keys that have the codepage ID.
		// "InternetEncoding" seems to be a bit more accurate than "Codepage". (e.g. for UTF-8)
		keyValSize = sizeof(DWORD);
		retS = RegQueryValueExA(hKey, "InternetEncoding", NULL, &keyType, (LPBYTE)&codepageID, &keyValSize);
		if (retS == ERROR_SUCCESS && keyType == REG_DWORD)
			break;
		keyValSize = sizeof(DWORD);
		retS = RegQueryValueExA(hKey, "Codepage", NULL, &keyType, (LPBYTE)&codepageID, &keyValSize);
		if (retS == ERROR_SUCCESS && keyType == REG_DWORD)
			break;
		
		keyValSize = 0x80;
		retS = RegQueryValueExA(hKey, "AliasForCharset", NULL, &keyType, (LPBYTE)cpAlias, &keyValSize);
		if (retS != ERROR_SUCCESS || keyType != REG_SZ)
			break;
		if (keyValSize >= 0x80)
			keyValSize = 0x7F;
		cpAlias[keyValSize] = '\0';	// ensure '\0' termination
		RegCloseKey(hKey);
		
		// generate new RegKey path and try all the stuff again
		retI = snprintf(fullKeyPath, MAX_PATH, "%s\\%s", REGKEY_CP_LIST, cpAlias);
		if (retI <= 0 || retI >= MAX_PATH)
			return 0;
	}
	
	RegCloseKey(hKey);
	
	return (UINT)codepageID;
}

UINT8 CPConv_Init(CPCONV** retCPC, const char* cpFrom, const char* cpTo)
{
	CPCONV* cpc;
	
	cpc = (CPCONV*)calloc(1, sizeof(CPCONV));
	if (cpc == NULL)
		return 0xFF;
	
	cpc->cpiFrom = GetCodepageFromStr(cpFrom);
	if (! cpc->cpiFrom)
	{
		free(cpc);
		return 0x80;
	}
	cpc->cpiTo = GetCodepageFromStr(cpTo);
	if (! cpc->cpiTo)
	{
		free(cpc);
		return 0x81;
	}
	cpc->cpsFrom = strdup(cpFrom);
	cpc->cpsTo = strdup(cpTo);
	
	*retCPC = cpc;
	return 0x00;
}

void CPConv_Deinit(CPCONV* cpc)
{
	free(cpc->cpsFrom);
	free(cpc->cpsTo);
	
	free(cpc);
	
	return;
}

UINT8 CPConv_StrConvert(CPCONV* cpc, size_t* outSize, char** outStr, size_t inSize, const char* inStr)
{
	int wcBufSize;
	wchar_t* wcBuf;
	const wchar_t* wcStr;
	UINT8 canAlloc;
	
	if (inSize == 0)
	{
		// add +1 for conuting the terminating \0 character
		if (cpc->cpiFrom == CP_UTF16_NE || cpc->cpiFrom == CP_UTF16_OE)
			inSize = (wcslen((const wchar_t*)inStr) + 1) * sizeof(wchar_t);
		else
			inSize = (strlen(inStr) + 1) * sizeof(char);
	}
	if (inSize == 0)
	{
		*outSize = 0;
		return 0x02;	// nothing to convert
	}
	canAlloc = (*outStr == NULL);
	
	if (cpc->cpiFrom == CP_UTF16_NE)	// UTF-16, native endian
	{
		wcBufSize = inSize / sizeof(wchar_t);
		wcBuf = NULL;
		wcStr = (const wchar_t*)inStr;
	}
	else if (cpc->cpiFrom == CP_UTF16_OE)	// UTF-16, opposite endian
	{
		size_t curChrPos;
		char* wcBufPtr;
		
		wcBufSize = inSize / sizeof(wchar_t);
		wcBuf = (wchar_t*)malloc(wcBufSize * sizeof(wchar_t));
		wcBufPtr = (char*)wcBuf;
		for (curChrPos = 0; curChrPos < inSize; curChrPos += 0x02)
		{
			wcBufPtr[curChrPos + 0x00] = inStr[curChrPos + 0x01];
			wcBufPtr[curChrPos + 0x01] = inStr[curChrPos + 0x00];
		}
		wcStr = wcBuf;
	}
	else
	{
		wcBufSize = MultiByteToWideChar(cpc->cpiFrom, 0x00, inStr, inSize, NULL, 0);
		if (wcBufSize < 0 || (wcBufSize == 0 && inSize > 0))
			return 0x80;	// conversion error
		wcBuf = (wchar_t*)malloc(wcBufSize * sizeof(wchar_t));
		wcBufSize = MultiByteToWideChar(cpc->cpiFrom, 0x00, inStr, inSize, wcBuf, wcBufSize);
		if (wcBufSize < 0)
			wcBufSize = 0;
		wcStr = wcBuf;
	}
	
	// at this point we have a "wide-character" (UTF-16) string in wcStr
	
	if (cpc->cpiTo == CP_UTF16_NE || cpc->cpiTo == CP_UTF16_OE)
	{
		size_t reqSize = (size_t)wcBufSize * sizeof(wchar_t);
		if (! canAlloc)
		{
			if (*outSize > reqSize)
				*outSize = reqSize;
			memcpy(*outStr, wcStr, *outSize);
			free(wcBuf);
		}
		else if (wcStr == wcBuf)
		{
			*outSize = reqSize;
			*outStr = (char*)wcBuf;
		}
		else
		{
			*outSize = reqSize;
			*outStr = (char*)malloc(*outSize * sizeof(wchar_t));
			memcpy(*outStr, wcStr, *outSize);
			free(wcBuf);
		}
		
		if (cpc->cpiTo == CP_UTF16_OE)
		{
			size_t curChrPos;
			char* bufPtr = *outStr;
			for (curChrPos = 0; curChrPos < *outSize; curChrPos += 0x02)
			{
				char temp = bufPtr[curChrPos + 0x00];
				bufPtr[curChrPos + 0x00] = bufPtr[curChrPos + 0x01];
				bufPtr[curChrPos + 0x01] = temp;
			}
		}
		
		if (*outSize < reqSize)
			return 0x10;	// conversion incomplete due to lack of buffer space
		else
			return 0x00;	// conversion successfull
	}
	else
	{
		int reqSize;
		int convChrs;
		
		reqSize = WideCharToMultiByte(cpc->cpiTo, 0x00, wcStr, wcBufSize, NULL, 0, NULL, NULL);
		if (reqSize < 0)
		{
			free(wcBuf);
			*outSize = 0;
			return 0x80;	// conversion error
		}
		if (canAlloc)
		{
			*outSize = (size_t)reqSize;
			*outStr = (char*)malloc((*outSize) * sizeof(char));
		}
		convChrs = WideCharToMultiByte(cpc->cpiTo, 0x00, wcStr, wcBufSize, *outStr, *outSize, NULL, NULL);
		*outSize = (convChrs >= 0) ? (size_t)convChrs : 0;
		
		free(wcBuf);
		if (convChrs <= 0)
			return 0x80;	// conversion error
		else if (convChrs < reqSize)
			return 0x10;	// conversion incomplete due to lack of buffer space
		else
			return 0x00;	// conversion successfull
	}
}
