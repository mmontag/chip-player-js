/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See docs/COPYING
 * for more information.
 */

#include "common.h"
#include "virtual.h"
#include "mixer.h"
#include "synth.h"


/* Mixers
 *
 * To increase performance eight mixers are defined, one for each
 * combination of the following parameters: interpolation, resolution
 * and number of channels.
 */
#define INTERPOLATE() do { \
    pos += frac >> SMIX_SHIFT; \
    frac &= SMIX_MASK; \
    smp_l1 = sptr[pos]; \
    smp_dt = sptr[pos + 1] - smp_l1; \
    smp_in = smp_l1 + ((frac * smp_dt) >> SMIX_SHIFT); \
} while (0)

#define DONT_INTERPOLATE() do { \
    pos += frac >> SMIX_SHIFT; \
    frac &= SMIX_MASK; \
    smp_in = sptr[pos]; \
} while (0)

#define SAVE_FILTER() do { \
    vi->filter.l1 = fl1; \
    vi->filter.l2 = fl2; \
    vi->filter.r1 = fr1; \
    vi->filter.r2 = fr2; \
} while (0)

#define MIX_STEREO() do { \
    *(buffer++) += smp_in * vr; \
    *(buffer++) += smp_in * vl; \
    frac += step; \
} while (0)

#define MIX_MONO() do { \
    *(buffer++) += smp_in * vl; \
    frac += step; \
} while (0)

#define MIX_STEREO_AC() do { \
    if (vi->attack) { \
	int a = SLOW_ATTACK - vi->attack; \
	*(buffer++) += smp_in * vr * a / SLOW_ATTACK; \
	*(buffer++) += smp_in * vl * a / SLOW_ATTACK; \
	vi->attack--; \
    } else { \
	*(buffer++) += smp_in * vr; \
	*(buffer++) += smp_in * vl; \
    } \
    frac += step; \
} while (0)

#define MIX_STEREO_AC_FILTER() do { \
    sr = smp_in * vr; \
    sl = smp_in * vl; \
    sr = (a0 * sr + b0 * fr1 + b1 * fr2) / FILTER_PRECISION; \
    sl = (a0 * sl + b0 * fr1 + b1 * fr2) / FILTER_PRECISION; \
    fr2 = fr1; fr1 = sr; \
    fl2 = fl1; fl1 = sl; \
    if (vi->attack) { \
	int a = SLOW_ATTACK - vi->attack; \
	*(buffer++) += (int)(sr * a / SLOW_ATTACK); \
	*(buffer++) += (int)(sl * a / SLOW_ATTACK); \
	vi->attack--; \
    } else { \
	*(buffer++) += (int)sr; \
	*(buffer++) += (int)sl; \
    } \
    frac += step; \
} while (0)

#define MIX_MONO_AC() do { \
    if (vi->attack) { \
	*(buffer++) += smp_in * vl * (SLOW_ATTACK - vi->attack) / SLOW_ATTACK; \
	vi->attack--; \
    } else { \
	*(buffer++) += smp_in * vl; \
    } \
    frac += step; \
} while (0)

#define MIX_MONO_AC_FILTER() do { \
    sl = smp_in * vl; \
    sl = (a0 * sl + b0 * fr1 + b1 * fr2) / FILTER_PRECISION; \
    fl2 = fl1; fl1 = sl; \
    if (vi->attack) { \
	*(buffer++) += (int)(sl * (SLOW_ATTACK - vi->attack) / SLOW_ATTACK); \
	vi->attack--; \
    } else { \
	*(buffer++) += (int)sl; \
    } \
    frac += step; \
} while (0)

#define VAR_NORM(x) \
    register int smp_in; \
    x *sptr = vi->sptr; \
    int pos = vi->pos; \
    int frac = vi->frac

#define VAR_ITPT(x) \
    VAR_NORM(x); \
    int smp_l1, smp_dt

