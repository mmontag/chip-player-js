// Audio Stream - API functions
#define _CRTDBG_MAP_ALLOC
//#include <stdio.h>
#include <stdlib.h>
//#include <string.h>	// for memset

#include "../stdtype.h"
#include "../stdbool.h"

#include "AudioStream.h"
#include "../utils/OSMutex.h"


#ifdef AUDDRV_WAVEWRITE
extern AUDIO_DRV audDrv_WaveWrt;
#endif

#ifdef AUDDRV_WINMM
extern AUDIO_DRV audDrv_WinMM;
#endif
#ifdef AUDDRV_DSOUND
extern AUDIO_DRV audDrv_DSound;
#endif
#ifdef AUDDRV_XAUD2
extern AUDIO_DRV audDrv_XAudio2;
#endif
#ifdef AUDDRV_WASAPI
extern AUDIO_DRV audDrv_WASAPI;
#endif

#ifdef AUDDRV_OSS
extern AUDIO_DRV audDrv_OSS;
#endif
#ifdef AUDDRV_SADA
extern AUDIO_DRV audDrv_SADA;
#endif
#ifdef AUDDRV_ALSA
extern AUDIO_DRV audDrv_ALSA;
#endif
#ifdef AUDDRV_LIBAO
extern AUDIO_DRV audDrv_LibAO;
#endif
#ifdef AUDDRV_PULSE
extern AUDIO_DRV audDrv_Pulse;
#endif

#ifdef AUDDRV_CA
extern AUDIO_DRV audDrv_CA;
#endif

AUDIO_DRV* audDrivers[] =
{
#ifdef AUDDRV_WAVEWRITE
	&audDrv_WaveWrt,
#endif
#ifdef AUDDRV_WINMM
	&audDrv_WinMM,
#endif
#ifdef AUDDRV_DSOUND
	&audDrv_DSound,
#endif
#ifdef AUDDRV_XAUD2
	&audDrv_XAudio2,
#endif
#ifdef AUDDRV_WASAPI
	&audDrv_WASAPI,
#endif
#ifdef AUDDRV_OSS
	&audDrv_OSS,
#endif
#ifdef AUDDRV_SADA
	&audDrv_SADA,
#endif
#ifdef AUDDRV_ALSA
	&audDrv_ALSA,
#endif
#ifdef AUDDRV_LIBAO
	&audDrv_LibAO,
#endif
#ifdef AUDDRV_PULSE
	&audDrv_Pulse,
#endif
#ifdef AUDDRV_CA
	&audDrv_CA,
#endif
	NULL
};


typedef struct _audio_driver_load
{
	UINT8 flags;
	AUDIO_DRV* drvStruct;
} ADRV_LOAD;

typedef struct _audio_driver_instance ADRV_INSTANCE;
typedef struct _audio_driver_list ADRV_LIST;
struct _audio_driver_list
{
	const ADRV_INSTANCE* drvInst;
	ADRV_LIST* next;
};
struct _audio_driver_instance
{
	UINT32 ID;	// -1 = unused
	AUDIO_DRV* drvStruct;
	AUDIO_OPTS drvOpts;
	void* drvData;
	void* userParam;
	AUDFUNC_FILLBUF mainCallback;
	ADRV_LIST* forwardDrvs;
	OS_MUTEX* hMutex;	// for locking access to "forwardDrvs"
};

#define ADFLG_ENABLE	0x01
#define ADFLG_IS_INIT	0x02
#define ADFLG_AVAILABLE	0x80

#define ADID_UNUSED		(UINT32)-1


static UINT8 ADrvLst_Add(ADRV_LIST** headPtr, const ADRV_INSTANCE* drvData);
static UINT8 ADrvLst_Remove(ADRV_LIST** headPtr, const ADRV_INSTANCE* drvData);
static ADRV_LIST* ADrvLst_FindItem(ADRV_LIST* head, const ADRV_INSTANCE* drvData, ADRV_LIST** retLastItm);
static UINT8 ADrvLst_Clear(ADRV_LIST** headPtr);

