/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See docs/COPYING
 * for more information.
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
#include "synth.h"
#include "spectrum.h"
#include "smix.h"

#define	FREE	-1
#define MAX_VOICES_CHANNEL 16

static struct xmp_drv_info *drv_array = NULL;


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
		if ((ret = drv->init(ctx)) == 0)
		    break;
	    }
	}
    } else {
	ret = XMP_ERR_DSPEC;

	for (drv = drv_array; drv; drv = drv->next) {
	    if (o->verbosity > 2)
		report ("Probing %s... ", drv->description);
	    if (drv->init(ctx) == 0) {
		if (o->verbosity > 2)
		    report ("found\n");
		ret = 0;
		break;
	    }
	    if (o->verbosity > 2)
		report ("not found\n");
	}
    }
    if (ret)
	return ret;

    o->drv_id = drv->id;
    d->description = drv->description;
    d->help = drv->help;
    d->driver = drv;

    xmp_smix_on(ctx);

    return 0;
}


void xmp_drv_resetvoice(struct xmp_context *ctx, int voc, int mute)
{
    struct xmp_driver_context *d = &ctx->d;
    struct voice_info *vi = &d->voice_array[voc];

    if ((uint32)voc >= d->maxvoc)
	return;

    if (mute)
	xmp_smix_setvol(ctx, voc, 0);

    d->curvoc--;
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
    struct xmp_smixer_context *s = &ctx->s;
    /*struct xmp_options *o = &ctx->o;*/

    d->memavl = 0;
    s->buf32b = NULL;
    if ((status = drv_select(ctx)) != 0)
	return status;

    /*synth_init(o->freq);
    synth_reset();*/

    return 0;
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
	    return 0;
	}
    }

    return XMP_ERR_NODRV;
}


void xmp_drv_close(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    memset(d->cmute_array, 0, XMP_MAXCH * sizeof(int));
    d->driver->shutdown(ctx);
    xmp_smix_off(ctx);
    /*synth_deinit();*/
}


/* xmp_drv_on (number of tracks) */
int xmp_drv_on(struct xmp_context *ctx, int num)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_smixer_context *s = &ctx->s;
    struct xmp_mod_context *m = &p->m;
    struct xmp_options *o = &ctx->o;

    d->numtrk = num;
    num = xmp_smix_numvoices(ctx, -1);

    d->numchn = d->numtrk;
    d->chnvoc = m->flags & XMP_CTL_VIRTUAL ? MAX_VOICES_CHANNEL : 1;

    if (d->chnvoc > 1)
	d->numchn += num;
    else if (num > d->numchn)
	num = d->numchn;

    num = d->maxvoc = xmp_smix_numvoices(ctx, num);

    d->voice_array = calloc(d->maxvoc, sizeof (struct voice_info));
    d->ch2vo_array = calloc(d->numchn, sizeof (int));
    d->ch2vo_count = calloc(d->numchn, sizeof (int));

    if (!(d->voice_array && d->ch2vo_array && d->ch2vo_count))
	return XMP_ERR_ALLOC;

    for (; num--; d->voice_array[num].chn = d->voice_array[num].root = FREE);
    for (num = d->numchn; num--; d->ch2vo_array[num] = FREE);

    d->curvoc = d->agevoc = 0;

    s->mode = o->outfmt & XMP_FMT_MONO ? 1 : 2;
    s->resol = o->resol > 8 ? 2 : 1;
    smix_resetvar(ctx);

    return 0;
}


void xmp_drv_off(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    /*xmp_drv_writepatch(ctx, NULL);*/

    if (d->numchn < 1)
	return;

    d->curvoc = d->maxvoc = 0;
    d->numchn = 0;
    d->numtrk = 0;
    free(d->voice_array);
    free(d->ch2vo_array);
    free(d->ch2vo_count);
}


void xmp_drv_reset(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    int i;

    if (d->numchn < 1)
	return;

    xmp_smix_numvoices(ctx, d->maxvoc);

    memset(d->ch2vo_count, 0, d->numchn * sizeof (int));
    memset(d->voice_array, 0, d->maxvoc * sizeof (struct voice_info));

    for (i = 0; i < d->maxvoc; i++)
	 d->voice_array[i].chn = d->voice_array[i].root = FREE;

    for (i = 0; i < d->numchn; i++)
	d->ch2vo_array[i] = FREE;

    d->curvoc = d->agevoc = 0;
}


