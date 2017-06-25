#ifndef __EMUHELPER_H__
#define __EMUHELPER_H__

#ifdef _USE_MATH_DEFINES
// MS VC6 doesn't have M_PI yet
#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif
#endif

#if _MSC_VEV < 1400
// Math function defines from VC2010's math.h for VC6
#ifndef powf
#define powf(x,y)   ((float)pow((double)(x), (double)(y)))
#endif
#endif

#ifdef _DEBUG
#define logerror	printf
#else
#define logerror
#endif

// get parent struct from chip data pointer
#define CHP_GET_INF_PTR(info)	(((DEV_DATA*)info)->chipInf)


#define SRATE_CUSTOM_HIGHEST(srmode, rate, customrate)	\
	if (srmode == DEVRI_SRMODE_CUSTOM ||	\
		(srmode == DEVRI_SRMODE_HIGHEST && rate < customrate))	\
		rate = customrate;


// round up to the nearest power of 2
// from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
INLINE UINT32 ceil_pow2(UINT32 v)
{
	v --;
	v |= (v >>  1);
	v |= (v >>  2);
	v |= (v >>  4);
	v |= (v >>  8);
	v |= (v >> 16);
	v ++;
	return v;
}

// create a mask that covers the range 0...v-1
INLINE UINT32 pow2_mask(UINT32 v)
{
	if (v == 0)
		return 0;
	v --;
	v |= (v >>  1);
	v |= (v >>  2);
	v |= (v >>  4);
	v |= (v >>  8);
	v |= (v >> 16);
	return v;
}

#endif	// __EMUHELPER_H__