UINT8 Audio_Init(void);
//UINT8 Audio_Deinit(void);
//UINT32 Audio_GetDriverCount(void);
//UINT8 Audio_GetDriverInfo(UINT32 drvID, AUDDRV_INFO** retDrvInfo);
static ADRV_INSTANCE* GetFreeRunSlot(void);
//UINT8 AudioDrv_Init(UINT32 drvID, void** retDrvStruct);
//UINT8 AudioDrv_Deinit(void** drvStruct);
//const AUDIO_DEV_LIST* AudioDrv_GetDeviceList(void* drvStruct);
//AUDIO_OPTS* AudioDrv_GetOptions(void* drvStruct);
//UINT8 AudioDrv_Start(void* drvStruct, UINT32 devID);
//UINT8 AudioDrv_Stop(void* drvStruct);
//UINT8 AudioDrv_Pause(void* drvStruct);
//UINT8 AudioDrv_Resume(void* drvStruct);
static UINT32 DoDataForwarding(void* drvStruct, void* userParam, UINT32 bufSize, void* data);
//UINT8 AudioDrv_SetCallback(void* drvStruct, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
//UINT8 AudioDrv_DataForward_Add(void* drvStruct, const void* destDrvStruct);
//UINT8 AudioDrv_DataForward_Remove(void* drvStruct, const void* destDrvStruct);
//UINT8 AudioDrv_DataForward_RemoveAll(void* drvStruct);
//UINT32 AudioDrv_GetBufferSize(void* drvStruct);
//UINT8 AudioDrv_IsBusy(void* drvStruct);
//UINT8 AudioDrv_WriteData(void* drvStruct, UINT32 dataSize, void* data);
//UINT32 AudioDrv_GetLatency(void* drvStruct);


static UINT32 audDrvCount = 0;
static ADRV_LOAD* audDrvLoaded = NULL;

static UINT32 runDevCount = 0;
static ADRV_INSTANCE* runDevices = NULL;

static bool isInit = false;


#include "AudioStream_LstFuncs.h"


UINT8 Audio_Init(void)
{
	UINT32 curDrv;
	UINT32 curDrvLd;
	AUDIO_DRV* tempDrv;
	ADRV_LOAD* tempDLoad;
	
	if (isInit)
		return AERR_WASDONE;
	
	audDrvCount = 0;
	while(audDrivers[audDrvCount] != NULL)
		audDrvCount ++;
	audDrvLoaded = (ADRV_LOAD*)malloc(audDrvCount * sizeof(ADRV_LOAD));
	
	curDrvLd = 0;
	for (curDrv = 0; curDrv < audDrvCount; curDrv ++)
	{
		tempDrv = audDrivers[curDrv];
		tempDLoad = &audDrvLoaded[curDrvLd];
		tempDLoad->drvStruct = tempDrv;
		if (tempDrv->IsAvailable())
			tempDLoad->flags = ADFLG_ENABLE | ADFLG_AVAILABLE;
		else
			tempDLoad->flags = 0x00;
		curDrvLd ++;
	}
	
	audDrvCount = curDrvLd;
	if (! audDrvCount)
	{
		free(audDrvLoaded);	audDrvLoaded = NULL;
		return AERR_NODRVS;
	}
	
	//runDevCount = 0;
	//runDevices = NULL;
	runDevCount = 0x10;
	runDevices = (ADRV_INSTANCE*)realloc(runDevices, runDevCount * sizeof(ADRV_INSTANCE));
	for (curDrv = 0; curDrv < runDevCount; curDrv ++)
		runDevices[curDrv].ID = -1;
	
	isInit = true;
	return AERR_OK;
}

UINT8 Audio_Deinit(void)
{
	UINT32 curDev;
	ADRV_INSTANCE* tempAIns;
	
	if (! isInit)
		return AERR_WASDONE;
	
	for (curDev = 0; curDev < runDevCount; curDev ++)
	{
		tempAIns = &runDevices[curDev];
		if (tempAIns->ID != ADID_UNUSED)
		{
			tempAIns->drvStruct->Stop(tempAIns->drvData);
			tempAIns->drvStruct->Destroy(tempAIns->drvData);
			ADrvLst_Clear(&tempAIns->forwardDrvs);
		}
	}
	for (curDev = 0; curDev < audDrvCount; curDev ++)
	{
		if (audDrvLoaded[curDev].flags & ADFLG_IS_INIT)
			audDrvLoaded[curDev].drvStruct->Deinit();
	}
	
	free(runDevices);	runDevices = NULL;
	runDevCount = 0;
	free(audDrvLoaded);	audDrvLoaded = NULL;
	audDrvCount = 0;
	
	isInit = false;
	return AERR_OK;
}

UINT32 Audio_GetDriverCount(void)
{
	return audDrvCount;
}

UINT8 Audio_GetDriverInfo(UINT32 drvID, AUDDRV_INFO** retDrvInfo)
{
	if (drvID >= audDrvCount)
		return AERR_INVALID_DRV;
	
	if (retDrvInfo != NULL)
		*retDrvInfo = &audDrvLoaded[drvID].drvStruct->drvInfo;
	return AERR_OK;
}

static ADRV_INSTANCE* GetFreeRunSlot(void)
{
	UINT32 curDev;
	
	for (curDev = 0; curDev < runDevCount; curDev ++)
	{
		if (runDevices[curDev].ID == ADID_UNUSED)
		{
			runDevices[curDev].ID = curDev;
			return &runDevices[curDev];
		}
	}
	return NULL;
	
	// Can't do realloc due to external use of references!!
	/*curDev = runDevCount;
	runDevCount ++;
	runDevices = (ADRV_INSTANCE*)realloc(runDevices, runDevCount * sizeof(ADRV_INSTANCE));
	runDevices[curDev].ID = curDev;
	return &runDevices[curDev];*/
}

