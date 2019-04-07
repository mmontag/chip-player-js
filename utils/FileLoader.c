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

struct FileLoader {
	UINT8 _modeCompr;
	UINT32 _bytesTotal;
	LOADER_HANDLES _hLoad[1];
};

typedef struct FileLoader FILE_LOADER;

static void *FileLoader_dopen(void *context, va_list argp)
{
	UINT8 fileHdr[4];
	size_t readBytes;
	const char *fileName = (const char *)va_arg(argp,void *);

	FILE_LOADER *loader = (FILE_LOADER *)malloc(sizeof(FILE_LOADER));

	if(loader == NULL) return NULL;
	memset(loader,0,sizeof(FILE_LOADER));
	
	loader->_hLoad->hFileRaw = fopen(fileName, "rb");
	if (loader->_hLoad->hFileRaw == NULL)
	{
		free(loader);
		return NULL;
	}
	
	readBytes = fread(fileHdr, 0x01, 4, loader->_hLoad->hFileRaw);
	if (readBytes < 4)
	{
		fclose(loader->_hLoad->hFileRaw);
		free(loader);
		return NULL;
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
		
		loader->_hLoad->hFileGZ = gzopen(fileName, "rb");
		if (loader->_hLoad->hFileGZ == NULL)
		{
			free(loader);
			return NULL;
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
	
	return (void *)loader;
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

static void *FileLoader_dclose(void *context) {
	if(context == NULL) return 0;
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->_modeCompr == FLMODE_CMP_RAW) fclose(loader->_hLoad->hFileRaw);
	else gzclose(loader->_hLoad->hFileGZ);
	free(loader);
	return NULL;
}

static INT32 FileLoader_dtell(void *context) {
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->_modeCompr == FLMODE_CMP_RAW)
	{
		return ftell(loader->_hLoad->hFileRaw);
	}
	return gztell(loader->_hLoad->hFileGZ);
}

static UINT32 FileLoader_dlength(void *context) {
	FILE_LOADER *loader = (FILE_LOADER *)context;
	return loader->_bytesTotal;
}

static UINT8 FileLoader_deof(void *context) {
	FILE_LOADER *loader = (FILE_LOADER *)context;
	if(loader->_modeCompr == FLMODE_CMP_RAW) return feof(loader->_hLoad->hFileRaw);
	return gzeof(loader->_hLoad->hFileGZ);
}

const DATA_LOADER_CALLBACKS fileLoader = {
	.type    = "Default File Loader",
	.dopen   = FileLoader_dopen,
	.dread   = FileLoader_dread,
	.dseek   = FileLoader_dseek,
	.dclose  = FileLoader_dclose,
	.dtell   = FileLoader_dtell,
	.dlength = FileLoader_dlength,
	.deof    = FileLoader_deof,
};
