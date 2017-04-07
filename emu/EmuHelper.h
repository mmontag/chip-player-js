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


#if ! LOW_PRECISION_RATIOCNTR
// by default we use a high-precision 32.32 fixed point counter
#define RC_SHIFT	32
typedef UINT64		RC_TYPE;
#else
// alternatively we can use lower-precision 12.20 fixed point
#define RC_SHIFT	20
typedef UINT32		RC_TYPE;
#endif

typedef struct
{
	RC_TYPE inc;	// counter increment
	RC_TYPE val;	// current value
} RATIO_CNTR;

INLINE void RC_SET_RATIO(RATIO_CNTR* rc, UINT32 mul, UINT32 div)
{
	rc->inc = (RC_TYPE)((((UINT64)mul << RC_SHIFT) + div / 2) / div);
}

INLINE void RC_SET_INC(RATIO_CNTR* rc, double val)
{
	rc->inc = (RC_TYPE)(((RC_TYPE)1 << RC_SHIFT) * val + 0.5);
}

INLINE void RC_STEP(RATIO_CNTR* rc)
{
	rc->val += rc->inc;
}

INLINE UINT32 RC_GET_VAL(const RATIO_CNTR* rc)
{
	return (UINT32)(rc->val >> RC_SHIFT);
}

INLINE void RC_SET_VAL(RATIO_CNTR* rc, UINT32 val)
{
	rc->val = (RC_TYPE)val << RC_SHIFT;
}

INLINE void RC_RESET(RATIO_CNTR* rc)
{
	rc->val = 0;
}

INLINE void RC_MASK(RATIO_CNTR* rc)
{
	rc->val &= (((RC_TYPE)1 << RC_SHIFT) - 1);
}


#endif	// __EMUHELPER_H__
