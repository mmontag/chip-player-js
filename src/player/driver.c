/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See docs/COPYING
 * for more information.
 *
 * $Id: driver.c,v 1.63 2007-10-28 11:20:22 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "driver.h"
#include "convert.h"
#include "mixer.h"
#include "period.h"
#include "readlzw.h"

#define	FREE	-1

static struct xmp_drv_info *drv_array = NULL;

static int numvoc;		/* number of soundcard voices */
static int numchn;		/* number of channels need to play one module */
static int numtrk;		/* number of tracks in module */
static int maxvoc;		/* */
static int agevoc;		/* */
static int extern_drv;		/* set output to driver other than mixer */

static inline void drv_resetvoice (struct xmp_context *, int, int);


extern struct xmp_drv_info drv_file;
extern struct xmp_drv_info drv_wav;

extern struct xmp_drv_info drv_osx;
extern struct xmp_drv_info drv_solaris;
extern struct xmp_drv_info drv_hpux;
extern struct xmp_drv_info drv_bsd;
extern struct xmp_drv_info drv_netbsd;
extern struct xmp_drv_info drv_openbsd;
extern struct xmp_drv_info drv_sgi;
extern struct xmp_drv_info drv_aix;
extern struct xmp_drv_info drv_oss_seq;
extern struct xmp_drv_info drv_oss_mix;
extern struct xmp_drv_info drv_alsa_mix;
extern struct xmp_drv_info drv_alsa05;
extern struct xmp_drv_info drv_net;
extern struct xmp_drv_info drv_esd;
extern struct xmp_drv_info drv_arts;
extern struct xmp_drv_info drv_nas;
extern struct xmp_drv_info drv_os2dart;
extern struct xmp_drv_info drv_qnx;
extern struct xmp_drv_info drv_beos;


void (*xmp_event_callback) (unsigned long);

#include "../player/mixer.c"


void xmp_init_drivers()
{
#ifndef ENABLE_PLUGIN

    /* Output to file will be always available -- except in the plugin ;) */
    xmp_drv_register(&drv_file);
    xmp_drv_register(&drv_wav);

#ifdef DRIVER_OSX
    xmp_drv_register(&drv_osx);
#endif
#ifdef DRIVER_WIN32
    xmp_drv_register(&drv_win32);
#endif
#ifdef DRIVER_SOLARIS
    xmp_drv_register(&drv_solaris);
#endif
#ifdef DRIVER_HPUX
    xmp_drv_register(&drv_hpux);
#endif
#ifdef DRIVER_BSD
    xmp_drv_register(&drv_bsd);
#endif
#ifdef DRIVER_NETBSD
    xmp_drv_register(&drv_netbsd);
#endif
#ifdef DRIVER_OPENBSD
    xmp_drv_register(&drv_openbsd);
#endif
#ifdef DRIVER_SGI
    xmp_drv_register(&drv_sgi);
#endif
#ifdef DRIVER_OSS_SEQ
    xmp_drv_register(&drv_oss_seq);
#endif
#ifdef DRIVER_OSS_MIX
    xmp_drv_register(&drv_oss_mix);
#endif
#ifdef DRIVER_ALSA_MIX
    xmp_drv_register(&drv_alsa_mix);
#endif
#ifdef DRIVER_ALSA05
    xmp_drv_register(&drv_alsa05);
#endif
#ifdef DRIVER_QNX
    xmp_drv_register(&drv_qnx);
#endif
#ifdef DRIVER_BEOS
    xmp_drv_register(&drv_beos);
#endif
#ifdef DRIVER_NET
    xmp_drv_register(&drv_net);
#endif
#ifdef DRIVER_OS2DART
    xmp_drv_register(&drv_os2dart);
#endif
#ifdef DRIVER_ARTS
    xmp_drv_register(&drv_arts);
#endif
#ifdef DRIVER_ESD
    xmp_drv_register(&drv_esd);
#endif
#ifdef DRIVER_NAS
    xmp_drv_register(&drv_nas);
#endif

#endif	/* ENABLE_PLUGIN */
}


