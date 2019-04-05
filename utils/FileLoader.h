#ifndef __FILELOADER_H__
#define __FILELOADER_H__

#include <stdtype.h>
#include <stdio.h>
#include <zlib.h>

#ifdef __cplusplus
extern "C" {
#endif

union LoaderHandles
{
	const void *dataPtr;
	FILE *hFileRaw;
	gzFile hFileGZ;
};

typedef union LoaderHandles LoaderHandles;

enum
{
	FLSTAT_EMPTY = 0,
	FLSTAT_LOADING = 1,
	FLSTAT_LOADED = 2
};

enum
{
	// mode: sources
	FLMODE_SRC_MEMORY = 0x01,
	FLMODE_SRC_FILE = 0x02,

	// mode: compression
	FLMODE_CMP_RAW = 0x00,
	FLMODE_CMP_GZ = 0x10
};

struct FileLoader {
	UINT8 _modeSrc;
	UINT8 _modeCompr;
	UINT8 _status;
	UINT32 _bytesTotal;
	UINT32 _bytesLoaded;
	UINT32 _readStopOfs;
	LoaderHandles _hLoad[1];
	UINT8 *_data;
};

typedef struct FileLoader FILE_LOADER;

FILE_LOADER *FileLoader_New(void);
void FileLoader_Delete(FILE_LOADER *loader);
UINT8 FileLoader_Init(FILE_LOADER *loader);
UINT8 *FileLoader_GetFileData(FILE_LOADER *loader);
UINT32 FileLoader_GetTotalFileSize(FILE_LOADER *loader);
UINT32 FileLoader_GetFileSize(FILE_LOADER *loader);
UINT8 FileLoader_GetMode(FILE_LOADER *loader);
UINT8 FileLoader_GetStatus(FILE_LOADER *loader);
UINT8 FileLoader_ReadData(FILE_LOADER *loader, UINT32 numBytes);
UINT8 FileLoader_CancelLoading(FILE_LOADER *loader);
UINT8 FileLoader_LoadData(FILE_LOADER *loader, UINT32 dataSize, const UINT8 *data);
UINT8 FileLoader_LoadFile(FILE_LOADER *loader, const char *fileName);
UINT8 FileLoader_FreeData(FILE_LOADER *loader);
void FileLoader_SetPreloadBytes(FILE_LOADER *loader, UINT32 byteCount);
void FileLoader_ReadUntil(FILE_LOADER *loader, UINT32 fileOffset);
void FileLoader_ReadFullFile(FILE_LOADER *loader);

#ifdef __cplusplus
}
#endif

#endif /* __FILELOADER_H__ */
