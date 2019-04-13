#ifndef __FILELOADER_H__
#define __FILELOADER_H__

#include "DataLoader.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const DATA_LOADER_CALLBACKS fileLoader;

DATA_LOADER *FileLoader_Init(const char *fileName);
void FileLoader_Deinit(DATA_LOADER *);

#define FileLoader_Load(c) DataLoader_Load(c)
#define FileLoader_Reset(c) DataLoader_Reset(c)
#define FileLoader_GetData(c) DataLoader_GetData(c)
#define FileLoader_GetTotalSize(c) DataLoader_GetTotalSize(c)
#define FileLoader_GetSize(c) DataLoader_GetSize(c)
#define FileLoader_GetSize(c) DataLoader_GetSize(c)
#define FileLoader_GetStatus(c) DataLoader_GetStatus(c)
#define FileLoader_Read(c) DataLoader_Read(c)
#define FileLoader_CancelLoading(c) DataLoader_CancelLoading(c)
#define FileLoader_SetPreloadBytes(c) DataLoader_SetPreloadBytes(c)
#define FileLoader_ReadUntil(c) DataLoader_ReadUntil(c)
#define FileLoader_ReadAll(c) DataLoader_ReadAll(c)

#ifdef __cplusplus
}
#endif

#endif
