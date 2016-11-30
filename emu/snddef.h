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

// macros to extract the actual clock value and the "special flag"
// from the VGM clock value
#define CHPCLK_CLOCK(clock)	 (clock & 0x7FFFFFFF)
#define CHPCLK_FLAG(clock)	((clock & 0x80000000) >> 31)


#define FCC_MAME	0x4D414D45	// MAME
#define FCC_MAXM	0x4D41584D	// Maxim emulation
#define FCC_GPGX	0x47504758	// Genesis Plus GX

#endif	// __SNDDEF_H__
