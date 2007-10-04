/* Extended Module Player
 * Copyright (C) 1997-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: mixer.c,v 1.11 2007-10-04 14:45:00 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mixer.h"
#include "synth.h"

#define FLAG_ITPT	0x01
#define FLAG_16_BITS	0x02
#define FLAG_STEREO	0x04
#define FLAG_FILTER	0x08
#define FLAG_BACKWARD	0x10
#define FLAG_ACTIVE	0x20
#define FLAG_SYNTH		0x40
#define FIDX_FLAGMASK	(FLAG_ITPT | FLAG_16_BITS | FLAG_STEREO | FLAG_FILTER)

#define LIM_FT		 12
#define LIM8_HI		 127
#define LIM8_LO		-127
#define LIM12_HI	 4095
#define LIM12_LO	-4096
#define LIM16_HI	 32767
#define LIM16_LO	-32768

#define TURN_OFF	0
#define TURN_ON		1

static char** smix_buffer = NULL;	/* array of output buffers */
static int* smix_buf32b = NULL;		/* temp buffer for 32 bit samples */
static int smix_numvoc;			/* default softmixer voices number */
static int smix_numbuf;			/* number of buffer to output */
static int smix_mode;			/* mode = 0:OFF, 1:MONO, 2:STEREO */
static int smix_resol;			/* resolution output 0:8bit, 1:16bit */
static int smix_ticksize;
int **xmp_mix_buffer = &smix_buf32b;

static int echo_msg;

void    smix_mn8norm      (struct voice_info *, int *, int, int, int, int);
void    smix_mn8itpt      (struct voice_info *, int *, int, int, int, int);
void    smix_mn16norm     (struct voice_info *, int *, int, int, int, int);
void    smix_mn16itpt     (struct voice_info *, int *, int, int, int, int);
void    smix_st8norm      (struct voice_info *, int *, int, int, int, int);
void    smix_st8itpt      (struct voice_info *, int *, int, int, int, int);
void    smix_st16norm     (struct voice_info *, int *, int, int, int, int);
void    smix_st16itpt	  (struct voice_info *, int *, int, int, int, int);

void    smix_mn8itpt_flt  (struct voice_info *, int *, int, int, int, int);
void    smix_mn16itpt_flt (struct voice_info *, int *, int, int, int, int);
void    smix_st8itpt_flt  (struct voice_info *, int *, int, int, int, int);
void    smix_st16itpt_flt (struct voice_info *, int *, int, int, int, int);

void    smix_synth	  (struct voice_info *, int *, int, int, int, int);

static void     out_su8norm     (char*, int*, int, int);
static void     out_su16norm    (int16*, int*, int, int);
static void     out_u8ulaw      (char*, int*, int, int);

/* Array index:
 *
 * bit 0: 0=non-interpolated, 1=interpolated
 * bit 1: 0=8 bit, 1=16 bit
 * bit 2: 0=mono, 1=stereo
 */

static void (*mix_fn[])() = {
    /* unfiltered */
    smix_mn8norm,
    smix_mn8itpt,
    smix_mn16norm,
    smix_mn16itpt,
    smix_st8norm,
    smix_st8itpt,
    smix_st16norm,
    smix_st16itpt,

    /* filtered */
    smix_mn8norm,
    smix_mn8itpt_flt,
    smix_mn16norm,
    smix_mn16itpt_flt,
    smix_st8norm,
    smix_st8itpt_flt,
    smix_st16norm,
    smix_st16itpt_flt,
};

#define OUT_U8ULAW	0
#define OUT_SU8NORM	1
#define OUT_SU16NORM	2

static void (*out_fn[])() = {
    out_u8ulaw,
    out_su8norm,
    out_su16norm
};

static int smix_p1, smix_p2;

#define ACLICK_0() do { \
    *dest = smp; \
} while (0)

#define ACLICK_1() do { \
    smp = (smp + smix_p1) / 2; \
    *dest = smp; \
    smix_p1 = smp; \
} while (0)

#define ACLICK_2() do { \
    smp = (smp + smix_p1 + smix_p2) / 3; \
    *dest = smp; \
    smix_p2 = smix_p1; \
    smix_p1 = smp; \
} while (0)

#define DOWNMIX_8(x) do { \
    for (; num--; ++src, ++dest) { \
	smp = *src >> (LIM_FT + 8); \
	smp = smp > LIM8_HI ? lhi : smp < LIM8_LO ? llo : smp + offs; \
	ACLICK_##x(); \
    } \
} while (0)

