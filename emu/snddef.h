#ifndef __SNDDEF_H__
#define __SNDDEF_H__

#include <common_def.h>

/* offsets and addresses are 32-bit (for now...) */
typedef UINT32	offs_t;

/* stream_sample_t is used to represent a single sample in a sound stream */
typedef INT32 DEV_SMPL;

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#ifdef _DEBUG
#define logerror	printf
#else
#define logerror
#endif

// macros to extract the actual clock value and the "special flag"
// from the VGM clock value
#define CHPCLK_CLOCK(clock)	 (clock & 0x7FFFFFFF)
#define CHPCLK_FLAG(clock)	((clock & 0x80000000) >> 31)


#define SRATE_CUSTOM_HIGHEST(srmode, rate, customrate)	\
	if (srmode == DEVRI_SRMODE_CUSTOM ||	\
		(srmode == DEVRI_SRMODE_HIGHEST && rate < customrate))	\
		rate = customrate;

#endif	// __SNDDEF_H__
