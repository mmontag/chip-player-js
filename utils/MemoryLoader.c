#include "MemoryLoader.h"

#include <zlib.h>
#include <stdtype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

enum
{
	// mode: compression
	MLMODE_CMP_RAW = 0x00,
	MLMODE_CMP_GZ = 0x10
};

struct MemoryLoader {
	UINT8 _modeCompr;
	UINT32 _pos;
	UINT32 _length;
	UINT8 *_data;
	z_stream _stream;
};

typedef struct MemoryLoader MEMORY_LOADER;

static inline UINT32 ReadLE32(const UINT8 *data)
{
	return	(data[0x03] << 24) | (data[0x02] << 16) |
			(data[0x01] <<  8) | (data[0x00] <<  0);
}

static UINT8 MemoryLoader_dopen(void *context)
{
	MEMORY_LOADER *loader = (MEMORY_LOADER *)context;
	loader->_pos = 0;
	if(loader->_data[0] == 31 && loader->_data[1] == 139)
	{
		loader->_modeCompr = MLMODE_CMP_GZ;
		loader->_stream.zalloc = Z_NULL;
		loader->_stream.zfree = Z_NULL;
		loader->_stream.opaque = Z_NULL;
		loader->_stream.avail_in = loader->_length;
		loader->_stream.next_in = loader->_data;
		loader->_length = ReadLE32((const UINT8 *)&loader->_data[loader->_length - 4]);
		if(inflateInit2(&loader->_stream,47) != Z_OK) {
			return 0x01;
		}
	}
	else
	{
		loader->_modeCompr = MLMODE_CMP_RAW;
	}
	return 0x00;
}

static UINT32 MemoryLoader_dread(void *context, UINT8 *buffer, UINT32 numBytes)
{
	MEMORY_LOADER *loader = (MEMORY_LOADER *)context;
	if(loader->_pos >= loader->_length) return 0;
	if(loader->_pos + numBytes > loader->_length)
		numBytes = loader->_length - loader->_pos;

	if(loader->_modeCompr == MLMODE_CMP_RAW) {
		memcpy(buffer,loader->_data + loader->_pos,numBytes);
		loader->_pos += numBytes;
		return numBytes;
	}
	int ret = 0;
	UINT32 bytesWritten = 0;

	loader->_stream.avail_out = numBytes;
	loader->_stream.next_out = buffer;
	ret = inflate(&loader->_stream,Z_SYNC_FLUSH);

	if(ret != Z_OK) return 0;

	bytesWritten = loader->_stream.total_out - loader->_pos;
	loader->_pos = loader->_stream.total_out;
	return bytesWritten;
}

static UINT32 MemoryLoader_dlength(void *context)
{
	MEMORY_LOADER *loader = (MEMORY_LOADER *)context;
	return loader->_length;
}

static INT32 MemoryLoader_dtell(void *context)
{
	(void)context;
	return -1; /* TODO */
}

static UINT8 MemoryLoader_dseek(void *context, UINT32 offset, UINT8 whence)
{
	(void)context;
	(void)offset;
	(void)whence;
	return 0; /* TODO */
}

static UINT8 MemoryLoader_dclose(void *context)
{
	MEMORY_LOADER *loader = (MEMORY_LOADER *)context;
	if(loader->_modeCompr == MLMODE_CMP_GZ) {
		inflateEnd(&loader->_stream);
	}
	return 0x00;
}

static UINT8 MemoryLoader_deof(void *context) {
	MEMORY_LOADER *loader = (MEMORY_LOADER *)context;
	return loader->_pos == loader->_length;

}

const DATA_LOADER_CALLBACKS memoryLoader = {
	.type	= "Default Memory Loader",
	.dopen   = MemoryLoader_dopen,
	.dread   = MemoryLoader_dread,
	.dseek   = MemoryLoader_dseek,
	.dclose  = MemoryLoader_dclose,
	.dtell   = MemoryLoader_dtell,
	.dlength = MemoryLoader_dlength,
	.deof	= MemoryLoader_deof,
};

DATA_LOADER *MemoryLoader_Init(UINT8 *buffer, UINT32 length)
{
	DATA_LOADER *dLoader = (DATA_LOADER *)malloc(sizeof(DATA_LOADER));
	if(dLoader == NULL) return NULL;
	memset(dLoader,0,sizeof(DATA_LOADER));

	MEMORY_LOADER *mLoader = (MEMORY_LOADER *)malloc(sizeof(MEMORY_LOADER));
	if(mLoader == NULL)
	{
		MemoryLoader_Deinit(dLoader);
		return NULL;
	}
	memset(mLoader,0,sizeof(MEMORY_LOADER));

	mLoader->_data = buffer;
	mLoader->_length = length;

	DataLoader_Setup(dLoader,&memoryLoader,mLoader);

	return dLoader;
}

void MemoryLoader_Deinit(DATA_LOADER *dLoader)
{
	if(dLoader == NULL) return;
	DataLoader_Reset(dLoader);
	if(dLoader->_context) free(dLoader->_context);
	free(dLoader);
}