static int drv_select(struct xmp_context *ctx)
{
    struct xmp_drv_info *drv;
    int ret;
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_options *o = &ctx->o;

    if (!drv_array)
	return XMP_ERR_NODRV;

    if (o->drv_id) {
 	ret = XMP_ERR_NODRV;
	for (drv = drv_array; drv; drv = drv->next) {
	    if (!strcmp (drv->id, o->drv_id)) {
		if ((ret = drv->init(ctx)) == XMP_OK)
		    break;
	    }
	}
    } else {
	ret = XMP_ERR_DSPEC;
	drv = drv_array->next;	/* skip file */
	drv = drv->next;	/* skip wav */
	for (; drv; drv = drv->next) {
	    if (o->verbosity > 2)
		report ("Probing %s... ", drv->description);
	    if (drv->init(ctx) == XMP_OK) {
		if (o->verbosity > 2)
		    report ("found\n");
		ret = XMP_OK;
		break;
	    }
	    if (o->verbosity > 2)
		report ("not found\n");
	}
    }
    if (ret != XMP_OK)
	return ret;

    o->drv_id = drv->id;
    d->description = drv->description;
    d->help = drv->help;
    d->driver = drv;

    return XMP_OK;
}


static void drv_resetvoice(struct xmp_context *ctx, int voc, int mute)
{
    struct xmp_driver_context *d = &ctx->d;
    struct voice_info *vi = &d->voice_array[voc];

    if ((uint32) voc >= numvoc)
	return;

    if (mute)
	d->driver->setvol(ctx, voc, 0);

    d->numvoc--;
    d->ch2vo_count[vi->root]--;
    d->ch2vo_array[vi->chn] = FREE;
    memset(vi, 0, sizeof (struct voice_info));
    vi->chn = vi->root = FREE;
}


void xmp_drv_register(struct xmp_drv_info *drv)
{
    if (!drv_array) {
	drv_array = drv;
    } else {
	struct xmp_drv_info *tmp;

	for (tmp = drv_array; tmp->next; tmp = tmp->next);
	tmp->next = drv;
    }
    drv->next = NULL;
}


int xmp_drv_open(struct xmp_context *ctx)
{
    int status;
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_options *o = &ctx->o;

    d->memavl = 0;
    smix_buf32b = NULL;
    extern_drv = TURN_ON;
    if ((status = drv_select(ctx)) != XMP_OK)
	return status;

    d->patch_array = calloc(XMP_MAXPAT, sizeof(struct patch_info *));

    if (d->patch_array == NULL) {
	d->driver->shutdown ();
	return XMP_ERR_ALLOC;
    }

    synth_init(o->freq);
    synth_reset();

    return XMP_OK;
}


/* for the XMMS/BMP/Audacious plugin */
int xmp_drv_set(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_options *o = &ctx->o;
    struct xmp_drv_info *drv;

    if (!drv_array)
	return XMP_ERR_NODRV;

    for (drv = drv_array; drv; drv = drv->next) {
	if (!strcmp (drv->id, o->drv_id)) {
	    d->driver = drv;
	    return XMP_OK;
	}
    }

    return XMP_ERR_NODRV;
}


void xmp_drv_close(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    xmp_drv_off(ctx);
    memset(d->cmute_array, 0, XMP_MAXCH * sizeof(int));
    free(d->patch_array);
    d->driver->shutdown();
    synth_deinit();
}


/* xmp_drv_on (number of tracks) */
int xmp_drv_on(struct xmp_context *ctx, int num)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &p->m;
    struct xmp_options *o = &ctx->o;

    numtrk = d->numtrk = num;
    num = d->driver->numvoices(135711);
    d->driver->reset();

    numchn = numtrk;
    maxvoc = m->fetch & XMP_CTL_VIRTUAL ? o->maxvoc : 1;

    if (maxvoc > 1)
	numchn += num;
    else if (num > numchn)
	num = numchn;

    num = numvoc = d->driver->numvoices(num);

    d->voice_array = calloc(numvoc, sizeof (struct voice_info));
    d->ch2vo_array = calloc(numchn, sizeof (int));
    d->ch2vo_count = calloc(numchn, sizeof (int));

    if (!(d->voice_array && d->ch2vo_array && d->ch2vo_count))
	return XMP_ERR_ALLOC;

    for (; num--; d->voice_array[num].chn = d->voice_array[num].root = FREE);
    for (num = numchn; num--; d->ch2vo_array[num] = FREE);

    d->numvoc = agevoc = 0;
    d->numchn = numchn;

    smix_mode = o->outfmt & XMP_FMT_MONO ? 1 : 2;
    smix_resol = o->resol > 8 ? 2 : 1;
    smix_resetvar(ctx);

    return XMP_OK;
}


