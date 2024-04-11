#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "../common_def.h"
#include "FileLoader.h"

#if HAVE_FILELOADER_W
#include <wchar.h>
#endif

#ifdef _MSC_VER
#define strdup	_strdup
#define wcsdup	_wcsdup
#endif

enum
{
	// mode: compression
	FLMODE_CMP_RAW = 0x00,
	FLMODE_CMP_GZ = 0x10
};

typedef struct _file_loader FILE_LOADER;

typedef UINT8 (*FLOAD_GENERIC)(FILE_LOADER *loader);
typedef UINT32 (*FLOAD_READ)(FILE_LOADER *loader, UINT8 *buffer, UINT32 numBytes);
typedef UINT8 (*FLOAD_SEEK)(FILE_LOADER *loader, UINT32 offset, UINT8 whence);
typedef INT32 (*FLOAD_TELL)(FILE_LOADER *loader);
typedef UINT32 (*FLOAD_LENGTH)(FILE_LOADER *loader);

typedef union _loader_handles
{
	FILE *hFileRaw;
	gzFile hFileGZ;
} LOADER_HANDLES;

struct _file_loader
{
	UINT8 modeCompr;
	UINT32 bytesTotal;
	LOADER_HANDLES hLoad;
	char *fileName;
#if HAVE_FILELOADER_W
	wchar_t* fileNameW;	// Note: used when fileName == NULL
#endif

	FLOAD_READ Read;
	FLOAD_SEEK Seek;
	FLOAD_GENERIC Close;
	FLOAD_TELL Tell;
	FLOAD_GENERIC Eof;
};


static UINT8 FileLoader_dopen(void *context);
static UINT32 FileLoader_dread(void *context, UINT8 *buffer, UINT32 numBytes);
static UINT8 FileLoader_dseek(void *context, UINT32 offset, UINT8 whence);
static UINT8 FileLoader_dclose(void *context);
static INT32 FileLoader_dtell(void *context);
static UINT32 FileLoader_dlength(void *context);
static UINT8 FileLoader_deof(void *context);

static UINT32 FileLoader_ReadRaw(FILE_LOADER *loader, UINT8 *buffer, UINT32 numBytes);
static UINT8 FileLoader_SeekRaw(FILE_LOADER *loader, UINT32 offset, UINT8 whence);
static UINT8 FileLoader_CloseRaw(FILE_LOADER *loader);
static INT32 FileLoader_TellRaw(FILE_LOADER *loader);
static UINT8 FileLoader_EofRaw(FILE_LOADER *loader);

static UINT32 FileLoader_ReadGZ(FILE_LOADER *loader, UINT8 *buffer, UINT32 numBytes);
static UINT8 FileLoader_SeekGZ(FILE_LOADER *loader, UINT32 offset, UINT8 whence);
static UINT8 FileLoader_CloseGZ(FILE_LOADER *loader);
static INT32 FileLoader_TellGZ(FILE_LOADER *loader);
static UINT8 FileLoader_EofGZ(FILE_LOADER *loader);

//DATA_LOADER *FileLoader_Init(const char *fileName);
//DATA_LOADER *FileLoader_InitW(const wchar_t *fileName);
static void FileLoader_dfree(void *context);


INLINE UINT32 ReadLE32(const UINT8 *data)
{
	return	(data[0x03] << 24) | (data[0x02] << 16) |
			(data[0x01] <<  8) | (data[0x00] <<  0);
}

static UINT8 FileLoader_dopen(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	UINT8 fileHdr[4];
	size_t readBytes;

#if HAVE_FILELOADER_W
	if (loader->fileName == NULL)
		loader->hLoad.hFileRaw = _wfopen(loader->fileNameW, L"rb");
	else
#endif
		loader->hLoad.hFileRaw = fopen(loader->fileName, "rb");
	if (loader->hLoad.hFileRaw == NULL) return 0x01;

	readBytes = fread(fileHdr, 0x01, 4, loader->hLoad.hFileRaw);
	if (readBytes < 4)
		memset(&fileHdr[readBytes], 0x00, 4 - readBytes);

	if (fileHdr[0] == 31 && fileHdr[1] == 139)	// check for .gz file header
	{
		UINT8 sizeBuffer[4];
		fseek(loader->hLoad.hFileRaw, -4, SEEK_END);
		fread(sizeBuffer, 0x04, 0x01, loader->hLoad.hFileRaw);
		loader->bytesTotal = ReadLE32(sizeBuffer);
		if (loader->bytesTotal < (UINT32)ftell(loader->hLoad.hFileRaw) / 2)
			loader->bytesTotal = 0;
		fclose(loader->hLoad.hFileRaw);
		loader->hLoad.hFileRaw = NULL;

#if HAVE_FILELOADER_W
		if (loader->fileName == NULL)
			loader->hLoad.hFileGZ = gzopen_w(loader->fileNameW, "rb");
		else
#endif
			loader->hLoad.hFileGZ = gzopen(loader->fileName, "rb");
		if (loader->hLoad.hFileGZ == NULL)
			return 0x01;	// TODO: separate error code for "invalid .gz file"
		loader->modeCompr = FLMODE_CMP_GZ;
		loader->Read = &FileLoader_ReadGZ;
		loader->Seek = &FileLoader_SeekGZ;
		loader->Close = &FileLoader_CloseGZ;
		loader->Tell = &FileLoader_TellGZ;
		loader->Eof = &FileLoader_EofGZ;
		return 0x00;
	}

	fseek(loader->hLoad.hFileRaw, 0, SEEK_END);
	loader->bytesTotal = ftell(loader->hLoad.hFileRaw);
	rewind(loader->hLoad.hFileRaw);

	loader->modeCompr = FLMODE_CMP_RAW;
	loader->Read = &FileLoader_ReadRaw;
	loader->Seek = &FileLoader_SeekRaw;
	loader->Close = &FileLoader_CloseRaw;
	loader->Tell = &FileLoader_TellRaw;
	loader->Eof = &FileLoader_EofRaw;

	return 0x00;
}