#define VAR_FILT \
    float fl1 = vi->filter.l1, fl2 = vi->filter.l2; \
    float fr1 = vi->filter.r1, fr2 = vi->filter.r2; \
    float a0 = (float)vi->filter.a0; \
    float b0 = (float)vi->filter.b0; \
    float b1 = (float)vi->filter.b1; \
    float sr, sl

#define SMIX_MIXER(f) void f(struct mixer_voice *vi, int *buffer, \
    int count, int vl, int vr, int step)


/* Handler for 8 bit samples, interpolated stereo output
 */
SMIX_MIXER(smix_st8itpt)
{
    VAR_ITPT(int8);
    while (count--) { INTERPOLATE(); MIX_STEREO_AC(); }
}


/* Handler for 16 bit samples, interpolated stereo output
 */
SMIX_MIXER(smix_st16itpt)
{
    VAR_ITPT(int16);

    vl >>= 8;
    vr >>= 8;
    while (count--) { INTERPOLATE(); MIX_STEREO_AC(); }
}


/* Handler for 8 bit samples, non-interpolated stereo output
 */
SMIX_MIXER(smix_st8norm)
{
    VAR_NORM(int8);
    while (count--) { DONT_INTERPOLATE(); MIX_STEREO(); }
}


/* Handler for 16 bit samples, non-interpolated stereo output
 */
SMIX_MIXER(smix_st16norm)
{
    VAR_NORM(int16);

    vl >>= 8;
    vr >>= 8;
    while (count--) { DONT_INTERPOLATE(); MIX_STEREO(); }
}


/* Handler for 8 bit samples, interpolated mono output
 */
SMIX_MIXER(smix_mn8itpt)
{
    VAR_ITPT(int8);

    vl <<= 1;
    while (count--) { INTERPOLATE(); MIX_MONO_AC(); }
}


/* Handler for 16 bit samples, interpolated mono output
 */
SMIX_MIXER(smix_mn16itpt)
{
    VAR_ITPT(int16);

    vl >>= 7;
    while (count--) { INTERPOLATE(); MIX_MONO_AC(); }
}


/* Handler for 8 bit samples, non-interpolated mono output
 */
SMIX_MIXER(smix_mn8norm)
{
    VAR_NORM(int8);

    vl <<= 1;
    while (count--) { DONT_INTERPOLATE(); MIX_MONO(); }
}


/* Handler for 16 bit samples, non-interpolated mono output
 */
SMIX_MIXER(smix_mn16norm)
{
    VAR_NORM(int16);

    vl >>= 7;
    while (count--) { DONT_INTERPOLATE(); MIX_MONO(); }
}

/*
 * Filtering mixers
 */

/* Handler for 8 bit samples, interpolated stereo output
 */
SMIX_MIXER(smix_st8itpt_flt)
{
    VAR_ITPT(int8);
    VAR_FILT;

    while (count--) { INTERPOLATE(); MIX_STEREO_AC_FILTER(); }
    SAVE_FILTER();
}


/* Handler for 16 bit samples, interpolated stereo output
 */
SMIX_MIXER(smix_st16itpt_flt)
{
    VAR_ITPT(int16);
    VAR_FILT;

    vl >>= 8;
    vr >>= 8;
    while (count--) { INTERPOLATE(); MIX_STEREO_AC_FILTER(); }
    SAVE_FILTER();
}


/* Handler for 8 bit samples, interpolated mono output
 */
SMIX_MIXER(smix_mn8itpt_flt)
{
    VAR_ITPT(int8);
    VAR_FILT;

    vl <<= 1;
    while (count--) { INTERPOLATE(); DO_FILTER(); MIX_MONO_AC_FILTER(); }
    SAVE_FILTER();
}


/* Handler for 16 bit samples, interpolated mono output
 */
SMIX_MIXER(smix_mn16itpt_flt)
{
    VAR_ITPT(int16);
    VAR_FILT;

    vl >>= 7;
    while (count--) { INTERPOLATE(); DO_FILTER(); MIX_MONO_AC_FILTER(); }
    SAVE_FILTER();
}