void xmp_drv_off(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    if (numchn < 1)
	return;

    xmp_drv_writepatch(ctx, NULL);
    d->numvoc = numvoc = 0;
    d->numchn = numchn = 0;
    numtrk = 0;
    free(d->voice_array);
    free(d->ch2vo_array);
    free(d->ch2vo_count);
}


/*inline*/ void xmp_drv_clearmem(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    if (d->driver)
	d->driver->clearmem();
}


void xmp_drv_reset(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    int num;

    if (numchn < 1)
	return;

    d->driver->numvoices(d->driver->numvoices (43210));
    d->driver->reset();
    d->driver->numvoices(num = numvoc);

    memset(d->ch2vo_count, 0, numchn * sizeof (int));
    memset(d->voice_array, 0, numvoc * sizeof (struct voice_info));

    for (; num--; d->voice_array[num].chn = d->voice_array[num].root = FREE);
    for (num = numchn; num--; d->ch2vo_array[num] = FREE);

    d->numvoc = agevoc = 0;
}


void xmp_drv_resetchannel(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    d->driver->setvol(ctx, voc, 0);

    d->numvoc--;
    d->ch2vo_count[d->voice_array[voc].root]--;
    d->ch2vo_array[chn] = FREE;
    memset(&d->voice_array[voc], 0, sizeof (struct voice_info));
    d->voice_array[voc].chn = d->voice_array[voc].root = FREE;
}


static int drv_allocvoice(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc, vfree;
    uint32 age;

    if (d->ch2vo_count[chn] < maxvoc) {
	for (voc = numvoc; voc-- && d->voice_array[voc].chn != FREE;);
	if (voc < 0)
	    return voc;

	d->voice_array[voc].age = agevoc;
	d->ch2vo_count[chn]++;
	d->numvoc++;

	return voc;
    }

    for (voc = numvoc, vfree = age = FREE; voc--;) {
	if (d->voice_array[voc].root == chn && d->voice_array[voc].age < age)
	    age = d->voice_array[vfree = voc].age;
    }

    d->ch2vo_array[d->voice_array[vfree].chn] = FREE;
    d->voice_array[vfree].age = agevoc;

    return vfree;
}


void xmp_drv_mute(struct xmp_context *ctx, int chn, int status)
{
    struct xmp_driver_context *d = &ctx->d;

    if ((uint32)chn >= XMP_MAXCH)
	return;

    if (status < 0)
	d->cmute_array[chn] = !d->cmute_array[chn];
    else
	d->cmute_array[chn] = status;
}


void xmp_drv_setvol(struct xmp_context *ctx, int chn, int vol)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    if (d->voice_array[voc].root < XMP_MAXCH && d->cmute_array[d->voice_array[voc].root])
	vol = TURN_OFF;

    d->driver->setvol(ctx, voc, vol);

    if (!(vol || chn < numtrk))
	drv_resetvoice(ctx, voc, TURN_ON);
}


void xmp_drv_setpan(struct xmp_context *ctx, int chn, int pan)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    d->driver->setpan(ctx, voc, pan);
}


void xmp_drv_seteffect(struct xmp_context *ctx, int chn, int type, int val)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    d->driver->seteffect(ctx, voc, type, val);
}


void xmp_drv_setsmp(struct xmp_context *ctx, int chn, int smp)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc, pos, itp;
    struct voice_info *vi;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    vi = &d->voice_array[voc];
    if ((uint32)smp >= XMP_MAXPAT || !d->patch_array[smp] || vi->smp == smp)
	return;

    pos = vi->pos;
    itp = vi->itpt;

    smix_setpatch(ctx, voc, smp);
    smix_voicepos(ctx, voc, pos, itp);

    if (extern_drv) {
	d->driver->setpatch(voc, smp);
	d->driver->setnote(voc, vi->note);
	d->driver->voicepos(voc,
			pos << !!(d->patch_array[smp]->mode & WAVE_16_BITS));
    }
}


