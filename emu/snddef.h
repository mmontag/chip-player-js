#ifndef __SNDDEF_H__
#define __SNDDEF_H__

#include <common_def.h>

/* offsets and addresses are 32-bit (for now...) */
typedef UINT32	offs_t;

/* stream_sample_t is used to represent a single sample in a sound stream */
typedef INT32 DEV_SMPL;
//typedef INT32 stream_sample_t;

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#ifdef _DEBUG
#define logerror	printf
#else
#define logerror
#endif

//typedef void (*SRATE_CALLBACK)(void*, UINT32);

#endif	// __SNDDEF_H__
