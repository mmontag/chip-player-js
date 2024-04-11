#ifndef __AUDIOSTRUCTS_H__
#define __AUDIOSTRUCTS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"

typedef struct _audio_options
{
	UINT32 sampleRate;
	UINT8 numChannels;
	UINT8 numBitsPerSmpl;
	
	UINT32 usecPerBuf;
	UINT32 numBuffers;
} AUDIO_OPTS;

typedef struct _audio_device_list
{
	UINT32 devCount;
	char** devNames;
} AUDIO_DEV_LIST;


typedef UINT32 (*AUDFUNC_FILLBUF)(void* drvStruct, void* userParam, UINT32 bufSize, void* data);

typedef UINT8 (*AUDFUNC_COMMON)(void);
typedef const AUDIO_DEV_LIST* (*AUDFUNC_DEVLIST)(void);
typedef AUDIO_OPTS* (*AUDFUNC_DEFOPTS)(void);
typedef UINT8 (*AUDFUNC_DRVCOMMON)(void* drvObj);
typedef UINT8 (*AUDFUNC_DRVCREATE)(void** retDrvObj);
typedef UINT8 (*AUDFUNC_DRVSTART)(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
typedef UINT32 (*AUDFUNC_DRVRET32)(void* drvObj);
typedef UINT8 (*AUDFUNC_DRVSETCB)(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
typedef UINT8 (*AUDFUNC_DRVWRTDATA)(void* drvObj, UINT32 dataSize, void* data);


typedef struct _audio_driver_info
{
	UINT8 drvType;
	UINT8 drvSig;
	const char* drvName;
} AUDDRV_INFO;
typedef struct _audio_driver
{
	AUDDRV_INFO drvInfo;
	
	AUDFUNC_COMMON IsAvailable;
	AUDFUNC_COMMON Init;
	AUDFUNC_COMMON Deinit;
	AUDFUNC_DEVLIST GetDevList;	// device list
	AUDFUNC_DEFOPTS GetDefOpts;	// default options
	
	AUDFUNC_DRVCREATE Create;	// returns driver structure
	AUDFUNC_DRVCOMMON Destroy;	// Note: caller must clear its reference pointer manually
	AUDFUNC_DRVSTART Start;
	AUDFUNC_DRVCOMMON Stop;
	
	AUDFUNC_DRVCOMMON Pause;
	AUDFUNC_DRVCOMMON Resume;
	
	AUDFUNC_DRVSETCB SetCallback;
	AUDFUNC_DRVRET32 GetBufferSize;	// returns bytes, 0 = unbuffered
	AUDFUNC_DRVCOMMON IsBusy;
	AUDFUNC_DRVWRTDATA WriteData;
	
	AUDFUNC_DRVRET32 GetLatency;	// returns msec
} AUDIO_DRV;



#define ADRVTYPE_NULL	0x00	// does nothing
#define ADRVTYPE_OUT	0x01	// stream to speakers
#define ADRVTYPE_DISK	0x02	// write to disk

#define ADRVSIG_WAVEWRT	0x01	// WAV Writer
#define ADRVSIG_WINMM	0x10	// [Windows] WinMM
#define ADRVSIG_DSOUND	0x11	// [Windows] DirectSound
#define ADRVSIG_XAUD2	0x12	// [Windows] XAudio2
#define ADRVSIG_WASAPI	0x13	// [Windows] Windows Audio Session API
#define ADRVSIG_OSS		0x20	// [Linux] Open Sound System (/dev/dsp)
#define ADRVSIG_SADA	0x21	// [NetBSD] Solaris Audio Device Architecture (/dev/audio)
#define ADRVSIG_ALSA 	0x22	// [Linux] Advanced Linux Sound Architecture
#define ADRVSIG_PULSE 	0x23	// [Linux] PulseAudio
#define ADRVSIG_CA      0x24    // [macOS] Core Audio
#define ADRVSIG_LIBAO	0x40	// libao



#define AERR_OK				0x00
#define AERR_BUSY			0x01
#define AERR_TOO_MUCH_DATA	0x02	// data to be written is larger than buffer size
#define AERR_WASDONE		0x18	// Init-call was already called with success
#define AERR_NODRVS			0x20	// no Audio Drivers found
#define AERR_INVALID_DRV	0x21	// invalid Audio Driver ID
#define AERR_DRV_DISABLED	0x22	// driver was disabled
#define AERR_INVALID_DEV	0x41
#define AERR_NO_SUPPORT		0x80	// not supported
#define AERR_BAD_MODE		0x81

#define AERR_FILE_ERR		0xC0
#define AERR_NOT_OPEN		0xC1

#define AERR_API_ERR		0xF0
#define AERR_CALL_SPC_FUNC	0xF1	// Needs a driver-specific function to be called first.

#ifdef __cplusplus
}
#endif

#endif	// __AUDIOSTRUCTS_H__