void xmp_drv_resetchannel(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    xmp_smix_setvol(ctx, voc, 0);

    d->curvoc--;
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

    if (d->ch2vo_count[chn] < d->chnvoc) {
	for (voc = d->maxvoc; voc-- && d->voice_array[voc].chn != FREE;);
	if (voc < 0)
	    return voc;

	d->voice_array[voc].age = d->agevoc;
	d->ch2vo_count[chn]++;
	d->curvoc++;

	return voc;
    }

    for (voc = d->maxvoc, vfree = age = FREE; voc--;) {
	if (d->voice_array[voc].root == chn && d->voice_array[voc].age < age)
	    age = d->voice_array[vfree = voc].age;
    }

    d->ch2vo_array[d->voice_array[vfree].chn] = FREE;
    d->voice_array[vfree].age = d->agevoc;

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

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    if (d->voice_array[voc].root < XMP_MAXCH && d->cmute_array[d->voice_array[voc].root])
	vol = 0;

    xmp_smix_setvol(ctx, voc, vol);

    if (!(vol || chn < d->numtrk))
	xmp_drv_resetvoice(ctx, voc, 1);
}


void xmp_drv_setpan(struct xmp_context *ctx, int chn, int pan)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    xmp_smix_setpan(ctx, voc, pan);
}


void xmp_drv_seteffect(struct xmp_context *ctx, int chn, int type, int val)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    xmp_smix_seteffect(ctx, voc, type, val);
}


void xmp_drv_setsmp(struct xmp_context *ctx, int chn, int smp)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc, pos, itp;
    struct voice_info *vi;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    vi = &d->voice_array[voc];
    if (vi->smp == smp)
	return;

    pos = vi->pos;
    itp = vi->itpt;

    smix_setpatch(ctx, voc, smp);
    smix_voicepos(ctx, voc, pos, itp);
}


int xmp_drv_setpatch(struct xmp_context *ctx, int chn, int ins, int smp, int note, int nna, int dct, int dca, int flg, int cont_sample)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc, vfree;

    if ((uint32)chn >= d->numchn)
	return -1;

    if (ins < 0)
	smp = -1;

    if (dct) {
	for (voc = d->maxvoc; voc--;) {
	    if (d->voice_array[voc].root == chn && d->voice_array[voc].ins == ins) {
		if ((dct == XXM_DCT_INST) ||
		    (dct == XXM_DCT_SMP && d->voice_array[voc].smp == smp) ||
		    (dct == XXM_DCT_NOTE && d->voice_array[voc].note == note)) {
		    if (dca) {
			if (voc != d->ch2vo_array[chn] || d->voice_array[voc].act)
			    d->voice_array[voc].act = dca;
		    } else {
			xmp_drv_resetvoice(ctx, voc, 1);
		    }
		}
	    }
	}
    }

    voc = d->ch2vo_array[chn];

    if (voc > FREE) {
	if (d->voice_array[voc].act && d->chnvoc > 1) {
	    if ((vfree = drv_allocvoice(ctx, chn)) > FREE) {
		d->voice_array[vfree].root = chn;
		d->voice_array[d->ch2vo_array[chn] = vfree].chn = chn;
		for (chn = d->numtrk; d->ch2vo_array[chn++] > FREE;);
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
	xmp_drv_resetvoice(ctx, voc, 1);
	return chn;	/* was -1 */
    }

    if (!cont_sample)
	smix_setpatch(ctx, voc, smp);
    smix_setnote(ctx, voc, note);
    d->voice_array[voc].ins = ins;
    d->voice_array[voc].act = nna;
    d->agevoc++;

    return chn;
}


void xmp_drv_setnna(struct xmp_context *ctx, int chn, int nna)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    d->voice_array[voc].act = nna;
}


void xmp_drv_setbend(struct xmp_context *ctx, int chn, int bend)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    smix_setbend(ctx, voc, bend);
}


void xmp_drv_retrig(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    smix_voicepos(ctx, voc, 0, 0);
}


void xmp_drv_pastnote(struct xmp_context *ctx, int chn, int act)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    for (voc = d->maxvoc; voc--;) {
	if (d->voice_array[voc].root == chn && d->voice_array[voc].chn >= d->numtrk) {
	    if (act == XMP_ACT_CUT)
		xmp_drv_resetvoice(ctx, voc, 1);
	    else
		d->voice_array[voc].act = act;
	}
    }
}


void xmp_drv_voicepos(struct xmp_context *ctx, int chn, int pos)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &p->m;
    struct xxm_sample *xxs;
    int voc;

    if ((uint32)chn >= d->numchn || (uint32) (voc = d->ch2vo_array[chn]) >= d->maxvoc)
	return;

    xxs = &m->xxs[d->voice_array[voc].smp];

    if (pos > xxs->len)	/* Attempt to set offset beyond the end of sample */
	return;

    smix_voicepos(ctx, voc, pos, 0);
}


int xmp_drv_cstat(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return XMP_CHN_DUMB;

    return chn < d->numtrk ? XMP_CHN_ACTIVE : d->voice_array[voc].act;
}


