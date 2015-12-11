#ifndef __EMUSTRUCTS_H__
#define __EMUSTRUCTS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdtype.h>
#include "snddef.h"

typedef void (*DEVCB_SRATE_CHG)(void* info, UINT32 newSRate);

typedef void (*DEVFUNC_CTRL)(void* info);
typedef void (*DEVFUNC_UPDATE)(void* info, UINT32 samples, DEV_SMPL** outputs);
typedef void (*DEVFUNC_OPTMASK)(void* info, UINT32 optionBits);
typedef void (*DEVFUNC_PANALL)(void* info, INT16* channelPanVal);

typedef UINT8 (*DEVFUNC_READ_A8D8)(void* info, UINT8 addr);
typedef UINT16 (*DEVFUNC_READ_A8D16)(void* info, UINT8 addr);

typedef void (*DEVFUNC_WRITE_A8D8)(void* info, UINT8 addr, UINT8 data);
typedef void (*DEVFUNC_WRITE_A8D16)(void* info, UINT8 addr, UINT16 data);
typedef void (*DEVFUNC_WRITE_A16D8)(void* info, UINT16 addr, UINT8 data);
typedef void (*DEVFUNC_WRITE_A16D16)(void* info, UINT16 addr, UINT16 data);

#define DEVRW_A8D8		0x11	//  8-bit address,  8-bit data
#define DEVRW_A8D16		0x12	//  8-bit address, 16-bit data
#define DEVRW_A16D8		0x21	// 16-bit address,  8-bit data
#define DEVRW_A16D16	0x22	// 16-bit address, 16-bit data

typedef struct _devinf_readwrite_functions
{
	UINT8 rType;	// read function type, see DEVRW_ constants
	UINT8 wType;	// write function type, see DEVRW_ constants
	UINT8 qwType;
	
	void* Read;
	void* Write;
	void* QuickWrite;
} DEVINF_RWFUNCS;

// generic device data structure
// MUST be the first variable included in all device-specifc structures
typedef struct _device_data
{
	void* chipInf;	// pointer to CHIP_INF (depends on specific chip)
} DEV_DATA;
typedef struct _device_info
{
	DEV_DATA* dataPtr;	// points to chip data structure
	UINT32 sampleRate;
	
	DEVFUNC_CTRL Stop;
	DEVFUNC_CTRL Reset;
	DEVFUNC_UPDATE Update;
	
	DEVFUNC_OPTMASK SetOptionBits;
	DEVFUNC_OPTMASK SetMuteMask;
	DEVFUNC_PANALL SetPanning;
	
	DEVINF_RWFUNCS rwFuncs;
} DEV_INFO;


#define DEVRI_SRMODE_NATIVE		0x00
#define DEVRI_SRMODE_CUSTOM		0x01
#define DEVRI_SRMODE_HIGHEST	0x02
typedef struct _device_generic_config
{
	UINT8 emuCore;		// emulation core (if multiple ones are available)
	UINT8 srMode;		// sample rate mode
	
	UINT32 clock;		// chip clock
	UINT32 smplRate;	// sample rate for SRMODE_CUSTOM/DEVRI_SRMODE_HIGHEST
						// Note: Some cores ignore the srMode setting and always use smplRate.
} DEV_GEN_CFG;


#ifdef __cplusplus
}
#endif

#endif	// __EMUSTRUCTS_H__
