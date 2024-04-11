// Windows Signals
// ---------------

#include <stdlib.h>
#include <stddef.h>

#include <windows.h>

#include "../stdtype.h"
#include "OSSignal.h"

//typedef struct _os_signal OS_SIGNAL;
struct _os_signal
{
	HANDLE hEvent;
};

UINT8 OSSignal_Init(OS_SIGNAL** retSignal, UINT8 initState)
{
	OS_SIGNAL* sig;
	
	sig = (OS_SIGNAL*)calloc(1, sizeof(OS_SIGNAL));
	if (sig == NULL)
		return 0xFF;
	
	sig->hEvent = CreateEvent(NULL, FALSE, initState ? TRUE : FALSE, NULL);
	if (sig->hEvent == NULL)
	{
		free(sig);
		return 0x80;
	}
	
	*retSignal = sig;
	return 0x00;
}

void OSSignal_Deinit(OS_SIGNAL* sig)
{
	CloseHandle(sig->hEvent);
	free(sig);
	
	return;
}

UINT8 OSSignal_Signal(OS_SIGNAL* sig)
{
	BOOL retVal;
	
	retVal = SetEvent(sig->hEvent);
	return retVal ? 0x00 : 0xFF;
}

UINT8 OSSignal_Reset(OS_SIGNAL* sig)
{
	BOOL retVal;
	
	retVal = ResetEvent(sig->hEvent);
	return retVal ? 0x00 : 0xFF;
}

UINT8 OSSignal_Wait(OS_SIGNAL* sig)
{
	DWORD retVal;
	
	retVal = WaitForSingleObject(sig->hEvent, INFINITE);
	if (retVal == WAIT_OBJECT_0)
		return 0x00;
	else
		return 0xFF;
}