/* Downmix 32bit samples to 8bit, signed or unsigned, mono or stereo output */
static void out_su8norm(char *dest, int *src, int num, int cod)
{
    int smp, lhi, llo, offs;

    offs = (cod & XMP_FMT_UNS) ? 0x80 : 0;

    lhi = LIM8_HI + offs;
    llo = LIM8_LO + offs;

    switch (xmp_ctl->aclick) {
    case 0 : DOWNMIX_8(0); break;
    case 1 : DOWNMIX_8(1); break;
    default: DOWNMIX_8(2);
    }
}

#define DOWNMIX_16(x) do { \
    for (; num--; ++src, ++dest) { \
	smp = *src >> LIM_FT; \
	smp = smp > LIM16_HI ? lhi : smp < LIM16_LO ? llo : smp + offs; \
	ACLICK_##x(); \
    } \
} while (0)

/* Downmix 32bit samples to 16bit, signed or unsigned, mono or stereo output */
static void out_su16norm(int16 *dest, int *src, int num, int cod)
{
    int smp, lhi, llo, offs;

    offs = (cod & XMP_FMT_UNS) ? 0x8000 : 0;

    lhi = LIM16_HI + offs;
    llo = LIM16_LO + offs;

    switch (xmp_ctl->aclick) {
    case 0 : DOWNMIX_16(0); break;
    case 1 : DOWNMIX_16(1); break;
    default: DOWNMIX_16(2);
    }
}

#define DOWNMIX_ULAW(x) do { \
    for (; num--; ++src, ++dest) { \
	smp = *src >> (LIM_FT + 4); \
	smp = smp > LIM12_HI ? ulaw_encode(LIM12_HI) : \
	      smp < LIM12_LO ? ulaw_encode(LIM12_LO) : ulaw_encode (smp); \
	ACLICK_##x(); \
    } \
} while (0)

/* Downmix 32bit samples to 8bit, unsigned ulaw, mono or stereo output */
static void out_u8ulaw(char *dest, int *src, int num, int cod)
{
    int smp;

    switch (xmp_ctl->aclick) {
    case 0 : DOWNMIX_ULAW(0); break;
    case 1 : DOWNMIX_ULAW(1); break;
    default: DOWNMIX_ULAW(2);
    }
}


/* Prepare the mixer for the next tick */
inline static void smix_resetvar()
{
/*
    smix_ticksize = xmp_ctl->freq * xmp_ctl->rrate * 33 / xmp_bpm / 100;
    smix_ticksize /= xmp_ctl->fetch & XMP_CTL_MEDBPM ? 125 : 33;
*/
    smix_ticksize = xmp_ctl->fetch & XMP_CTL_MEDBPM ?
	xmp_ctl->freq * xmp_ctl->rrate * 33 / xmp_bpm / 12500 :
    	xmp_ctl->freq * xmp_ctl->rrate / xmp_bpm / 100;

    if (smix_buf32b)
	memset(smix_buf32b, 0, smix_ticksize * smix_mode * sizeof (int));
}


/* Fill the output buffer calling one of the handlers. The buffer contains
 * sound for one tick (a PAL frame or 1/50s for standard vblank-timed mods)
 */