int xmp_drv_setpatch(struct xmp_context *ctx, int chn, int ins, int smp, int note, int nna, int dct, int dca, int flg, int cont_sample)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc, vfree;

    if ((uint32) chn >= numchn)
	return -1;
    if (ins < 0 || (uint32) smp >= XMP_MAXPAT || !d->patch_array[smp])
	smp = -1;

    if (dct) {
	for (voc = numvoc; voc--;) {
	    if (d->voice_array[voc].root == chn && d->voice_array[voc].ins == ins) {
		if ((dct == XXM_DCT_INST) ||
		    (dct == XXM_DCT_SMP && d->voice_array[voc].smp == smp) ||
		    (dct == XXM_DCT_NOTE && d->voice_array[voc].note == note)) {
		    if (dca) {
			if (voc != d->ch2vo_array[chn] || d->voice_array[voc].act)
			    d->voice_array[voc].act = dca;
		    } else {
			drv_resetvoice(ctx, voc, TURN_ON);
		    }
		}
	    }
	}
    }

    voc = d->ch2vo_array[chn];

    if (voc > FREE) {
	if (d->voice_array[voc].act && maxvoc > 1) {
	    if ((vfree = drv_allocvoice(ctx, chn)) > FREE) {
		d->voice_array[vfree].root = chn;
		d->voice_array[d->ch2vo_array[chn] = vfree].chn = chn;
		for (chn = numtrk; d->ch2vo_array[chn++] > FREE;);
		d->voice_array[voc].chn = --chn;
		d->ch2vo_array[chn] = voc;
		voc = vfree;
	    } else if (flg) {
		return -1;
	    }
	}
    } else {
	if ((voc = drv_allocvoice(ctx, chn)) < 0)
	    return -1;
	d->voice_array[d->ch2vo_array[chn] = voc].chn = chn;
	d->voice_array[voc].root = chn;
    }

    if (smp < 0) {
	drv_resetvoice(ctx, voc, TURN_ON);
	return chn;	/* was -1 */
    }

    if (!cont_sample)
	smix_setpatch(ctx, voc, smp);
    smix_setnote(ctx, voc, note);
    d->voice_array[voc].ins = ins;
    d->voice_array[voc].act = nna;

    if (extern_drv) {
	if (!cont_sample)
	    d->driver->setpatch(voc, smp);
	d->driver->setnote(voc, note);
    }

    agevoc++;

    return chn;
}


void xmp_drv_setnna(struct xmp_context *ctx, int chn, int nna)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    d->voice_array[voc].act = nna;
}


void xmp_drv_setbend(struct xmp_context *ctx, int chn, int bend)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    smix_setbend(ctx, voc, bend);

    if (extern_drv)
	d->driver->setbend(voc, bend);
}


void xmp_drv_retrig(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= numchn || (uint32)voc >= numvoc)
	return;

    smix_voicepos(ctx, voc, 0, 0);

    if (extern_drv)
	d->driver->setnote(voc, d->voice_array[voc].note);
}


void xmp_drv_pastnote(struct xmp_context *ctx, int chn, int act)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    for (voc = numvoc; voc--;) {
	if (d->voice_array[voc].root == chn && d->voice_array[voc].chn >= numtrk) {
	    if (act == XMP_ACT_CUT)
		drv_resetvoice(ctx, voc, TURN_ON);
	    else
		d->voice_array[voc].act = act;
	}
    }
}


void xmp_drv_voicepos(struct xmp_context *ctx, int chn, int pos)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;
    struct patch_info *pi;

    if ((uint32) chn >= numchn || (uint32) (voc = d->ch2vo_array[chn]) >= numvoc)
	return;

    pi = d->patch_array[d->voice_array[voc].smp];

    if (pi->base_note != C4_FREQ)	/* process crunching samples */
	pos = ((long long)pos << SMIX_SHIFT) / (int)
	    (((long long)pi->base_note << SMIX_SHIFT) / C4_FREQ);

    if (pos > pi->len)	/* Attempt to set offset beyond the end of sample */
	return;

    smix_voicepos(ctx, voc, pos, 0);

    if (extern_drv)
	d->driver->voicepos(voc, pos << !!(pi->mode & WAVE_16_BITS));
}


int xmp_drv_cstat(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32) chn >= numchn || (uint32)voc >= numvoc)
	return XMP_CHN_DUMB;

    return chn < numtrk ? XMP_CHN_ACTIVE : d->voice_array[voc].act;
}


inline void xmp_drv_echoback(struct xmp_context *ctx, int msg)
{
    struct xmp_driver_context *d = &ctx->d;

    d->driver->echoback(msg);
}


inline int xmp_drv_getmsg(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    return d->driver->getmsg();
}


extern int **xmp_mix_buffer;
int hold_buffer[256];
int hold_enabled = 0;

inline void xmp_drv_bufdump(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    int i = softmixer(ctx);

    if (hold_enabled)
	memcpy(hold_buffer, *xmp_mix_buffer, 256 * sizeof (int));

    d->driver->bufdump(ctx, i);
}


