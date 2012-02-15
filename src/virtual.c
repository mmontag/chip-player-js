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
#include "mixer.h"

#define	FREE	-1
#define MAX_VOICES_CHANNEL 16


void virtch_resetvoice(struct context_data *ctx, int voc, int mute)
{
    struct player_data *p = &ctx->p;
    struct mixer_voice *vi = &p->virt.voice_array[voc];

    if ((uint32)voc >= p->virt.maxvoc)
	return;

    if (mute)
	mixer_setvol(ctx, voc, 0);

    p->virt.virt_used--;
    p->virt.virt_channel[vi->root].count--;
    p->virt.virt_channel[vi->chn].map = FREE;
    memset(vi, 0, sizeof (struct mixer_voice));
    vi->chn = vi->root = FREE;
}


/* virtch_on (number of tracks) */
int virtch_on(struct context_data *ctx, int num)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	int i;

	p->virt.num_tracks = num;
	num = mixer_numvoices(ctx, -1);

	p->virt.virt_channels = p->virt.num_tracks;
	p->virt.chnvoc = HAS_QUIRK(QUIRK_VIRTUAL) ? MAX_VOICES_CHANNEL : 1;

	if (p->virt.chnvoc > 1)
		p->virt.virt_channels += num;
	else if (num > p->virt.virt_channels)
		num = p->virt.virt_channels;

	num = p->virt.maxvoc = mixer_numvoices(ctx, num);

	p->virt.voice_array = calloc(p->virt.maxvoc, sizeof(struct mixer_voice));
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


void virtch_off(struct context_data *ctx)
{
    struct player_data *p = &ctx->p;

    if (p->virt.virt_channels < 1)
	return;

    p->virt.virt_used = p->virt.maxvoc = 0;
    p->virt.virt_channels = 0;
    p->virt.num_tracks = 0;
    free(p->virt.voice_array);
    free(p->virt.virt_channel);
}


void virtch_reset(struct context_data *ctx)
{
    struct player_data *p = &ctx->p;
    int i;

    if (p->virt.virt_channels < 1)
	return;

    mixer_numvoices(ctx, p->virt.maxvoc);

    memset(p->virt.voice_array, 0, p->virt.maxvoc * sizeof (struct mixer_voice));
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


static int alloc_voice(struct context_data *ctx, int chn)
{
    struct player_data *p = &ctx->p;
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
        struct mixer_voice *vi = &p->virt.voice_array[i];
	if (vi->root == chn && vi->age < age) {
	    num = i;
	    age = p->virt.voice_array[num].age;
	}
    }

    /* Free oldest voice */
    p->virt.virt_channel[p->virt.voice_array[num].chn].map = FREE;
    p->virt.voice_array[num].age = p->virt.age;

    return num;
}


void virtch_mute(struct context_data *ctx, int chn, int status)
{
    struct player_data *p = &ctx->p;

    if ((uint32)chn >= XMP_MAX_CHANNELS)
	return;

    if (status < 0)
	p->channel_mute[chn] = !p->channel_mute[chn];
    else
	p->channel_mute[chn] = status;
}

static inline int map_virt_channel(struct player_data *p, int chn)
{
	int voc;
	
	if ((uint32)chn >= p->virt.virt_channels)
		return - 1;

	voc = p->virt.virt_channel[chn].map;

	if ((uint32)voc >= p->virt.maxvoc)
		return -1;

	return voc;
}

void virtch_resetchannel(struct context_data *ctx, int chn)
{
    struct player_data *p = &ctx->p;
    int voc;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    mixer_setvol(ctx, voc, 0);

    p->virt.virt_used--;
    p->virt.virt_channel[p->virt.voice_array[voc].root].count--;
    p->virt.virt_channel[chn].map = FREE;
    memset(&p->virt.voice_array[voc], 0, sizeof (struct mixer_voice));
    p->virt.voice_array[voc].chn = p->virt.voice_array[voc].root = FREE;
}

void virtch_setvol(struct context_data *ctx, int chn, int vol)
{
    struct player_data *p = &ctx->p;
    int voc, root;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    root = p->virt.voice_array[voc].root;
    if (root < XMP_MAX_CHANNELS && p->channel_mute[root])
	vol = 0;

    mixer_setvol(ctx, voc, vol);

    if (!(vol || chn < p->virt.num_tracks))
	virtch_resetvoice(ctx, voc, 1);
}


