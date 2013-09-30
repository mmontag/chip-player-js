/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdlib.h>
#include "common.h"
#include "player.h"
#include "virtual.h"
#include "med_extras.h"

#ifdef __SUNPRO_C
#pragma error_messages (off,E_STATEMENT_NOT_REACHED)
#endif

/* Commands in the volume and waveform sequence table:
 *
 *	Cmd	Vol	Wave	Action
 *
 *	0xff	    END		End sequence
 *	0xfe	    JMP		Jump
 *	0xfd	 -	ARE	End arpeggio definition
 *	0xfc	 -	ARP	Begin arpeggio definition
 *	0xfb	    HLT		Halt
 *	0xfa	JWS	JVS	Jump waveform/volume sequence
 *	0xf9	     -		-
 *	0xf8	     -		-
 *	0xf7	 -	VWF	Set vibrato waveform
 *	0xf6	EST	RES	?/reset pitch
 *	0xf5	EN2	VBS	Looping envelope/set vibrato speed
 *	0xf4	EN1	VBD	One shot envelope/set vibrato depth
 *	0xf3	    CHU		Change volume/pitch up speed
 *	0xf2	    CHD		Change volume/pitch down speed
 *	0xf1	    WAI		Wait
 *	0xf0	    SPD		Set speed
 */

#define VT me->vol_table[xc->ins][ce->vp++]
#define WT me->wav_table[xc->ins][ce->wp++]
#define VT_SKIP ce->vp++
#define WT_SKIP ce->wp++


static const int sine[32] = {
	   0,  49,  97, 141, 180, 212, 235, 250,
	 255, 250, 235, 212, 180, 141,  97,  49,
	   0, -49, -97,-141,-180,-212,-235,-250,
	-255,-250,-235,-212,-180,-141, -97, -49
};

int med_change_period(struct context_data *ctx, struct channel_data *xc)
{
	struct med_channel_extras *ce = xc->extra;
	int vib;

	/* Vibrato */

#if 0
	if (ce->vib_wf >= xxi[xc->ins].nsm)	/* invalid waveform */
		return 0;

	if (xxs[xxi[xc->ins][ce->vib_wf].sid].len != 32)
		return 0;
#endif

	/* FIXME: always using sine waveform */

	vib = (sine[ce->vib_idx >> 5] * ce->vib_depth) >> 10;
	ce->vib_idx += ce->vib_speed;
	ce->vib_idx %= (32 << 5);

	return vib;
}


int med_linear_bend(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;
	struct med_module_extras *me = m->extra;
	struct med_channel_extras *ce = xc->extra;
	int arp;

	/* Arpeggio */

	if (ce->arp == 0)
		return 0;

	if (me->wav_table[xc->ins][ce->arp] == 0xfd) /* empty arpeggio */
		return 0;

	arp = me->wav_table[xc->ins][ce->aidx++];
	if (arp == 0xfd) {
		ce->aidx = ce->arp;
		arp = me->wav_table[xc->ins][ce->aidx++];
	}

	return (100 << 7) * arp;
}