inline void xmp_drv_starttimer(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    xmp_drv_sync(ctx, 0);
    d->driver->starttimer();
}


void xmp_drv_stoptimer(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    for (voc = numvoc; voc--; )
	d->driver->setvol(ctx, voc, 0);

    d->driver->stoptimer();

    xmp_drv_bufdump(ctx);
}


double xmp_drv_sync(struct xmp_context *ctx, double step)
{
    struct xmp_driver_context *d = &ctx->d;
    static double next_time = 0;

    if (step == 0)
	next_time = step;

    d->driver->sync(next_time += step);

    return next_time;
}


inline void xmp_drv_bufwipe(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    d->driver->bufwipe();
}


int xmp_drv_writepatch(struct xmp_context *ctx, struct patch_info *patch)
{
    struct xmp_driver_context *d = &ctx->d;
    int num;

    if (!d->patch_array)	/* FIXME -- this makes xmms happy */
    	return XMP_OK;

    if (!patch) {
	d->driver->writepatch(ctx, patch);

	for (num = XMP_MAXPAT; num--;) {
	    free(d->patch_array[num]);
	    d->patch_array[num] = NULL;
	}
	return XMP_OK;
    }
    if (patch->instr_no >= XMP_MAXPAT)
	return XMP_ERR_PATCH;
    d->patch_array[patch->instr_no] = patch;

    return XMP_OK;
}


int xmp_drv_flushpatch(struct xmp_context *ctx, int ratio)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_options *o = &ctx->o;
    struct patch_info *patch;
    int smp, num, crunch;

    if (!d->patch_array)		/* FIXME -- this makes xmms happy */
	return XMP_OK;

    if (!ratio)
	ratio = 0x10000;

    for (smp = XMP_MAXPAT, num = 0; smp--;)
	if (d->patch_array[smp])
	    num++;

    if (extern_drv) {
	reportv(ctx, 0, "Uploading smps : %d ", num);

	for (smp = XMP_MAXPAT; smp--;) {
	    if (!d->patch_array[smp])
		continue;
	    patch = d->patch_array[smp];

	    if (patch->len == XMP_PATCH_FM) {
		reportv(ctx, 0, "F");
		continue;
	    }

	    crunch = xmp_cvt_crunch(&patch, ratio);
	    xmp_cvt_anticlick (patch);
	    if ((num = d->driver->writepatch(ctx, patch)) != XMP_OK) {
		d->patch_array[smp] = NULL;	/* Bad type, reset array */
		free (patch);
	    } else 
		d->patch_array[smp] = realloc(patch, sizeof (struct patch_info));

	    if (o->verbosity) {
		if (num != XMP_OK)
		    report ("E");		/* Show type error */
		else if (!crunch)
		    report ("i");		/* Show sbi patch type */
		else report (crunch < 0x10000 ?
		    "c" : crunch > 0x10000 ? "x" : ".");
	    }
	}
	reportv(ctx, 0, "\n");
    } else {					/* Softmixer writepatch */
	for (smp = XMP_MAXPAT; smp--;) {
	    if (!d->patch_array[smp])
		continue;
	    patch = d->patch_array[smp];
	    xmp_cvt_anticlick (patch);
	    if (d->driver->writepatch(ctx, patch) != XMP_OK) {
		d->patch_array[smp] = NULL;	/* Bad type, reset array */
		free (patch);
	    }
	}
    }

    return XMP_OK;
}


static void adpcm4_decoder(uint8 *inp, uint8 *outp, char *tab, int len)
{
    char delta = 0;
    uint8 b0, b1;
    int i;

    len = (len + 1) / 2;

    for (i = 0; i < len; i++) {
	b0 = *inp;
	b1 = *inp++ >> 4;
        delta += tab[b0 & 0x0f];
	*outp++ = delta;
        delta += tab[b1 & 0x0f];
        *outp++ = delta;
    }
}


