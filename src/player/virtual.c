/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See docs/COPYING
 * for more information.
 */

#include <stdlib.h>
#include <string.h>
#include "virtual.h"
#include "convert.h"
#include "mixer.h"
#include "smix.h"

#define	FREE	-1
#define MAX_VOICES_CHANNEL 16


void virtch_resetvoice(struct xmp_context *ctx, int voc, int mute)
{
    struct xmp_player_context *p = &ctx->p;
    struct voice_info *vi = &p->virt.voice_array[voc];

    if ((uint32)voc >= p->virt.maxvoc)
	return;

    if (mute)
	xmp_smix_setvol(ctx, voc, 0);

    p->virt.virt_used--;
    p->virt.virt_channel[vi->root].count--;
    p->virt.virt_channel[vi->chn].map = FREE;
    memset(vi, 0, sizeof (struct voice_info));
    vi->chn = vi->root = FREE;
}


/* virtch_on (number of tracks) */
int virtch_on(struct xmp_context *ctx, int num)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &ctx->m;
	int i;

	p->virt.num_tracks = num;
	num = xmp_smix_numvoices(ctx, -1);

	p->virt.virt_channels = p->virt.num_tracks;
	p->virt.chnvoc = m->flags & XMP_CTL_VIRTUAL ? MAX_VOICES_CHANNEL : 1;

	if (p->virt.chnvoc > 1)
		p->virt.virt_channels += num;
	else if (num > p->virt.virt_channels)
		num = p->virt.virt_channels;

	num = p->virt.maxvoc = xmp_smix_numvoices(ctx, num);

	p->virt.voice_array = calloc(p->virt.maxvoc, sizeof(struct voice_info));
	if (p->virt.voice_array == NULL)
		goto err;

	for (i = 0; i < p->virt.maxvoc; i++) {
		p->virt.voice_array[i].chn = FREE;
		p->virt.voice_array[i].root = FREE;
	}

	p->virt.virt_channel = malloc(p->virt.virt_channels * sizeof(struct virt_channel));
	if (p->virt.virt_channel == NULL)
		goto err1;

	for (i = 0; i < p->virt.virt_channels; i++) {
		 p->virt.virt_channel[i].map = FREE;
		 p->virt.virt_channel[i].count = 0;
	}

	p->virt.virt_used = p->virt.age = 0;

	return 0;

err1:
	free(p->virt.voice_array);
err:
	return -1;
}


void virtch_off(struct xmp_context *ctx)
{
    struct xmp_player_context *p = &ctx->p;

    if (p->virt.virt_channels < 1)
	return;

    p->virt.virt_used = p->virt.maxvoc = 0;
    p->virt.virt_channels = 0;
    p->virt.num_tracks = 0;
    free(p->virt.voice_array);
    free(p->virt.virt_channel);
}


void virtch_reset(struct xmp_context *ctx)
{
    struct xmp_player_context *p = &ctx->p;
    int i;

    if (p->virt.virt_channels < 1)
	return;

    xmp_smix_numvoices(ctx, p->virt.maxvoc);

    memset(p->virt.voice_array, 0, p->virt.maxvoc * sizeof (struct voice_info));
    for (i = 0; i < p->virt.maxvoc; i++) {
	p->virt.voice_array[i].chn = FREE;
	p->virt.voice_array[i].root = FREE;
    }

    for (i = 0; i < p->virt.virt_channels; i++) {
	p->virt.virt_channel[i].map = FREE;
	p->virt.virt_channel[i].count = 0;
    }

    p->virt.virt_used = p->virt.age = 0;
}


void virtch_resetchannel(struct xmp_context *ctx, int chn)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    xmp_smix_setvol(ctx, voc, 0);

    p->virt.virt_used--;
    p->virt.virt_channel[p->virt.voice_array[voc].root].count--;
    p->virt.virt_channel[chn].map = FREE;
    memset(&p->virt.voice_array[voc], 0, sizeof (struct voice_info));
    p->virt.voice_array[voc].chn = p->virt.voice_array[voc].root = FREE;
}


static int drv_allocvoice(struct xmp_context *ctx, int chn)
{
    struct xmp_player_context *p = &ctx->p;
    int i, num;
    uint32 age;

    if (p->virt.virt_channel[chn].count < p->virt.chnvoc) {

	/* Find free voice */
	for (i = 0; i < p->virt.maxvoc; i++) {
	    if (p->virt.voice_array[i].chn == FREE)
		break;
	}

	/* not found */
	if (i == p->virt.maxvoc)
	    return -1;

	p->virt.voice_array[i].age = p->virt.age;
	p->virt.virt_channel[chn].count++;
	p->virt.virt_used++;

	return i;
    }

    /* Find oldest voice */
    num = age = FREE;
    for (i = 0; i < p->virt.maxvoc; i++) {
	if (p->virt.voice_array[i].root == chn && p->virt.voice_array[i].age < age) {
	    num = i;
	    age = p->virt.voice_array[num].age;
	}
    }

    /* Free oldest voice */
    p->virt.virt_channel[p->virt.voice_array[num].chn].map = FREE;
    p->virt.voice_array[num].age = p->virt.age;

    return num;
}


