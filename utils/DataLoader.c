#include <stdlib.h>
#include <string.h>

#include "../stdtype.h"
#include "DataLoader.h"

UINT8 DataLoader_Reset(DATA_LOADER *loader)
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

UINT8 *DataLoader_GetData(DATA_LOADER *loader) {
	return loader->_data;
}

UINT32 DataLoader_GetTotalSize(const DATA_LOADER *loader) {
	return loader->_bytesTotal;
}

UINT32 DataLoader_GetSize(const DATA_LOADER *loader) {
	return loader->_bytesLoaded;
}

UINT8 DataLoader_GetStatus(const DATA_LOADER *loader) {
	return loader->_status;
}

UINT8 DataLoader_CancelLoading(DATA_LOADER *loader)
{
	if(loader->_status != DLSTAT_LOADING) return 0x01;

	if(loader->_callbacks->dclose(loader->_context)) return 0x01;
	loader->_status = DLSTAT_LOADED;

	return 0x00;
}

UINT8 DataLoader_Load(DATA_LOADER *loader)
{
	UINT8 ret;
	if (loader->_status == DLSTAT_LOADING)
		return 0x01;

	DataLoader_Reset(loader);

	ret = loader->_callbacks->dopen(loader->_context);
	if(ret) return ret;

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
		;
	return;
}

UINT32 DataLoader_Read(DATA_LOADER *loader, UINT32 numBytes)
{
	UINT32 endOfs;
	UINT32 readBytes;

	if (loader->_status != DLSTAT_LOADING)
		return 0;

	endOfs = loader->_bytesLoaded + numBytes;
	if (endOfs < loader->_bytesLoaded)
		endOfs = (UINT32)-1;
	if (endOfs > loader->_bytesTotal)
		endOfs = loader->_bytesTotal;

	loader->_data = (UINT8 *)realloc(loader->_data,endOfs);
	if(loader->_data == NULL) {
		return 0;
	}

	numBytes = endOfs - loader->_bytesLoaded;
	readBytes = loader->_callbacks->dread(loader->_context,&loader->_data[loader->_bytesLoaded],numBytes);
	if(!readBytes) return 0;
	loader->_bytesLoaded += readBytes;

	/* Note: The check (loaded >= total) is necessary, as the EoF flag is set only when
	         reading *beyond* the end of the file. Stopping at the last byte doesn't set it. */
	if (loader->_bytesLoaded >= loader->_bytesTotal || loader->_callbacks->deof(loader->_context))
	{
		DataLoader_CancelLoading(loader);
		loader->_status = DLSTAT_LOADED;
	}

	return readBytes;
}

void DataLoader_Deinit(DATA_LOADER *dLoader)
{
	if(dLoader == NULL) return;
	DataLoader_Reset(dLoader);
	if(dLoader->_context != NULL)
	{
		if (dLoader->_callbacks->ddeinit)
			dLoader->_callbacks->ddeinit(dLoader->_context);
		else
			free(dLoader->_context);
	}
	free(dLoader);
}

void DataLoader_Setup(DATA_LOADER *loader, const DATA_LOADER_CALLBACKS *callbacks, void *context) {
	loader->_data = NULL;
	loader->_status = DLSTAT_EMPTY;
	loader->_readStopOfs = (UINT32)-1;
	loader->_context = context;
	loader->_callbacks = callbacks;
}
