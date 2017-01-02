#ifndef __EMUHELPER_H__
#define __EMUHELPER_H__

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

#endif	// __EMUHELPER_H__