static int softmixer()
{
    struct voice_info* vi;
    struct patch_info* pi;
    int smp_cnt, tic_cnt, lpsta, lpend;
    int vol_l, vol_r, itp_inc, voc;
    int synth = 1;
    int* buf_pos;

    for (voc = numvoc; voc--; ) {
	vi = &voice_array[voc];
	if (vi->chn < 0)
	    continue;
	if (vi->period < 1) {
	    drv_resetvoice (voc, 1);
	    continue;
	}
	buf_pos = smix_buf32b;
	vol_r = (vi->vol * (0x80 - vi->pan)) >> 4;
	vol_l = (vi->vol * (0x80 + vi->pan)) >> 4;

	if (vi->fidx & FLAG_SYNTH) {
	    if (synth) {
		smix_synth (vi, buf_pos, smix_ticksize, vol_l, vol_r,
			     vi->fidx & FLAG_STEREO);
	        synth = 0;
	    }
	    continue;
	}

	itp_inc = ((long long) vi->pbase << SMIX_SHIFT) / vi->period;

	pi = patch_array[vi->smp];

	/* This is for bidirectional sample loops */
	if (vi->fidx & FLAG_BACKWARD)
	    itp_inc = -itp_inc;

	/* Sample loop processing. Offsets in samples, not bytes */
	lpsta = pi->loop_start >> !!(vi->fidx & FLAG_16_BITS);
	lpend = pi->loop_end >> !!(vi->fidx & FLAG_16_BITS);

	for (tic_cnt = smix_ticksize; tic_cnt; ) {
	    /* How many samples we can write before the loop break or
	     * sample end... */
	    smp_cnt = 1 + (((long long) (vi->end - vi->pos) << SMIX_SHIFT)
		- vi->itpt) / itp_inc;

	    if (itp_inc > 0) {
		if (vi->end < vi->pos)
		    smp_cnt = 0;
	    } else
		if (vi->end > vi->pos)
		    smp_cnt = 0;

	    /* Ok, this shouldn't happen, but in 'Languede.mod'... */
	    if (smp_cnt < 0)
		smp_cnt = 0;
	    
	    /* ...inside the tick boundaries */
	    if (smp_cnt > tic_cnt)
		smp_cnt = tic_cnt;

	    if (vi->vol) {
		int mixer = vi->fidx & FIDX_FLAGMASK;

		/* "Beautiful Ones" apparently uses 0xfe as 'no filter' :\ */
		if (vi->cutoff >= 0xfe)
		    mixer &= ~FLAG_FILTER;

		/* Call the output handler */
		mix_fn[mixer](vi, buf_pos, smp_cnt, vol_l, vol_r, itp_inc);
		buf_pos += smix_mode * smp_cnt;
	    }

	    vi->itpt += itp_inc * smp_cnt;
	    vi->pos += vi->itpt >> SMIX_SHIFT;
	    vi->itpt &= SMIX_MASK;

	    /* No more samples in this tick */
	    if (!(tic_cnt -= smp_cnt))
		continue;

	    /* Single shot sample */
            if (!(vi->fidx ^= vi->fxor) || lpsta >= lpend) {
		drv_resetvoice(voc, 0);
		tic_cnt = 0;
		continue;
	    }

	    if (!(vi->fidx & FLAG_BACKWARD || vi->fxor)) {
		vi->pos -= lpend - lpsta;
	    } else {
		vi->itpt += (itp_inc = -itp_inc);
		vi->pos += vi->itpt >> SMIX_SHIFT;
		vi->itpt &= SMIX_MASK;
		vi->end = itp_inc > 0 ? lpend : lpsta;
	    }
	}
    }

    return smix_ticksize * smix_mode * smix_resol;
}


static void smix_voicepos(int voc, int pos, int itp)
{
    struct voice_info *vi = &voice_array[voc];
    struct patch_info *pi = patch_array[vi->smp];
    int lpend, res, mde;

    if (pi->len == XMP_PATCH_FM)
	return;

    res = !!(pi->mode & WAVE_16_BITS);
    mde = (pi->mode & WAVE_LOOPING) && !(pi->mode & WAVE_BIDIR_LOOP);
    mde = (mde << res) + res + 1;	/* see xmp_cvt_anticlick */

    lpend = pi->len - mde;
    if (pi->mode & WAVE_LOOPING)
	lpend =  lpend > pi->loop_end ? pi->loop_end : lpend;
    lpend >>= res;

    if (pos < lpend) {
	vi->pos = pos;
	vi->itpt = itp;
	vi->end = lpend;
	if (vi->fidx & FLAG_BACKWARD)
	    vi->fidx ^= vi->fxor;
    } else {
	drv_resetvoice(voc, TURN_ON);		/* Bad data, reset voice */
    }
}


static void smix_setpatch(int voc, int smp)
{
    struct voice_info *vi = &voice_array[voc];
    struct patch_info *pi = patch_array[smp];

    vi->smp = smp;
    vi->vol = 0;
    vi->freq = (long long) C4_FREQ * pi->base_freq / xmp_ctl->freq;
    
    if (pi->len == XMP_PATCH_FM) {
	vi->fidx = FLAG_SYNTH;
	if (xmp_ctl->outfmt & XMP_FMT_MONO)
	    vi->pan = TURN_OFF;
	else {
	    vi->pan = pi->panning;
	    vi->fidx |= FLAG_STEREO;
	}

	synth_setpatch(voc, (uint8 *)pi->data);

	return;
    }
    
    xmp_smix_setvol(voc, 0);

    vi->sptr = extern_drv ? NULL : pi->data;
    vi->fidx = xmp_ctl->fetch & XMP_CTL_ITPT ?
	FLAG_ITPT | FLAG_ACTIVE : FLAG_ACTIVE;

    if (xmp_ctl->outfmt & XMP_FMT_MONO) {
	vi->pan = TURN_OFF;
    } else {
	vi->pan = pi->panning;
	vi->fidx |= FLAG_STEREO;
    }

    if (pi->mode & WAVE_16_BITS)
	vi->fidx |= FLAG_16_BITS;

    if (xmp_ctl->fetch & XMP_CTL_FILTER)
	vi->fidx |= FLAG_FILTER;

    if (pi->mode & WAVE_LOOPING)
	vi->fxor = !!(pi->mode & WAVE_BIDIR_LOOP) * FLAG_BACKWARD;
    else
	vi->fxor = vi->fidx;

    smix_voicepos(voc, 0, 0);
}


