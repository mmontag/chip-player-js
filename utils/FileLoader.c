#include "FileLoader.h"

#include <zlib.h>
#include <stdio.h>
#include <stdtype.h>
#include <stdlib.h>
#include <string.h>

enum
{
	// mode: compression
	FLMODE_CMP_RAW = 0x00,
	FLMODE_CMP_GZ = 0x10
};


typedef union loader_handles
{
	FILE *hFileRaw;
	gzFile hFileGZ;
} LOADER_HANDLES;

typedef struct file_loader
{
	UINT8 modeCompr;
	UINT32 bytesTotal;
	LOADER_HANDLES hLoad;
	const char *fileName;
} FILE_LOADER;

static inline UINT32 ReadLE32(const UINT8 *data)
{
	return	(data[0x03] << 24) | (data[0x02] << 16) |
			(data[0x01] <<  8) | (data[0x00] <<  0);
}

static UINT8 FileLoader_dopen(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	UINT8 fileHdr[4];
	size_t readBytes;

	loader->hLoad.hFileRaw = fopen(loader->fileName, "rb");
	if (loader->hLoad.hFileRaw == NULL) return 0x01;

	readBytes = fread(fileHdr, 0x01, 4, loader->hLoad.hFileRaw);
	if (readBytes < 4)
	{
		fclose(loader->hLoad.hFileRaw);
		loader->hLoad.hFileRaw = NULL;
		return 0x01;
	}

	if (fileHdr[0] == 31 && fileHdr[1] == 139)
	{
		UINT8 sizeBuffer[4];
		fseek(loader->hLoad.hFileRaw, -4, SEEK_END);
		fread(sizeBuffer, 0x04, 0x01, loader->hLoad.hFileRaw);
		loader->bytesTotal = ReadLE32(sizeBuffer);
		if (loader->bytesTotal < ftell(loader->hLoad.hFileRaw) / 2)
			loader->bytesTotal = 0;
		fclose(loader->hLoad.hFileRaw);
		loader->hLoad.hFileRaw = NULL;

		loader->hLoad.hFileGZ = gzopen(loader->fileName, "rb");
		if (loader->hLoad.hFileGZ == NULL)
			return 0x01;
		loader->modeCompr = FLMODE_CMP_GZ;
	}
	else
	{
		fseek(loader->hLoad.hFileRaw, 0, SEEK_END);
		loader->bytesTotal = ftell(loader->hLoad.hFileRaw);
		rewind(loader->hLoad.hFileRaw);
		loader->modeCompr = FLMODE_CMP_RAW;
	}

	return 0x00;
}

static UINT32 FileLoader_dread(void *context, UINT8 *buffer, UINT32 numBytes)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if (loader->modeCompr == FLMODE_CMP_RAW)
		return fread(buffer, 0x01, numBytes, loader->hLoad.hFileRaw);

	return gzread(loader->hLoad.hFileGZ,buffer,numBytes);
}

static UINT8 FileLoader_dseek(void *context, UINT32 offset, UINT8 whence)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->modeCompr == FLMODE_CMP_RAW) return fseek(loader->hLoad.hFileRaw, offset, whence);
	if(whence == SEEK_END) return 0;
	return gzseek(loader->hLoad.hFileGZ, offset, whence);

}

static UINT8 FileLoader_dclose(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->modeCompr == FLMODE_CMP_RAW) fclose(loader->hLoad.hFileRaw);
	else gzclose(loader->hLoad.hFileGZ);

	return 0x00;
}

static INT32 FileLoader_dtell(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->modeCompr == FLMODE_CMP_RAW)
		return ftell(loader->hLoad.hFileRaw);
	return gztell(loader->hLoad.hFileGZ);
}

static UINT32 FileLoader_dlength(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->bytesTotal;
}

static UINT8 FileLoader_deof(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->modeCompr == FLMODE_CMP_RAW) return feof(loader->hLoad.hFileRaw);
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
		FileLoader_Deinit(dLoader);
		return NULL;
	}

	fLoader->fileName = fileName;

	DataLoader_Setup(dLoader,&fileLoader,fLoader);

	return dLoader;
}

const DATA_LOADER_CALLBACKS fileLoader = {
	.type	= "Default File Loader",
	.dopen   = FileLoader_dopen,
	.dread   = FileLoader_dread,
	.dseek   = FileLoader_dseek,
	.dclose  = FileLoader_dclose,
	.dtell   = FileLoader_dtell,
	.dlength = FileLoader_dlength,
	.deof	= FileLoader_deof,
};