UINT8 AudioDrv_Init(UINT32 drvID, void** retDrvStruct)
{
	ADRV_LOAD* tempDLoad;
	AUDIO_DRV* tempDrv;
	ADRV_INSTANCE* audInst;
	void* drvData;
	UINT8 retVal;
	
	if (drvID >= audDrvCount)
		return AERR_INVALID_DRV;
	tempDLoad = &audDrvLoaded[drvID];
	if (! (tempDLoad->flags & ADFLG_ENABLE))
		return AERR_DRV_DISABLED;
	
	tempDrv = tempDLoad->drvStruct;
	if (! (tempDLoad->flags & ADFLG_IS_INIT))
	{
		retVal = tempDrv->Init();
		if (retVal)
			return retVal;
		tempDLoad->flags |= ADFLG_IS_INIT;
	}
	
	retVal = tempDrv->Create(&drvData);
	if (retVal)
		return retVal;
	audInst = GetFreeRunSlot();
	audInst->drvStruct = tempDrv;
	audInst->drvOpts = *tempDrv->GetDefOpts();
	audInst->drvData = drvData;
	audInst->userParam = NULL;
	audInst->mainCallback = NULL;
	audInst->forwardDrvs = NULL;
	audInst->hMutex = NULL;
	OSMutex_Init(&audInst->hMutex, 0);
	*retDrvStruct = (void*)audInst;
	
	return AERR_OK;
}

UINT8 AudioDrv_Deinit(void** drvStruct)
{
	ADRV_INSTANCE* audInst;
	AUDIO_DRV* aDrv;
	UINT8 retVal;
	
	if (drvStruct == NULL)
		return AERR_OK;
	
	audInst = (ADRV_INSTANCE*)(*drvStruct);
	aDrv = audInst->drvStruct;
	if (audInst->ID == ADID_UNUSED)
		return AERR_INVALID_DRV;
	
	retVal = aDrv->Stop(audInst->drvData);	// just in case
	// continue regardless of errors
	retVal = aDrv->Destroy(audInst->drvData);
	if (retVal)
		return retVal;
	
	*drvStruct = NULL;
	audInst->ID = ADID_UNUSED;
	audInst->drvStruct = NULL;
	audInst->drvData = NULL;
	audInst->userParam = NULL;
	audInst->mainCallback = NULL;
	ADrvLst_Clear(&audInst->forwardDrvs);
	OSMutex_Deinit(audInst->hMutex);
	audInst->hMutex = NULL;
	return AERR_OK;
}

const AUDIO_DEV_LIST* AudioDrv_GetDeviceList(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	
	return aDrv->GetDevList();
}

AUDIO_OPTS* AudioDrv_GetOptions(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	
	return &audInst->drvOpts;
}

void* AudioDrv_GetDrvData(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	
	if (audInst == NULL || audInst->ID == ADID_UNUSED)
		return NULL;
	
	return audInst->drvData;
}

UINT8 AudioDrv_Start(void* drvStruct, UINT32 devID)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	UINT8 retVal;
	
	retVal = aDrv->Start(audInst->drvData, devID, &audInst->drvOpts, audInst);
	if (retVal)
		return retVal;
	
	return AERR_OK;
}

UINT8 AudioDrv_Stop(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	UINT8 retVal;
	
	retVal = aDrv->Stop(audInst->drvData);
	if (retVal)
		return retVal;
	
	return AERR_OK;
}

UINT8 AudioDrv_Pause(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	
	return aDrv->Pause(audInst->drvData);
}

UINT8 AudioDrv_Resume(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	
	return aDrv->Resume(audInst->drvData);
}

static UINT32 DoDataForwarding(void* drvStruct, void* userParam, UINT32 bufSize, void* data)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	ADRV_LIST* fwdList;
	const ADRV_INSTANCE* fwdInst;
	UINT32 dataSize;
	
	OSMutex_Lock(audInst->hMutex);
	// Using audInst->userParam instead of the userParam parameter makes
	// later changes of the userParam via SetCallback work properly.
	dataSize = audInst->mainCallback(drvStruct, audInst->userParam, bufSize, data);	// fill buffer
	fwdList = audInst->forwardDrvs;
	while(fwdList != NULL)
	{
		fwdInst = fwdList->drvInst;
		if (fwdInst->ID != ADID_UNUSED && fwdInst->drvStruct != NULL)
			fwdInst->drvStruct->WriteData(fwdInst->drvData, dataSize, data);
		fwdList = fwdList->next;
	}
	OSMutex_Unlock(audInst->hMutex);
	return dataSize;
}

