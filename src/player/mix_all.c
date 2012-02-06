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
    smp_x1 = sptr[pos]; \
    smp_dt = sptr[pos + 1] - smp_x1; \
    smp_in = smp_x1 + ((frac * smp_dt) >> SMIX_SHIFT); \
} while (0)

#define DONT_INTERPOLATE() do { \
    smp_in = sptr[frac >> SMIX_SHIFT]; \
} while (0)

#define DO_FILTER() do { \
    smp_in = (smp_in * vi->filter.B0 + fx1 * vi->filter.B1 + fx2 * vi->filter.B2) / FILTER_PRECISION; \
    fx2 = fx1; fx1 = smp_in; \
} while (0)

#define SAVE_FILTER() do { \
    vi->filter.X1 = fx1; \
    vi->filter.X2 = fx2; \
} while (0)

#define MIX_STEREO() do { \
    *(tmp_bk++) += smp_in * vr; \
    *(tmp_bk++) += smp_in * vl; \
    frac += step; \
} while (0)

#define MIX_MONO() do { \
    *(tmp_bk++) += smp_in * vl; \
    frac += step; \
} while (0)

#define MIX_STEREO_AC() do { \
    if (vi->attack) { \
	int a = SLOW_ATTACK - vi->attack; \
	*(tmp_bk++) += smp_in * vr * a / SLOW_ATTACK; \
	*(tmp_bk++) += smp_in * vl * a / SLOW_ATTACK; \
	vi->attack--; \
    } else { \
	*(tmp_bk++) += smp_in * vr; \
	*(tmp_bk++) += smp_in * vl; \
    } \
    frac += step; \
} while (0)

#define MIX_MONO_AC() do { \
    if (vi->attack) { \
	*(tmp_bk++) += smp_in * vl * (SLOW_ATTACK - vi->attack) / SLOW_ATTACK; \
	vi->attack--; \
    } else { \
	*(tmp_bk++) += smp_in * vl; \
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
    int smp_x1, smp_dt

#define VAR_FILT \
    int fx1 = vi->filter.X1, fx2 = vi->filter.X2

#define SMIX_MIXER(f) void f(struct voice_info *vi, int* tmp_bk, \
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

    sptr += pos;
    while (count--) { DONT_INTERPOLATE(); MIX_STEREO(); }
}


/* Handler for 16 bit samples, non-interpolated stereo output
 */
SMIX_MIXER(smix_st16norm)
{
    VAR_NORM(int16);

    vl >>= 8;
    vr >>= 8;
    sptr += pos;
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
    sptr += pos;
    while (count--) { DONT_INTERPOLATE(); MIX_MONO(); }
}


/* Handler for 16 bit samples, non-interpolated mono output
 */
SMIX_MIXER(smix_mn16norm)
{
    VAR_NORM(int16);

    vl >>= 7;
    sptr += pos;
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

    while (count--) { INTERPOLATE(); DO_FILTER(); MIX_STEREO_AC(); }
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
    while (count--) { INTERPOLATE(); DO_FILTER(); MIX_STEREO_AC(); }
    SAVE_FILTER();
}


/* Handler for 8 bit samples, interpolated mono output
 */
SMIX_MIXER(smix_mn8itpt_flt)
{
    VAR_ITPT(int8);
    VAR_FILT;

    vl <<= 1;
    while (count--) { INTERPOLATE(); DO_FILTER(); MIX_MONO_AC(); }
    SAVE_FILTER();
}


/* Handler for 16 bit samples, interpolated mono output
 */
SMIX_MIXER(smix_mn16itpt_flt)
{
    VAR_ITPT(int16);
    VAR_FILT;

    vl >>= 7;
    while (count--) { INTERPOLATE(); DO_FILTER(); MIX_MONO_AC(); }
    SAVE_FILTER();
}