void xmp_drv_bufdump(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    int i = xmp_smix_softmixer(ctx);
    void *b =  xmp_smix_buffer(ctx);

    d->driver->bufdump(ctx, b, i);
}


void xmp_drv_starttimer(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    d->driver->starttimer();
}


void xmp_drv_stoptimer(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    for (voc = d->maxvoc; voc--; )
	xmp_smix_setvol(ctx, voc, 0);

    d->driver->stoptimer();

    xmp_drv_bufdump(ctx);
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


int xmp_drv_loadpatch(struct xmp_context *ctx, FILE *f, int id, int basefreq, int flags, struct xxm_sample *xxs, void *buffer)
{
    struct xmp_options *o = &ctx->o;
    uint8 s[5];
    int bytelen, extralen;

    /* Synth patches
     * Default is YM3128 for historical reasons
     */
    if (flags & XMP_SMP_SYNTH) {
	int size = 11;		/* Adlib instrument size */

	if (flags & XMP_SMP_SPECTRUM)
	    size = sizeof (struct spectrum_sample);

	if ((xxs->data = malloc(size)) == NULL)
	    return XMP_ERR_ALLOC;

	memcpy(xxs->data, buffer, size);

	xxs->flg |= XMP_SAMPLE_SYNTH;

	return 0;
    }

    if (o->skipsmp) {		/* Will fail for ADPCM samples */
        if (~flags & XMP_SMP_NOLOAD)
	    fseek(f, xxs->len, SEEK_CUR);
	return 0;
    }

    /* Empty samples
     */
    if (xxs->len == 0) {
	return 0;
    }

    /* Patches with samples
     */
    bytelen = xxs->len;
    extralen = 1;
    if (xxs->flg & XMP_SAMPLE_16BIT) {
	bytelen *= 2;
	extralen = 2;
    }

    if ((xxs->data = malloc(bytelen + extralen)) == NULL)
	return XMP_ERR_ALLOC;

    if (flags & XMP_SMP_NOLOAD) {
	memcpy(xxs->data, buffer, bytelen);
    } else {
	int pos = ftell(f);
	int num = fread(s, 1, 5, f);

	fseek(f, pos, SEEK_SET);

	if (num == 5 && !memcmp(s, "ADPCM", 5)) {
	    int x2 = bytelen >> 1;
	    char table[16];

	    fseek(f, 5, SEEK_CUR);	/* Skip "ADPCM" */
	    fread(table, 1, 16, f);
	    fread(xxs->data + x2, 1, x2, f);
	    adpcm4_decoder((uint8 *)xxs->data + x2, (uint8 *)xxs->data,
						table, bytelen);
	} else {
	    int x = fread(xxs->data, 1, bytelen, f);
	    if (x != bytelen)
		fprintf(stderr, "short read: %d\n", bytelen - x);
	}
    }

    /* Fix endianism if needed */
    if (xxs->flg & XMP_SAMPLE_16BIT) {
	if (!!o->big_endian ^ !!(flags & XMP_SMP_BIGEND))
	    xmp_cvt_sex(xxs->len, xxs->data);
    }

    /* Convert samples to signed */
    if (xxs->flg & XMP_SAMPLE_UNSIGNED) {
        xmp_cvt_sig2uns(xxs->len, xxs->flg & XMP_SAMPLE_16BIT, xxs->data);
    }

    /* Downmix stereo samples */
    if (flags & XMP_SMP_STEREO) {
	xmp_cvt_stdownmix(xxs->len, xxs->flg & XMP_SAMPLE_16BIT, xxs->data);
	xxs->len /= 2;
    }

    if (flags & XMP_SMP_7BIT)
	xmp_cvt_2xsmp(xxs->len, xxs->data);

    if (flags & XMP_SMP_DIFF)
	xmp_cvt_diff2abs(xxs->len, xxs->flg & XMP_SAMPLE_16BIT, xxs->data);
    else if (flags & XMP_SMP_8BDIFF)
	xmp_cvt_diff2abs(xxs->len, 0, xxs->data);

    if (flags & XMP_SMP_VIDC)
	xmp_cvt_vidc(xxs->len, xxs->data);

    /* Duplicate last sample -- prevent click in bidir loops */
    if (xxs->flg & XMP_SAMPLE_16BIT) {
	xxs->data[bytelen] = xxs->data[bytelen - 2];
	xxs->data[bytelen + 1] = xxs->data[bytelen - 1];
    } else {
	xxs->data[bytelen] = xxs->data[bytelen - 1];
    }
    //xxs->len++;

    return 0;
}


struct xmp_drv_info *xmp_drv_array()
{
    return drv_array;
}
