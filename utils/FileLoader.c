#include "FileLoader.h"
#include <stdlib.h>
#include <string.h>

FILE_LOADER *FileLoader_New(void) {
	FILE_LOADER *loader = (FILE_LOADER *)malloc(sizeof(FILE_LOADER));
	if(loader != NULL) {
		FileLoader_Init(loader);
	}
	return loader;
}

UINT8 FileLoader_FreeData(FILE_LOADER *loader)
{
	if (loader->_status == FLSTAT_EMPTY) return 1;

	FileLoader_CancelLoading(loader);

	if(loader->_data) {
		free(loader->_data);
		loader->_data = NULL;
		loader->_bytesLoaded = 0;
	}

	loader->_status = FLSTAT_EMPTY;

	return 0;

}

UINT8 FileLoader_Init(FILE_LOADER *loader) {
	loader->_data = NULL;
	loader->_hLoad->dataPtr = NULL;
	loader->_modeSrc = 0x00;
	loader->_modeCompr = 0x00;
	loader->_status = FLSTAT_EMPTY;
	loader->_readStopOfs = (UINT32)-1;

	return 0;
}

void FileLoader_Delete(FILE_LOADER *loader) {
	FileLoader_FreeData(loader);
	free(loader);
}

UINT8 *FileLoader_GetFileData(FILE_LOADER *loader) {
	return loader->_data;
}

UINT32 FileLoader_GetTotalFileSize(FILE_LOADER *loader) {
	return loader->_bytesTotal;
}

UINT32 FileLoader_GetFileSize(FILE_LOADER *loader) {
	return loader->_bytesLoaded;
}

UINT8 FileLoader_GetMode(FILE_LOADER *loader) {
	return loader->_modeSrc | loader->_modeCompr;
}

UINT8 FileLoader_GetStatus(FILE_LOADER *loader) {
	return loader->_status;
}

UINT8 FileLoader_CancelLoading(FILE_LOADER *loader)
{
	if(loader->_status != FLSTAT_LOADING) return 0x01;
	switch(loader->_modeSrc)
	{
	case FLMODE_SRC_MEMORY:
		if (loader->_hLoad->dataPtr == NULL)
			return 0x01;
		loader->_hLoad->dataPtr = NULL;
		break;
	case FLMODE_SRC_FILE:
		switch(loader->_modeCompr)
		{
		case FLMODE_CMP_RAW:
			if (loader->_hLoad->hFileRaw == NULL)
				return 0x01;
			fclose(loader->_hLoad->hFileRaw);
			loader->_hLoad->hFileRaw = NULL;
			break;
		case FLMODE_CMP_GZ:
			if (loader->_hLoad->hFileGZ == NULL)
				return 0x01;
			gzclose(loader->_hLoad->hFileGZ);
			loader->_hLoad->hFileGZ = NULL;
			break;
		}
		break;
	}
	loader->_status = FLSTAT_LOADED;
	
	return 0x00;

}

UINT8 FileLoader_LoadData(FILE_LOADER *loader, UINT32 dataSize, const UINT8 *data)
{
	FileLoader_CancelLoading(loader);
	
	loader->_modeSrc = FLMODE_SRC_MEMORY;
	loader->_modeCompr = FLMODE_CMP_RAW;

	loader->_hLoad->dataPtr = data;
	loader->_bytesTotal = dataSize;
	loader->_bytesLoaded = 0x00;
	loader->_status = FLSTAT_LOADING;
	
	if (loader->_readStopOfs > 0)
		return FileLoader_ReadData(loader, loader->_readStopOfs);
	
	return 0x00;
}