UINT8 AudioDrv_SetCallback(void* drvStruct, AUDFUNC_FILLBUF FillBufCallback, void* userParam)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	UINT8 retVal;
	
	OSMutex_Lock(audInst->hMutex);
	audInst->userParam = userParam;
	audInst->mainCallback = FillBufCallback;
	if (audInst->forwardDrvs != NULL && audInst->mainCallback != NULL)
		retVal = aDrv->SetCallback(audInst->drvData, &DoDataForwarding, audInst->userParam);
	else
		retVal = aDrv->SetCallback(audInst->drvData, audInst->mainCallback, audInst->userParam);
	OSMutex_Unlock(audInst->hMutex);
	return retVal;
}

UINT8 AudioDrv_DataForward_Add(void* drvStruct, const void* destDrvStruct)
{
	ADRV_INSTANCE* audInstSrc = (ADRV_INSTANCE*)drvStruct;
	ADRV_INSTANCE* audInstDst = (ADRV_INSTANCE*)destDrvStruct;
	UINT8 retVal;
	
	if (audInstDst == NULL)
		return 0xFF;
	OSMutex_Lock(audInstSrc->hMutex);
	retVal = ADrvLst_Add(&audInstSrc->forwardDrvs, audInstDst);
	// If callbacks are enabled, make it use the Forwarding-Callback routine.
	if (audInstSrc->drvStruct != NULL && audInstSrc->mainCallback != NULL)
		audInstSrc->drvStruct->SetCallback(audInstSrc->drvData, &DoDataForwarding, audInstSrc->userParam);
	OSMutex_Unlock(audInstSrc->hMutex);
	return AERR_OK;
}

UINT8 AudioDrv_DataForward_Remove(void* drvStruct, const void* destDrvStruct)
{
	ADRV_INSTANCE* audInstSrc = (ADRV_INSTANCE*)drvStruct;
	ADRV_INSTANCE* audInstDst = (ADRV_INSTANCE*)destDrvStruct;
	UINT8 retVal;
	
	if (audInstDst == NULL)
		return 0xFF;
	OSMutex_Lock(audInstSrc->hMutex);
	retVal = ADrvLst_Remove(&audInstSrc->forwardDrvs, audInstDst);
	if (retVal)
	{
		OSMutex_Unlock(audInstSrc->hMutex);
		return retVal;
	}
	
	// make it call the original callback function
	if (audInstSrc->forwardDrvs == NULL && audInstSrc->drvStruct != NULL)
		audInstSrc->drvStruct->SetCallback(audInstSrc->drvData, audInstSrc->mainCallback, audInstSrc->userParam);
	OSMutex_Unlock(audInstSrc->hMutex);
	return AERR_OK;
}

UINT8 AudioDrv_DataForward_RemoveAll(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	
	UINT8 retVal;
	
	OSMutex_Lock(audInst->hMutex);
	retVal = ADrvLst_Clear(&audInst->forwardDrvs);
	if (retVal)
	{
		OSMutex_Unlock(audInst->hMutex);
		return retVal;
	}
	
	// make it call the original callback function
	if (audInst->drvStruct != NULL)
		audInst->drvStruct->SetCallback(audInst->drvData, audInst->mainCallback, audInst->userParam);
	OSMutex_Unlock(audInst->hMutex);
	return AERR_OK;
}

UINT32 AudioDrv_GetBufferSize(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	
	return aDrv->GetBufferSize(audInst->drvData);
}

UINT8 AudioDrv_IsBusy(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	
	return aDrv->IsBusy(audInst->drvData);
}

UINT8 AudioDrv_WriteData(void* drvStruct, UINT32 dataSize, void* data)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	ADRV_LIST* fwdList;
	const ADRV_INSTANCE* fwdInst;
	UINT8 retVal;
	
	retVal = aDrv->WriteData(audInst->drvData, dataSize, data);
	
	OSMutex_Lock(audInst->hMutex);
	fwdList = audInst->forwardDrvs;
	while(fwdList != NULL)
	{
		fwdInst = fwdList->drvInst;
		if (fwdInst->ID != ADID_UNUSED && fwdInst->drvStruct != NULL)
			fwdInst->drvStruct->WriteData(audInst->drvData, dataSize, data);
		fwdList = fwdList->next;
	}
	OSMutex_Unlock(audInst->hMutex);
	return retVal;
}

UINT32 AudioDrv_GetLatency(void* drvStruct)
{
	ADRV_INSTANCE* audInst = (ADRV_INSTANCE*)drvStruct;
	AUDIO_DRV* aDrv = audInst->drvStruct;
	
	return aDrv->GetLatency(audInst->drvData);
}
