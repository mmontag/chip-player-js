#include "DataLoader.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

DATA_LOADER *DataLoader_New(const DATA_LOADER_CALLBACKS *callbacks) {
	DATA_LOADER *loader = (DATA_LOADER *)malloc(sizeof(DATA_LOADER));
	if(loader != NULL) {
		DataLoader_Init(loader,callbacks);
	}
	return loader;
}

UINT8 DataLoader_Free(DATA_LOADER *loader)
{
	if (loader->_status == DLSTAT_EMPTY) return 1;

	DataLoader_CancelLoading(loader);

	if(loader->_data) {
		free(loader->_data);
		loader->_data = NULL;
		loader->_bytesLoaded = 0;
	}

	loader->_status = DLSTAT_EMPTY;

	return 0;

}

UINT8 DataLoader_Init(DATA_LOADER *loader,const DATA_LOADER_CALLBACKS *callbacks) {
	loader->_data = NULL;
	loader->_context = NULL;
	loader->_status = DLSTAT_EMPTY;
	loader->_readStopOfs = (UINT32)-1;
	loader->_callbacks = callbacks;

	return 0;
}

void DataLoader_Delete(DATA_LOADER *loader) {
	DataLoader_Free(loader);
	free(loader);
}

UINT8 *DataLoader_GetData(DATA_LOADER *loader) {
	return loader->_data;
}

UINT32 DataLoader_GetTotalSize(DATA_LOADER *loader) {
	return loader->_bytesTotal;
}

UINT32 DataLoader_GetSize(DATA_LOADER *loader) {
	return loader->_bytesLoaded;
}

UINT8 DataLoader_GetStatus(DATA_LOADER *loader) {
	return loader->_status;
}

UINT8 DataLoader_CancelLoading(DATA_LOADER *loader)
{
	if(loader->_status != DLSTAT_LOADING) return 0x01;
	loader->_context = loader->_callbacks->dclose(loader->_context);
	loader->_status = DLSTAT_LOADED;
	
	return 0x00;

}

UINT8 DataLoader_Load(DATA_LOADER *loader, ...)
{
	if (loader->_status == DLSTAT_LOADING)
		return 0x01;
	
	DataLoader_Free(loader);

	va_list argp;
	va_start(argp,loader);
	loader->_context = loader->_callbacks->dopen(loader->_context, argp);
	va_end(argp);
	if(loader->_context == NULL) {
		return 0x01;
	}
	
	if(loader->_data) free(loader->_data);
	loader->_bytesLoaded = 0x00;
	loader->_status = DLSTAT_LOADING;
	loader->_bytesTotal = loader->_callbacks->dlength(loader->_context);
	
	if (loader->_readStopOfs > 0)
		DataLoader_Read(loader,loader->_readStopOfs);
	
	return 0x00;
}

void DataLoader_SetPreloadBytes(DATA_LOADER *loader, UINT32 byteCount)
{
	loader->_readStopOfs = byteCount;
	return;
}

void DataLoader_ReadUntil(DATA_LOADER *loader, UINT32 fileOffset)
{
	if (fileOffset > loader->_bytesLoaded)
		DataLoader_Read(loader,fileOffset);
	return;
}

void DataLoader_ReadAll(DATA_LOADER *loader)
{
	while(DataLoader_Read(loader,loader->_bytesTotal - loader->_bytesLoaded) >0)
	{
	}
	return;
}

UINT32 DataLoader_Read(DATA_LOADER *loader, UINT32 numBytes)
{
	if (loader->_status != DLSTAT_LOADING)
		return 0;
	
	UINT32 endOfs;
	UINT32 readBytes;
	
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
		return 0;
	}
	
	numBytes = endOfs - loader->_bytesLoaded;
	readBytes = loader->_callbacks->dread(loader->_context,&loader->_data[loader->_bytesLoaded],numBytes);
	if(!readBytes) return 0;
	loader->_bytesLoaded += readBytes;

	if (loader->_callbacks->deof(loader->_context))
	{
		DataLoader_CancelLoading(loader);
		loader->_status = DLSTAT_LOADED;
	}
	
	return readBytes;
}