UINT8 FileLoader_LoadFile(FILE_LOADER *loader, const char* fileName)
{
	if (loader->_status == FLSTAT_LOADING)
		return 0x01;
	
	UINT8 fileHdr[4];
	size_t readBytes;
	
	FileLoader_FreeData(loader);
	
	loader->_hLoad->hFileRaw = fopen(fileName, "rb");
	if (loader->_hLoad->hFileRaw == NULL)
		return 0xFF;
	
	readBytes = fread(fileHdr, 0x01, 4, loader->_hLoad->hFileRaw);
	if (readBytes < 4)
	{
		fclose(loader->_hLoad->hFileRaw);
		return 0xFF;
	}
	loader->_modeSrc = FLMODE_SRC_FILE;
	
	if (fileHdr[0] == 31 && fileHdr[1] == 139)
	{
		fseek(loader->_hLoad->hFileRaw, -4, SEEK_END);
		fread(&loader->_bytesTotal, 0x04, 0x01, loader->_hLoad->hFileRaw);
		if (loader->_bytesTotal < ftell(loader->_hLoad->hFileRaw) / 2)
			loader->_bytesTotal = (UINT32)-1;
		fclose(loader->_hLoad->hFileRaw);
		loader->_hLoad->hFileRaw = NULL;
		
		loader->_hLoad->hFileGZ = gzopen(fileName, "rb");
		if (loader->_hLoad->hFileGZ == NULL)
			return 0xFF;
		loader->_modeCompr = FLMODE_CMP_GZ;
	}
	else
	{
		fseek(loader->_hLoad->hFileRaw, 0, SEEK_END);
		loader->_bytesTotal = ftell(loader->_hLoad->hFileRaw);
		rewind(loader->_hLoad->hFileRaw);
		loader->_modeCompr = FLMODE_CMP_RAW;
	}
	if(loader->_data) free(loader->_data);
	loader->_bytesLoaded = 0x00;
	loader->_status = FLSTAT_LOADING;
	
	if (loader->_readStopOfs > 0)
		FileLoader_ReadData(loader,loader->_readStopOfs);
	
	return 0x00;
}

void FileLoader_SetPreloadBytes(FILE_LOADER *loader, UINT32 byteCount)
{
	loader->_readStopOfs = byteCount;
	return;
}

void FileLoader_ReadUntil(FILE_LOADER *loader, UINT32 fileOffset)
{
	if (fileOffset > loader->_bytesLoaded)
		FileLoader_ReadData(loader,fileOffset);
	return;
}

void FileLoader_ReadFullFile(FILE_LOADER *loader)
{
	FileLoader_ReadData(loader,(UINT32)-1);
	return;
}

UINT8 FileLoader_ReadData(FILE_LOADER *loader, UINT32 numBytes)
{
	if (loader->_status != FLSTAT_LOADING)
		return 1;
	
	UINT32 endOfs;
	
	endOfs = loader->_bytesLoaded + numBytes;
	if (endOfs < loader->_bytesLoaded)
		endOfs = (UINT32)-1;
	if (endOfs > loader->_bytesTotal)
		endOfs = loader->_bytesTotal;

	if(loader->_data == NULL) {
		loader->_data = (UINT8 *)malloc(endOfs);
	} else {
		loader->_data = (UINT8 *)realloc(loader->_data,endOfs);
	}

	if(loader->_data == NULL) {
		return 1;
	}
	
	numBytes = endOfs - loader->_bytesLoaded;

	if (loader->_modeSrc == FLMODE_SRC_MEMORY)
	{
		const UINT8* dataPtr = (const UINT8*)loader->_hLoad->dataPtr;
		memcpy(&loader->_data[loader->_bytesLoaded], &dataPtr[loader->_bytesLoaded], numBytes);
		loader->_bytesLoaded = endOfs;
	}
	else if (loader->_modeSrc == FLMODE_SRC_FILE)
	{
		if (loader->_modeCompr == FLMODE_CMP_RAW)
		{
			UINT32 readBytes;
			
			readBytes = fread(&loader->_data[loader->_bytesLoaded], 0x01, numBytes, loader->_hLoad->hFileRaw);
			loader->_bytesLoaded += readBytes;
		}
		else if (loader->_modeCompr == FLMODE_CMP_GZ)
		{
			UINT32 readBytes;
			
			if (endOfs != (UINT32)-1)
			{
				readBytes = gzread(loader->_hLoad->hFileGZ, &loader->_data[loader->_bytesLoaded], numBytes);
				loader->_bytesLoaded += readBytes;
			}
			else
			{
				UINT32 blkSize = 0x4000;
				if (blkSize > numBytes)
					blkSize = numBytes;
				do
				{
					loader->_data = (UINT8 *)realloc(loader->_data, loader->_bytesLoaded + blkSize);
					if(loader->_data == NULL) return 1;
					readBytes = gzread(loader->_hLoad->hFileGZ, &loader->_data[loader->_bytesLoaded], blkSize);
					loader->_bytesLoaded += readBytes;
				} while(readBytes >= blkSize && loader->_bytesLoaded < endOfs);
				if (readBytes < blkSize)	// reached EOF
					loader->_bytesTotal = loader->_bytesLoaded;
			}
		}
	}
	if (loader->_bytesLoaded >= loader->_bytesTotal)
	{
		FileLoader_CancelLoading(loader);
		loader->_status = FLSTAT_LOADED;
	}
	
	return 0;
}
