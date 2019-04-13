#ifndef __MEMORYLOADER_H__
#define __MEMORYLOADER_H__

#include "DataLoader.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const DATA_LOADER_CALLBACKS memoryLoader;

DATA_LOADER *MemoryLoader_Init(UINT8 *buffer, UINT32 length);
void MemoryLoader_Deinit(DATA_LOADER *);

#define MemoryLoader_Load(c) DataLoader_Load(c)
#define MemoryLoader_Reset(c) DataLoader_Reset(c)
#define MemoryLoader_GetData(c) DataLoader_GetData(c)
#define MemoryLoader_GetTotalSize(c) DataLoader_GetTotalSize(c)
#define MemoryLoader_GetSize(c) DataLoader_GetSize(c)
#define MemoryLoader_GetSize(c) DataLoader_GetSize(c)
#define MemoryLoader_GetStatus(c) DataLoader_GetStatus(c)
#define MemoryLoader_Read(c) DataLoader_Read(c)
#define MemoryLoader_CancelLoading(c) DataLoader_CancelLoading(c)
#define MemoryLoader_SetPreloadBytes(c) DataLoader_SetPreloadBytes(c)
#define MemoryLoader_ReadUntil(c) DataLoader_ReadUntil(c)
#define MemoryLoader_ReadAll(c) DataLoader_ReadAll(c)

#ifdef __cplusplus
}
#endif

#endif
