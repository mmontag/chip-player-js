// POSIX Threads
// -------------

#include <stdlib.h>
#include <stddef.h>

#include <pthread.h>

#include "../stdtype.h"
#include "OSThread.h"

//typedef struct _os_thread OS_THREAD;
struct _os_thread
{
	pthread_t id;
	OS_THR_FUNC func;
	void* args;
};

static void* OSThread_Main(void* param);

UINT8 OSThread_Init(OS_THREAD** retThread, OS_THR_FUNC threadFunc, void* args)
{
	OS_THREAD* thr;
	int retVal;
	
	thr = (OS_THREAD*)calloc(1, sizeof(OS_THREAD));
	if (thr == NULL)
		return 0xFF;
	
	thr->func = threadFunc;
	thr->args = args;
	
	retVal = pthread_create(&thr->id, NULL, &OSThread_Main, thr);
	if (retVal)
	{
		free(thr);
		return 0x80;
	}
	
	*retThread = thr;
	return 0x00;
}

static void* OSThread_Main(void* param)
{
	OS_THREAD* thr = (OS_THREAD*)param;
	thr->func(thr->args);
	return 0;
}

void OSThread_Deinit(OS_THREAD* thr)
{
	if (thr->id)
		pthread_detach(thr->id);	// release handle
	free(thr);
	
	return;
}

void OSThread_Join(OS_THREAD* thr)
{
	if (! thr->id)
		return;
	
	pthread_join(thr->id, NULL);
	thr->id = 0;	// mark as "joined"
	
	return;
}

void OSThread_Cancel(OS_THREAD* thr)
{
	int retVal;
	
	if (! thr->id)
		return;
	
	retVal = pthread_cancel(thr->id);
	if (! retVal)
		thr->id = 0;	// stopped successfully
	
	return;
}

UINT64 OSThread_GetID(const OS_THREAD* thr)
{
#ifdef __APPLE__
	UINT64 idNum;
	pthread_threadid_np (thr->id, &idNum);
	return idNum;
#else
	return thr->id;
#endif
}

void* OSThread_GetHandle(OS_THREAD* thr)
{
	return &thr->id;
}