static void smix_setnote(int voc, int note)
{
    struct voice_info *vi = &voice_array[voc];

    vi->period = note_to_period2 (vi->note = note, 0);
    vi->pbase = SMIX_C4NOTE * vi->freq / patch_array[vi->smp]->base_note;
}


static inline void smix_setbend(int voc, int bend)
{
    struct voice_info *vi = &voice_array[voc];

    vi->period = note_to_period2 (vi->note, bend);

    if (vi->fidx & FLAG_SYNTH)
	synth_setnote(voc, vi->note, bend);
}


void xmp_smix_setvol(int voc, int vol)
{
    struct voice_info *vi = &voice_array[voc];
 
    vi->vol = vol;
}


void xmp_smix_seteffect(int voc, int type, int val)
{
    switch (type) {
    case XMP_FX_CUTOFF:
        voice_array[voc].cutoff = val;
	break;
    case XMP_FX_RESONANCE:
        voice_array[voc].resonance = val;
	break;
    case XMP_FX_FILTER_B0:
        voice_array[voc].flt_B0 = val;
	break;
    case XMP_FX_FILTER_B1:
        voice_array[voc].flt_B1 = val;
	break;
    case XMP_FX_FILTER_B2:
        voice_array[voc].flt_B2 = val;
	break;
    }
}


void xmp_smix_setpan(int voc, int pan)
{
    voice_array[voc].pan = pan;
}


void xmp_smix_echoback(int msg)
{
    xmp_event_callback(echo_msg = msg);
}


int xmp_smix_getmsg()
{
    return echo_msg;
}


int xmp_smix_numvoices(int num)
{
    return num > smix_numvoc ? smix_numvoc : num;
}

/* WARNING! Output samples must have the same byte order of the host machine!
 * (That's what happens in most cases anyway)
 */
int xmp_smix_writepatch(struct patch_info *patch)
{
    if (patch) {
	if (patch->len == XMP_PATCH_FM)
	    return XMP_OK;

	if (patch->len <= 0)
	    return XMP_ERR_PATCH;

	if (patch->mode & WAVE_UNSIGNED)
	    xmp_cvt_sig2uns (patch->len, patch->mode & WAVE_16_BITS,
		patch->data);
    }
    return XMP_OK;
}


int xmp_smix_on(struct xmp_control *ctl)
{
    int cnt;

    if (smix_numbuf)
	return XMP_OK;

    if (ctl->numbuf < 1)
	ctl->numbuf = 1;
    cnt = smix_numbuf = ctl->numbuf;

    smix_buffer = calloc(sizeof (void *), cnt);
    smix_buf32b = calloc(sizeof (int), OUT_MAXLEN);
    if (!(smix_buffer && smix_buf32b))
	return XMP_ERR_ALLOC;

    smix_p1 = smix_p2 = 0;
    while (cnt--) {
	if (!(smix_buffer[cnt] = calloc (SMIX_RESMAX, OUT_MAXLEN)))
	    return XMP_ERR_ALLOC;
    }

    smix_numvoc = SMIX_NUMVOC;
    extern_drv = TURN_OFF;

    //synth_init (ctl->freq);

    return XMP_OK;
}


void xmp_smix_off()
{
    while (smix_numbuf)
	free(smix_buffer[--smix_numbuf]);

    //synth_deinit();

    free(smix_buf32b);
    free(smix_buffer);
    smix_buf32b = NULL;
    smix_buffer = NULL;
    extern_drv = TURN_ON;
}


void *xmp_smix_buffer()
{
    static int outbuf;
    int act;

    if (!xmp_ctl->resol)
	act = OUT_U8ULAW;
    else if (xmp_ctl->resol > 8)
	act = OUT_SU16NORM;
    else
	act = OUT_SU8NORM;

    /* The mixer works with multiple buffers -- this is useless when using
     * multi-buffered sound output (e.g. OSS fragments) but can be useful for
     * DMA transfers in DOS.
     */
    if (++outbuf >= smix_numbuf)
	outbuf = 0;

    out_fn[act](smix_buffer[outbuf], smix_buf32b, smix_mode * smix_ticksize,
							xmp_ctl->outfmt);

    smix_resetvar();

    return smix_buffer[outbuf]; 
}