void virtch_mute(struct xmp_context *ctx, int chn, int status)
{
    struct xmp_player_context *p = &ctx->p;

    if ((uint32)chn >= XMP_MAX_CHANNELS)
	return;

    if (status < 0)
	p->cmute_array[chn] = !p->cmute_array[chn];
    else
	p->cmute_array[chn] = status;
}


void virtch_setvol(struct xmp_context *ctx, int chn, int vol)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    if (p->virt.voice_array[voc].root < XMP_MAX_CHANNELS && p->cmute_array[p->virt.voice_array[voc].root])
	vol = 0;

    xmp_smix_setvol(ctx, voc, vol);

    if (!(vol || chn < p->virt.num_tracks))
	virtch_resetvoice(ctx, voc, 1);
}


void virtch_setpan(struct xmp_context *ctx, int chn, int pan)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    xmp_smix_setpan(ctx, voc, pan);
}


void virtch_seteffect(struct xmp_context *ctx, int chn, int type, int val)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    xmp_smix_seteffect(ctx, voc, type, val);
}


void virtch_setsmp(struct xmp_context *ctx, int chn, int smp)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;
    struct voice_info *vi;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    vi = &p->virt.voice_array[voc];
    if (vi->smp == smp)
	return;

    smix_setpatch(ctx, voc, smp);
    smix_voicepos(ctx, voc, vi->pos, vi->frac);
}


int virtch_setpatch(struct xmp_context *ctx, int chn, int ins, int smp, int note, int nna, int dct, int dca, int flg, int cont_sample)
{
    struct xmp_player_context *p = &ctx->p;
    int voc, vfree;

    if ((uint32)chn >= p->virt.virt_channels)
	return -1;

    if (ins < 0)
	smp = -1;

    if (dct) {
	for (voc = 0; voc < p->virt.maxvoc; voc++) {
	    if (p->virt.voice_array[voc].root == chn && p->virt.voice_array[voc].ins == ins) {
		if ((dct == XMP_INST_DCT_INST) ||
		    (dct == XMP_INST_DCT_SMP && p->virt.voice_array[voc].smp == smp) ||
		    (dct == XMP_INST_DCT_NOTE && p->virt.voice_array[voc].note == note)) {
		    if (dca) {
			if (voc != p->virt.virt_channel[chn].map || p->virt.voice_array[voc].act)
			    p->virt.voice_array[voc].act = dca;
		    } else {
			virtch_resetvoice(ctx, voc, 1);
		    }
		}
	    }
	}
    }

    voc = p->virt.virt_channel[chn].map;

    if (voc > FREE) {
	if (p->virt.voice_array[voc].act && p->virt.chnvoc > 1) {
	    if ((vfree = drv_allocvoice(ctx, chn)) > FREE) {
		p->virt.voice_array[vfree].root = chn;
		p->virt.virt_channel[chn].map = vfree;
		p->virt.voice_array[p->virt.virt_channel[chn].map].chn = chn;
		for (chn = p->virt.num_tracks; p->virt.virt_channel[chn++].map > FREE; );
		p->virt.voice_array[voc].chn = --chn;
		p->virt.virt_channel[chn].map = voc;
		voc = vfree;
	    } else if (flg) {
		return -1;
	    }
	}
    } else {
	if ((voc = drv_allocvoice(ctx, chn)) < 0)
	    return -1;
	p->virt.virt_channel[chn].map = voc;
	p->virt.voice_array[p->virt.virt_channel[chn].map].chn = chn;
	p->virt.voice_array[voc].root = chn;
    }

    if (smp < 0) {
	virtch_resetvoice(ctx, voc, 1);
	return chn;	/* was -1 */
    }

    if (!cont_sample)
	smix_setpatch(ctx, voc, smp);

    smix_setnote(ctx, voc, note);
    p->virt.voice_array[voc].ins = ins;
    p->virt.voice_array[voc].act = nna;
    p->virt.age++;

    return chn;
}


void virtch_setnna(struct xmp_context *ctx, int chn, int nna)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    p->virt.voice_array[voc].act = nna;
}


void virtch_setbend(struct xmp_context *ctx, int chn, int bend)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    smix_setbend(ctx, voc, bend);
}


void virtch_voicepos(struct xmp_context *ctx, int chn, int pos)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return;

    smix_voicepos(ctx, voc, pos, 0);
}


void virtch_pastnote(struct xmp_context *ctx, int chn, int act)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    for (voc = p->virt.maxvoc; voc--;) {
	if (p->virt.voice_array[voc].root == chn && p->virt.voice_array[voc].chn >= p->virt.num_tracks) {
	    if (act == VIRTCH_ACTION_CUT)
		virtch_resetvoice(ctx, voc, 1);
	    else
		p->virt.voice_array[voc].act = act;
	}
    }
}


int virtch_cstat(struct xmp_context *ctx, int chn)
{
    struct xmp_player_context *p = &ctx->p;
    int voc;

    voc = p->virt.virt_channel[chn].map;

    if ((uint32)chn >= p->virt.virt_channels || (uint32)voc >= p->virt.maxvoc)
	return VIRTCH_INVALID;

    return chn < p->virt.num_tracks ? VIRTCH_ACTIVE : p->virt.voice_array[voc].act;
}