int xmp_drv_loadpatch(struct xmp_context *ctx, FILE *f, int id, int basefreq, int flags, struct xxm_sample *xxs, char *buffer)
{
    struct xmp_options *o = &ctx->o;
    struct patch_info *patch;
    int datasize;
    char s[5];

    /* FM patches
     */
    if (!xxs) {
	if ((patch = calloc(1, sizeof (struct patch_info) + 11)) == NULL)
	      return XMP_ERR_ALLOC;
	memcpy(patch->data, buffer, 11);
	patch->instr_no = id;
	patch->len = XMP_PATCH_FM;
	patch->base_note = 60;

	return xmp_drv_writepatch(ctx, patch);
    }

    if (o->skipsmp) {		/* Will fail for ADPCM samples */
        if (~flags & XMP_SMP_NOLOAD)
	    fseek(f, xxs->len, SEEK_CUR);
	return XMP_OK;
    }

    /* Empty samples
     */
    if (xxs->len < 4) {
	if (~flags & XMP_SMP_NOLOAD)
	    fread(s, 1, xxs->len, f);
	return XMP_OK;
    }
    /* Patches with samples
     */
    datasize = sizeof (struct patch_info) + xxs->len + sizeof (int);
    if ((patch = calloc(1, datasize)) == NULL)
	return XMP_ERR_ALLOC;

    if (flags & XMP_SMP_NOLOAD) {
	memcpy(patch->data, buffer, xxs->len);
    } else {
	int pos = ftell(f);
	fread(s, 1, 5, f);
	fseek(f, pos, SEEK_SET);

	if (!strncmp(s, "ADPCM", 5)) {
	    int x2 = xxs->len >> 1;
	    char table[16];

	    fseek(f, 5, SEEK_CUR);	/* Skip "ADPCM" */
	    fread(table, 1, 16, f);
	    fread(patch->data + x2, 1, x2, f);
	    adpcm4_decoder((uint8 *)patch->data + x2, (uint8 *)patch->data,
						table, xxs->len);
	} else {
	    fread(patch->data, 1, xxs->len, f);
	}
    }

    /* Fix endianism if needed */
    if (xxs->flg & WAVE_16_BITS) {
	if (o->big_endian ^ (flags & XMP_SMP_BIGEND))
	    xmp_cvt_sex(xxs->len, patch->data);
    }
    if (flags & XMP_SMP_7BIT)
	xmp_cvt_2xsmp(xxs->len, patch->data);
    if (flags & XMP_SMP_DIFF)
	xmp_cvt_diff2abs(xxs->len, xxs->flg & WAVE_16_BITS, patch->data);
    else if (flags & XMP_SMP_8BDIFF)
	xmp_cvt_diff2abs(xxs->len, 0, patch->data);
    if (flags & XMP_SMP_VIDC)
	xmp_cvt_vidc(xxs->len, patch->data);

    /* Duplicate last sample -- prevent click in bidir loops */
    if (xxs->flg & WAVE_16_BITS) {
	patch->data[xxs->len] = patch->data[xxs->len - 2];
	patch->data[xxs->len + 1] = patch->data[xxs->len - 1];
	xxs->len += 2;
    } else {
	patch->data[xxs->len] = patch->data[xxs->len - 1];
	xxs->len++;
    }

#if 0
    /* dump patch to file */
    if (id == 11) {
	printf("dump patch\n");
	FILE *f = fopen("patch_data", "w");
	fwrite(patch->data, 1, xxs->len, f);
	fclose(f);
    }
#endif

    patch->key = GUS_PATCH;
    patch->instr_no = id;
    patch->mode = xxs->flg;
    patch->mode |= (flags & XMP_SMP_UNS) ? WAVE_UNSIGNED : 0;
    patch->len = xxs->len;
    patch->loop_start = (xxs->lps > xxs->len) ? xxs->len : xxs->lps;
    patch->loop_end = (xxs->lpe > xxs->len) ? xxs->len : xxs->lpe;
    if (patch->loop_end <= patch->loop_start || !(patch->mode & WAVE_LOOPING))
	patch->mode &= ~(WAVE_LOOPING | WAVE_BIDIR_LOOP | WAVE_LOOP_BACK);

    patch->base_note = C4_FREQ;
    patch->base_freq = basefreq;
    patch->high_note = 0x7fffffff;
    patch->low_note = 0;
    patch->volume = 120;
    patch->detuning = 0;
    patch->panning = 0;

    xmp_cvt_crunch(&patch, flags & XMP_SMP_8X ? 0x80000 : 0x10000);

    return xmp_drv_writepatch(ctx, patch);
}


inline struct xmp_drv_info *xmp_drv_array()
{
    return drv_array;
}


void xmp_register_driver_callback(xmp_context ctx, void (*callback)(void *, int))
{
    ((struct xmp_context *)ctx)->d.callback = callback;
}

