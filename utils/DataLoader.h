#ifndef __DATALOADER_H__
#define __DATALOADER_H__

#include <stdtype.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DataLoaderCallbacks
{
	const char *type; /* for storing a human-readable file loader type */
	UINT8  (* dopen   )(void *context ); /* open a file, URL, piece of memory, return 0 on success */
	UINT32 (* dread   )(void *context, UINT8 *buffer, UINT32 numBytes); /* read bytes into buffer */
	UINT8  (* dseek   )(void *context, UINT32 offset, UINT8 whence); /* seek to byte offset */
	UINT8  (* dclose  )(void *context); /* closes out file, return 0 on success */
	INT32  (* dtell   )(void *context); /* returns the current position of the data */
	UINT32 (* dlength )(void *context); /* returns the length of the data, in bytes */
	UINT8  (* deof	  )(void *context); /* determines if we've seen eof or not (return 1 for eof) */
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
	const DATA_LOADER_CALLBACKS *_callbacks;
	void *_context;
};

typedef struct DataLoader DATA_LOADER;

/* calls the dopen and dlength functions
 * by default, loads whole file into memory, use
 * DataLoader_SetPreloadBytes to change this */
UINT8 DataLoader_Load(DATA_LOADER *loader);

/* Resets the DataLoader (calls DataReader_CancelLoading, unloads data, etc */
UINT8 DataLoader_Reset(DATA_LOADER *loader);

/* Returns a pointer to the DataLoader's memory buffer
 * call after any invocation of "Read", "ReadUntil", etc,
 * since the memory buffer pointer can change */
UINT8 *DataLoader_GetData(DATA_LOADER *loader);

/* returns _bytesTotal */
UINT32 DataLoader_GetTotalSize(DATA_LOADER *loader);

/* returns _bytesLoaded */
UINT32 DataLoader_GetSize(DATA_LOADER *loader);

/* returns _status */
UINT8 DataLoader_GetStatus(DATA_LOADER *loader);

/* calls dread, then deof
 * if deof > 0, also calls CancelLoading (dclose) */
UINT32 DataLoader_Read(DATA_LOADER *loader, UINT32 numBytes);

/* calls dclose */
UINT8 DataLoader_CancelLoading(DATA_LOADER *loader);

/* sets number of bytes to preload */
void DataLoader_SetPreloadBytes(DATA_LOADER *loader, UINT32 byteCount);

/* reads until offset */
void DataLoader_ReadUntil(DATA_LOADER *loader, UINT32 fileOffset);

/* read all data */
void DataLoader_ReadAll(DATA_LOADER *loader);

/* convenience function for MemoryLoader,FileLoader, etc */
void DataLoader_Setup(DATA_LOADER *loader, const DATA_LOADER_CALLBACKS *callbacks, void *context);


#ifdef __cplusplus
}
#endif

#endif /* __DATALOADER_H__ */