void med_play_extras(struct context_data *ctx, struct channel_data *xc, int chn,
		     int new_note)
{
	struct module_data *m = &ctx->m;
	struct med_module_extras *me;
	struct med_channel_extras *ce;
	int b, jws = 0, jvs = 0, loop;
	int temp;

	if (!HAS_MED_MODULE_EXTRAS(*m))
		return;

	me = (struct med_module_extras *)m->extra;
	ce = (struct med_channel_extras *)xc->extra;

	if (me->vol_table[xc->ins] == NULL || me->wav_table[xc->ins] == NULL)
		return;

	if (new_note) {
		ce->arp = ce->aidx = 0;
		ce->period = xc->period;
		ce->vp = ce->vc = ce->vw = 0;
		ce->wp = ce->wc = ce->ww = 0;
		ce->vv = 0;
		ce->vs = MED_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->vts;
		ce->ws = MED_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->wts;
	}

	if (ce->vs > 0 && ce->vc-- == 0) {
		ce->vc = ce->vs - 1;

		if (ce->vw > 0) {
			ce->vw--;
			goto skip_vol;
		}

		loop = jws = 0;

	    next_vt:
		switch (b = VT) {
		case 0xff:	/* END */
		case 0xfb:	/* HLT */
			ce->vp--;
			break;
		case 0xfe:	/* JMP */
			if (loop)	/* avoid infinite loop */
				break;
			temp = VT;
			ce->vp = temp;
			loop = 1;
			goto next_vt;
			break;
		case 0xfa:	/* JWS */
			jws = VT;
			break;
		case 0xf5:	/* EN2 */
		case 0xf4:	/* EN1 */
			VT_SKIP;	/* Not implemented */
			break;
		case 0xf3:	/* CHU */
			ce->vv = VT;
			break;
		case 0xf2:	/* CHD */
			ce->vv = -VT;
			break;
		case 0xf1:	/* WAI */
			ce->vw = VT;
			break;
		case 0xf0:	/* SPD */
			ce->vs = VT;
			break;
		default:
			if (b >= 0x00 && b <= 0x40)
				ce->volume = b;
		}

		ce->volume += ce->vv;
		CLAMP(ce->volume, 0, 64);

	    skip_vol:

		if (ce->ww > 0) {
			ce->ww--;
			goto skip_wav;
		}

		loop = jvs = 0;

	    next_wt:
		switch (b = WT) {
			struct xmp_instrument *xxi;

		case 0xff:	/* END */
		case 0xfb:	/* HLT */
			ce->wp--;
			break;
		case 0xfe:	/* JMP */
			if (loop)	/* avoid infinite loop */
				break;
			temp = WT;
			if (temp == 0xff) {	/* handle JMP END case */
				ce->wp--;	/* see lepeltheme ins 0x02 */
				break;
			}
			ce->wp = temp;
			loop = 1;
			goto next_wt;
		case 0xfd:	/* ARE */
			break;
		case 0xfc:	/* ARP */
			ce->arp = ce->aidx = ce->wp++;
			while (WT != 0xfd) ;
			break;
		case 0xfa:	/* JVS */
			jws = WT;
			break;
		case 0xf7:	/* VWF */
			ce->vwf = WT;
			break;
		case 0xf6:	/* RES */
			xc->period = ce->period;
			break;
		case 0xf5:	/* VBS */
			ce->vib_speed = WT;
			break;
		case 0xf4:	/* VBD */
			ce->vib_depth = WT;
			break;
		case 0xf3:	/* CHU */
			ce->wv = -WT;
			break;
		case 0xf2:	/* CHD */
			ce->wv = WT;
			break;
		case 0xf1:	/* WAI */
			ce->ww = WT;
			break;
		case 0xf0:	/* SPD */
			ce->ws = WT;
			break;
		default:
			xxi = &m->mod.xxi[xc->ins];
			if (b < xxi->nsm && xxi->sub[b].sid != xc->smp) {
				xc->smp = xxi->sub[b].sid;
				virt_setsmp(ctx, chn, xc->smp);
			}
		}
	    skip_wav:
		;
		/* xc->period += ce->wv; */
	}

	if (jws) {
		ce->wp = jws;
		jws = 0;
	}

	if (jvs) {
		ce->vp = jvs;
		jvs = 0;
	}
}

int med_new_instrument_extras(struct xmp_instrument *xxi)
{
	xxi->extra = calloc(1, sizeof(struct med_instrument_extras));
	if (xxi->extra == NULL)
		return -1;
	MED_INSTRUMENT_EXTRAS((*xxi))->magic = MED_EXTRAS_MAGIC;

	return 0;
}

int med_new_channel_extras(struct channel_data *xc)
{
	xc->extra = calloc(1, sizeof(struct med_channel_extras));
	if (xc->extra == NULL)
		return -1;
	MED_CHANNEL_EXTRAS((*xc))->magic = MED_EXTRAS_MAGIC;

	return 0;
}

void med_reset_channel_extras(struct channel_data *xc)
{
	memset((char *)xc->extra + 4, 0, sizeof(struct med_channel_extras) - 4);
}

void med_release_channel_extras(struct channel_data *xc)
{
	free(xc->extra);
}

int med_new_module_extras(struct module_data *m)
{
	struct med_module_extras *me;
	struct xmp_module *mod = &m->mod;

	m->extra = calloc(1, sizeof(struct med_module_extras));
	if (m->extra == NULL)
		return -1;
	MED_MODULE_EXTRAS((*m))->magic = MED_EXTRAS_MAGIC;

	me = (struct med_module_extras *)m->extra;

        me->vol_table = calloc(sizeof(uint8 *), mod->ins);
	if (me->vol_table == NULL)
		return -1;
        me->wav_table = calloc(sizeof(uint8 *), mod->ins);
	if (me->wav_table == NULL)
		return -1;

	return 0;
}

void med_release_module_extras(struct module_data *m)
{
	struct med_module_extras *me;
	struct xmp_module *mod = &m->mod;
	int i;

	me = (struct med_module_extras *)m->extra;

        if (me->vol_table) {
		for (i = 0; i < mod->ins; i++)
			free(me->vol_table[i]);
		free(me->vol_table);
	}

	if (me->wav_table) {
		for (i = 0; i < mod->ins; i++)
			free(me->wav_table[i]);
		free(me->wav_table);
        }

	free(m->extra);
}

