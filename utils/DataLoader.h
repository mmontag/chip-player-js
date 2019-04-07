#ifndef __DATALOADER_H__
#define __DATALOADER_H__

#include <stdtype.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DataLoaderCallbacks
{
	const char *type; /* for storing a human-readable file loader type */
	void * (* dopen   )(void *context, const void *param); /* open a file, URL, piece of memory, return a pointer to it, NULL on error */
	UINT32 (* dread   )(void *context, UINT8 *buffer, UINT32 numBytes); /* read bytes into buffer */
	UINT8  (* dseek   )(void *context, UINT32 offset, UINT8 whence); /* seek to byte offset */
	void * (* dclose  )(void *context); /* closes out file, return NULL context if appropriate */
	INT32  (* dtell   )(void *context); /* returns the current position of the data */
	UINT32 (* dlength )(void *context); /* returns the length of the data, in bytes */
	UINT8  (* deof    )(void *context); /* determines if we've seen eof or not (return 1 for eof) */
};

typedef struct DataLoaderCallbacks DATA_LOADER_CALLBACKS;

enum
{
	DLSTAT_EMPTY = 0,
	DLSTAT_LOADING = 1,
	DLSTAT_LOADED = 2
};

struct DataLoader {
	UINT8 _status;
	UINT32 _bytesTotal;
	UINT32 _bytesLoaded;
	UINT32 _readStopOfs;
	UINT8 *_data;
	void *_context;
	const DATA_LOADER_CALLBACKS *_callbacks;
};

typedef struct DataLoader DATA_LOADER;

DATA_LOADER *DataLoader_New(const DATA_LOADER_CALLBACKS *callbacks, void *context);
void DataLoader_Delete(DATA_LOADER *loader);
UINT8 DataLoader_Init(DATA_LOADER *loader, const DATA_LOADER_CALLBACKS *callbacks, void *context);
UINT8 *DataLoader_GetData(DATA_LOADER *loader);
UINT32 DataLoader_GetTotalSize(DATA_LOADER *loader);
UINT32 DataLoader_GetSize(DATA_LOADER *loader);
UINT8 DataLoader_GetMode(DATA_LOADER *loader);
UINT8 DataLoader_GetStatus(DATA_LOADER *loader);
UINT32 DataLoader_Read(DATA_LOADER *loader, UINT32 numBytes);
UINT8 DataLoader_CancelLoading(DATA_LOADER *loader);
UINT8 DataLoader_Load(DATA_LOADER *loader, const void *loadParam);
UINT8 DataLoader_Free(DATA_LOADER *loader);
void DataLoader_SetPreloadBytes(DATA_LOADER *loader, UINT32 byteCount);
void DataLoader_ReadUntil(DATA_LOADER *loader, UINT32 fileOffset);
void DataLoader_ReadAll(DATA_LOADER *loader);

#ifdef __cplusplus
}
#endif

#endif /* __DATALOADER_H__ */
