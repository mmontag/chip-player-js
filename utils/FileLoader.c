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


union LoaderHandles
{
	FILE *hFileRaw;
	gzFile hFileGZ;
};

typedef union LoaderHandles LOADER_HANDLES;

struct FileLoader
{
	UINT8 _modeCompr;
	UINT32 _bytesTotal;
	LOADER_HANDLES _hLoad[1];
	const char *fileName;
};

typedef struct FileLoader FILE_LOADER;

static UINT8 FileLoader_dopen(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	UINT8 fileHdr[4];
	size_t readBytes;

	loader->_hLoad->hFileRaw = fopen(loader->fileName, "rb");
	if (loader->_hLoad->hFileRaw == NULL) return 0x01;

	readBytes = fread(fileHdr, 0x01, 4, loader->_hLoad->hFileRaw);
	if (readBytes < 4)
	{
		fclose(loader->_hLoad->hFileRaw);
		loader->_hLoad->hFileRaw = NULL;
		return 0x01;
	}

	if (fileHdr[0] == 31 && fileHdr[1] == 139)
	{
		UINT32 totalSize;
		fseek(loader->_hLoad->hFileRaw, -4, SEEK_END);
		fread(&totalSize, 0x04, 0x01, loader->_hLoad->hFileRaw);
		loader->_bytesTotal = (UINT32)totalSize;
		if (loader->_bytesTotal < ftell(loader->_hLoad->hFileRaw) / 2)
		{
			loader->_bytesTotal = 0;
		}
		fclose(loader->_hLoad->hFileRaw);
		loader->_hLoad->hFileRaw = NULL;

		loader->_hLoad->hFileGZ = gzopen(loader->fileName, "rb");
		if (loader->_hLoad->hFileGZ == NULL)
		{
			return 0x01;
		}
		loader->_modeCompr = FLMODE_CMP_GZ;
	}
	else
	{
		fseek(loader->_hLoad->hFileRaw, 0, SEEK_END);
		loader->_bytesTotal = ftell(loader->_hLoad->hFileRaw);
		rewind(loader->_hLoad->hFileRaw);
		loader->_modeCompr = FLMODE_CMP_RAW;
	}

	return 0x00;
}

static UINT32 FileLoader_dread(void *context, UINT8 *buffer, UINT32 numBytes)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if (loader->_modeCompr == FLMODE_CMP_RAW)
	{
		return  fread(buffer, 0x01, numBytes, loader->_hLoad->hFileRaw);
	}

	return gzread(loader->_hLoad->hFileGZ,buffer,numBytes);
}

static UINT8 FileLoader_dseek(void *context, UINT32 offset, UINT8 whence)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->_modeCompr == FLMODE_CMP_RAW) return fseek(loader->_hLoad->hFileRaw, offset, whence);
	if(whence == SEEK_END) return 0;
	return gzseek(loader->_hLoad->hFileGZ, offset, whence);

}

static UINT8 FileLoader_dclose(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->_modeCompr == FLMODE_CMP_RAW) fclose(loader->_hLoad->hFileRaw);
	else gzclose(loader->_hLoad->hFileGZ);

	return 0x00;
}

static INT32 FileLoader_dtell(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->_modeCompr == FLMODE_CMP_RAW)
	{
		return ftell(loader->_hLoad->hFileRaw);
	}
	return gztell(loader->_hLoad->hFileGZ);
}

static UINT32 FileLoader_dlength(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->_bytesTotal;
}

static UINT8 FileLoader_deof(void *context)
{
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->_modeCompr == FLMODE_CMP_RAW) return feof(loader->_hLoad->hFileRaw);
	return gzeof(loader->_hLoad->hFileGZ);
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

DATA_LOADER *FileLoader_Init(const char *fileName)
{
	DATA_LOADER *dLoader = (DATA_LOADER *)malloc(sizeof(DATA_LOADER));
	if(dLoader == NULL) return NULL;
	memset(dLoader,0,sizeof(DATA_LOADER));

	FILE_LOADER *fLoader = (FILE_LOADER *)malloc(sizeof(FILE_LOADER));
	if(fLoader == NULL)
	{
		FileLoader_Deinit(dLoader);
		return NULL;
	}
	memset(fLoader,0,sizeof(FILE_LOADER));

	fLoader->fileName = fileName;

	DataLoader_Setup(dLoader,&fileLoader,fLoader);

	return dLoader;
}