void virtch_setpan(struct context_data *ctx, int chn, int pan)
{
    struct player_data *p = &ctx->p;
    int voc;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    mixer_setpan(ctx, voc, pan);
}


void virtch_seteffect(struct context_data *ctx, int chn, int type, int val)
{
    struct player_data *p = &ctx->p;
    int voc;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    mixer_seteffect(ctx, voc, type, val);
}


void virtch_setsmp(struct context_data *ctx, int chn, int smp)
{
    struct player_data *p = &ctx->p;
    struct mixer_voice *vi;
    int voc, pos, frac;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    vi = &p->virt.voice_array[voc];
    if (vi->smp == smp)
	return;

    pos = vi->pos;
    frac = vi->frac;

    mixer_setpatch(ctx, voc, smp);
    mixer_voicepos(ctx, voc, pos, frac);	/* Restore old position */
}


int virtch_setpatch(struct context_data *ctx, int chn, int ins, int smp, int note, int nna, int dct, int dca, int flg, int cont_sample)
{
    struct player_data *p = &ctx->p;
    int i, voc, vfree;

    if ((uint32)chn >= p->virt.virt_channels)
	return -1;

    if (ins < 0)
	smp = -1;

    voc = p->virt.virt_channel[chn].map;

    if (dct) {
	for (i = 0; i < p->virt.maxvoc; i++) {
            struct mixer_voice *vi = &p->virt.voice_array[i];
            if (vi->root == chn && vi->ins == ins) {
		int cond1 = (dct == XMP_INST_DCT_INST);
		int cond2 = (dct == XMP_INST_DCT_SMP && vi->smp == smp);
		int cond3 = (dct == XMP_INST_DCT_NOTE && vi->note == note);

                if (cond1 || cond2 || cond3) {
                    if (dca) {
                        if (i != voc || vi->act) {
                            vi->act = dca;
                        }
                    } else {
                        virtch_resetvoice(ctx, i, 1);
                    }
                }
	    }
        }
    }

    if (voc > FREE) {
	if (p->virt.voice_array[voc].act && p->virt.chnvoc > 1) {
	    vfree = alloc_voice(ctx, chn);
	    if (vfree > FREE) {
		p->virt.voice_array[vfree].root = chn;
		p->virt.virt_channel[chn].map = vfree;
		p->virt.voice_array[vfree].chn = chn;
		for (chn = p->virt.num_tracks; p->virt.virt_channel[chn].map > FREE; chn++);
		p->virt.voice_array[voc].chn = --chn;
		p->virt.virt_channel[chn].map = voc;
		voc = vfree;
	    } else if (flg) {
		return -1;
	    }
	}
    } else {
	voc = alloc_voice(ctx, chn);
	if (voc < 0)
	    return -1;
	p->virt.virt_channel[chn].map = voc;
	p->virt.voice_array[voc].chn = chn;
	p->virt.voice_array[voc].root = chn;
    }

    if (smp < 0) {
	virtch_resetvoice(ctx, voc, 1);
	return chn;	/* was -1 */
    }

    if (!cont_sample) {
	mixer_setpatch(ctx, voc, smp);
    }

    mixer_setnote(ctx, voc, note);
    p->virt.voice_array[voc].ins = ins;
    p->virt.voice_array[voc].act = nna;
    p->virt.age++;

    return chn;
}


void virtch_setnna(struct context_data *ctx, int chn, int nna)
{
    struct player_data *p = &ctx->p;
    int voc;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    p->virt.voice_array[voc].act = nna;
}


void virtch_setbend(struct context_data *ctx, int chn, int bend)
{
    struct player_data *p = &ctx->p;
    int voc;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    mixer_setbend(ctx, voc, bend);
}


void virtch_voicepos(struct context_data *ctx, int chn, int pos)
{
    struct player_data *p = &ctx->p;
    int voc;

    if ((voc = map_virt_channel(p, chn)) < 0)
        return;

    mixer_voicepos(ctx, voc, pos, 0);
}


void virtch_pastnote(struct context_data *ctx, int chn, int act)
{
    struct player_data *p = &ctx->p;
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


int virtch_cstat(struct context_data *ctx, int chn)
{
    struct player_data *p = &ctx->p;
    int voc;

    if ((voc = map_virt_channel(p, chn)) < 0)
	return VIRTCH_INVALID;

    return chn < p->virt.num_tracks ? VIRTCH_ACTIVE : p->virt.voice_array[voc].act;
}
