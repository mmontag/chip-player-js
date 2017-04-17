#ifndef __RATIOCNTR_H__
#define __RATIOCNTR_H__

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

#endif	// __RATIOCNTR_H__