static UINT32 FileLoader_dread(void *context, UINT8 *buffer, UINT32 numBytes)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->Read(loader, buffer, numBytes);
}

static UINT8 FileLoader_dseek(void *context, UINT32 offset, UINT8 whence)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->Seek(loader, offset, whence);
}

static UINT8 FileLoader_dclose(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->Close(loader);
}

static INT32 FileLoader_dtell(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->Tell(loader);
}

static UINT32 FileLoader_dlength(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->bytesTotal;
}

static UINT8 FileLoader_deof(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->Eof(loader);
}


static UINT32 FileLoader_ReadRaw(FILE_LOADER *loader, UINT8 *buffer, UINT32 numBytes)
{
	return fread(buffer, 0x01, numBytes, loader->hLoad.hFileRaw);
}

static UINT8 FileLoader_SeekRaw(FILE_LOADER *loader, UINT32 offset, UINT8 whence)
{
	return (UINT8)fseek(loader->hLoad.hFileRaw, offset, whence);
}

static UINT8 FileLoader_CloseRaw(FILE_LOADER *loader)
{
	fclose(loader->hLoad.hFileRaw);
	loader->hLoad.hFileRaw = NULL;
	return 0x00;
}

static INT32 FileLoader_TellRaw(FILE_LOADER *loader)
{
	return ftell(loader->hLoad.hFileRaw);
}

static UINT8 FileLoader_EofRaw(FILE_LOADER *loader)
{
	return feof(loader->hLoad.hFileRaw);
}


static UINT32 FileLoader_ReadGZ(FILE_LOADER *loader, UINT8 *buffer, UINT32 numBytes)
{
	return gzread(loader->hLoad.hFileGZ, buffer, numBytes);
}

static UINT8 FileLoader_SeekGZ(FILE_LOADER *loader, UINT32 offset, UINT8 whence)
{
	if(whence == SEEK_END) return 0;
	return (gzseek(loader->hLoad.hFileGZ, offset, whence) == -1) ? 0x01 : 0x00;
}

static UINT8 FileLoader_CloseGZ(FILE_LOADER *loader)
{
	gzclose(loader->hLoad.hFileGZ);
	loader->hLoad.hFileGZ = NULL;
	return 0x00;
}

static INT32 FileLoader_TellGZ(FILE_LOADER *loader)
{
	return gztell(loader->hLoad.hFileGZ);
}

static UINT8 FileLoader_EofGZ(FILE_LOADER *loader)
{
	return gzeof(loader->hLoad.hFileGZ);
}


DATA_LOADER *FileLoader_Init(const char *fileName)
{
	DATA_LOADER *dLoader;
	FILE_LOADER *fLoader;

	dLoader = (DATA_LOADER *)calloc(1, sizeof(DATA_LOADER));
	if(dLoader == NULL) return NULL;

	fLoader = (FILE_LOADER *)calloc(1, sizeof(FILE_LOADER));
	if(fLoader == NULL)
	{
		free(dLoader);
		return NULL;
	}

	fLoader->fileName = strdup(fileName);

	DataLoader_Setup(dLoader,&fileLoader,fLoader);

	return dLoader;
}

#if HAVE_FILELOADER_W
DATA_LOADER *FileLoader_InitW(const wchar_t *fileName)
{
	DATA_LOADER *dLoader;
	FILE_LOADER *fLoader;

	dLoader = (DATA_LOADER *)calloc(1, sizeof(DATA_LOADER));
	if(dLoader == NULL) return NULL;

	fLoader = (FILE_LOADER *)calloc(1, sizeof(FILE_LOADER));
	if(fLoader == NULL)
	{
		free(dLoader);
		return NULL;
	}

	fLoader->fileName = NULL;	// explicitly mark as "unused"
	fLoader->fileNameW = wcsdup(fileName);

	DataLoader_Setup(dLoader,&fileLoader,fLoader);

	return dLoader;
}
#endif

static void FileLoader_dfree(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
#if HAVE_FILELOADER_W
	if (loader->fileName == NULL)
		free(loader->fileNameW);
#endif
	free(loader->fileName);
	free(loader);
}

const DATA_LOADER_CALLBACKS fileLoader = {
	0x46494C45,		// "FILE"
	"File Loader",
	FileLoader_dopen,
	FileLoader_dread,
	FileLoader_dseek,
	FileLoader_dclose,
	FileLoader_dtell,
	FileLoader_dlength,
	FileLoader_deof,
	FileLoader_dfree,
};
