// Windows Threads
// ---------------

#include <stdlib.h>
#include <stddef.h>

#include <windows.h>

#include "../stdtype.h"
#include "OSThread.h"

//typedef struct _os_thread OS_THREAD;
struct _os_thread
{
	DWORD id;
	HANDLE hThread;
	OS_THR_FUNC func;
	void* args;
};

static DWORD WINAPI OSThread_Main(LPVOID lpParam);

UINT8 OSThread_Init(OS_THREAD** retThread, OS_THR_FUNC threadFunc, void* args)
{
	OS_THREAD* thr;
	
	thr = (OS_THREAD*)calloc(1, sizeof(OS_THREAD));
	if (thr == NULL)
		return 0xFF;
	
	thr->func = threadFunc;
	thr->args = args;
	
	thr->hThread = CreateThread(NULL, 0, &OSThread_Main, thr, 0x00, &thr->id);
	if (thr->hThread == NULL)
	{
		free(thr);
		return 0x80;
	}
	
	*retThread = thr;
	return 0x00;
}

static DWORD WINAPI OSThread_Main(LPVOID lpParam)
{
	OS_THREAD* thr = (OS_THREAD*)lpParam;
	thr->func(thr->args);
	return 0;
}

void OSThread_Deinit(OS_THREAD* thr)
{
	CloseHandle(thr->hThread);
	free(thr);
	
	return;
}

void OSThread_Join(OS_THREAD* thr)
{
	if (! thr->id)
		return;
	
	WaitForSingleObject(thr->hThread, INFINITE);
	thr->id = 0;	// mark as "joined"
	
	return;
}

void OSThread_Cancel(OS_THREAD* thr)
{
	BOOL retVal;
	
	if (! thr->id)
		return;
	
	retVal = TerminateThread(thr->hThread, 1);
	if (retVal)
		thr->id = 0;	// stopped successfully
	
	return;
}

UINT64 OSThread_GetID(const OS_THREAD* thr)
{
	return thr->id;
}

void* OSThread_GetHandle(OS_THREAD* thr)
{
	return &thr->hThread;
}
