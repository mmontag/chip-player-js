// Windows Mutexes
// ---------------

#include <stdlib.h>
#include <stddef.h>

#include <windows.h>

#include "../stdtype.h"
#include "OSMutex.h"

//typedef struct _os_mutex OS_MUTEX;
struct _os_mutex
{
	HANDLE hMutex;
};

UINT8 OSMutex_Init(OS_MUTEX** retMutex, UINT8 initLocked)
{
	OS_MUTEX* mtx;
	
	mtx = (OS_MUTEX*)calloc(1, sizeof(OS_MUTEX));
	if (mtx == NULL)
		return 0xFF;
	
	mtx->hMutex = CreateMutex(NULL, initLocked ? TRUE : FALSE, NULL);
	if (mtx->hMutex == NULL)
	{
		free(mtx);
		return 0x80;
	}
	
	*retMutex = mtx;
	return 0x00;
}

void OSMutex_Deinit(OS_MUTEX* mtx)
{
	CloseHandle(mtx->hMutex);
	free(mtx);
	
	return;
}

UINT8 OSMutex_Lock(OS_MUTEX* mtx)
{
	DWORD retVal;
	
	retVal = WaitForSingleObject(mtx->hMutex, INFINITE);
	if (retVal == WAIT_OBJECT_0)
		return 0x00;
	else
		return 0xFF;
}

UINT8 OSMutex_TryLock(OS_MUTEX* mtx)
{
	DWORD retVal;
	
	retVal = WaitForSingleObject(mtx->hMutex, 0);
	if (retVal == WAIT_OBJECT_0)
		return 0x00;
	else if (retVal == WAIT_TIMEOUT)
		return 0x01;
	else
		return 0xFF;
}

UINT8 OSMutex_Unlock(OS_MUTEX* mtx)
{
	BOOL retVal;
	
	retVal = ReleaseMutex(mtx->hMutex);
	return retVal ? 0x00 : 0xFF;
}
