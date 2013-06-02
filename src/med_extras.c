/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

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

#define VT m->med_vol_table[xc->ins][xc->extra.med.vp++]
#define WT m->med_wav_table[xc->ins][xc->extra.med.wp++]
#define VT_SKIP xc->extra.med.vp++
#define WT_SKIP xc->extra.med.wp++


static const int sine[32] = {
       0,  49,  97, 141, 180, 212, 235, 250,
     255, 250, 235, 212, 180, 141,  97,  49,
       0, -49, -97,-141,-180,-212,-235,-250,
    -255,-250,-235,-212,-180,-141, -97, -49
};

int med_get_vibrato(struct channel_data *xc)
{
	int vib;

#if 0
	if (xc->extra.med.vib_wf >= xxi[xc->ins].nsm)	/* invalid waveform */
		return 0;

	if (xxs[xxi[xc->ins][xc->extra.med.vib_wf].sid].len != 32)
		return 0;
#endif

	/* FIXME: always using sine waveform */

	vib = (sine[xc->extra.med.vib_idx >> 5] * xc->extra.med.vib_depth) >> 11;
	xc->extra.med.vib_idx += xc->extra.med.vib_speed;
	xc->extra.med.vib_idx %= (32 << 5);

	return vib;
}


int med_get_arp(struct module_data *m, struct channel_data *xc)
{
	int arp;

	if (xc->extra.med.arp == 0)
		return 0;

	if (m->med_wav_table[xc->ins][xc->extra.med.arp] == 0xfd) /* empty arpeggio */
		return 0;

	arp = m->med_wav_table[xc->ins][xc->extra.med.aidx++];
	if (arp == 0xfd) {
		xc->extra.med.aidx = xc->extra.med.arp;
		arp = m->med_wav_table[xc->ins][xc->extra.med.aidx++];
	}

	return 100 * arp;
}


void med_extras(struct context_data *ctx, int chn, struct channel_data *xc, int new_note)
{
    struct module_data *m = &ctx->m;
    int b, jws = 0, jvs = 0, loop = 0, jump = 0;
    int temp;

    if (m->med_vol_table == NULL || m->med_wav_table == NULL)
	return;

    if (m->med_vol_table[xc->ins] == NULL || m->med_wav_table[xc->ins] == NULL)
	return;

    if (new_note) {
	xc->extra.med.arp = xc->extra.med.aidx = 0;
	xc->extra.med.period = xc->period;
	xc->extra.med.vp = xc->extra.med.vc = xc->extra.med.vw = 0;
	xc->extra.med.wp = xc->extra.med.wc = xc->extra.med.ww = 0;
	xc->extra.med.vs = MED_EXTRA(m->mod.xxi[xc->ins])->vts;
	xc->extra.med.ws = MED_EXTRA(m->mod.xxi[xc->ins])->wts;
    }

    if (xc->extra.med.vs > 0 && xc->extra.med.vc-- == 0) {
	xc->extra.med.vc = xc->extra.med.vs - 1;

	if (xc->extra.med.vw > 0) {
	    xc->extra.med.vw--;
	    goto skip_vol;
	}

	jump = loop = jws = 0;
	switch (b = VT) {
	    while (jump--) {
	    case 0xff:		/* END */
	    case 0xfb:		/* HLT */
		xc->extra.med.vp--;
		break;
	    case 0xfe:		/* JMP */
		if (loop)	/* avoid infinite loop */
		    break;
		temp = VT;
		xc->extra.med.vp = temp;
		loop = jump = 1;
		break;
	    case 0xfa:		/* JWS */
		jws = VT;
		break;
	    case 0xf5:		/* EN2 */
	    case 0xf4:		/* EN1 */
		VT_SKIP;	/* Not implemented */
		break;
	    case 0xf3:		/* CHU */
		xc->extra.med.vv = VT;
		break;
	    case 0xf2:		/* CHD */
		xc->extra.med.vv = -VT;
		break;
	    case 0xf1:		/* WAI */
		xc->extra.med.vw = VT;
		break;
	    case 0xf0:		/* SPD */
		xc->extra.med.vs = VT;
		break;
	    default:
		if (b >= 0x00 && b <= 0x40)
		    xc->extra.med.volume = b;
	    }
	}

	xc->extra.med.volume += xc->extra.med.vv;
	CLAMP(xc->extra.med.volume, 0, 64);

skip_vol:

	if (xc->extra.med.ww > 0) {
	    xc->extra.med.ww--;
	    goto skip_wav;
	}

	jump = loop = jvs = 0;
	switch (b = WT) {
	    struct xmp_instrument *xxi;

	    while (jump--) {
	    case 0xff:		/* END */
	    case 0xfb:		/* HLT */
		xc->extra.med.wp--;
		break;
	    case 0xfe:		/* JMP */
		if (loop)	/* avoid infinite loop */
		    break;
		temp = WT;
		if (temp == 0xff) {	/* handle JMP END case */
		    xc->extra.med.wp--;	/* see lepeltheme ins 0x02 */
		    break;
		}
		xc->extra.med.wp = temp;
		loop = jump = 1;
		break;
	    case 0xfd:		/* ARE */
		break;
	    case 0xfc:		/* ARP */
		xc->extra.med.arp = xc->extra.med.aidx = xc->extra.med.wp++;
		while (WT != 0xfd);
		break;
	    case 0xfa:		/* JVS */
		jws = WT;
		break;
	    case 0xf7:		/* VWF */
		xc->extra.med.vwf = WT;
		break;
	    case 0xf6:		/* RES */
		xc->period = xc->extra.med.period;
		break;
	    case 0xf5:		/* VBS */
		xc->extra.med.vib_speed = WT;
		break;
	    case 0xf4:		/* VBD */
		xc->extra.med.vib_depth = WT;
		break;
	    case 0xf3:		/* CHU */
		xc->extra.med.wv = -WT;
		break;
	    case 0xf2:		/* CHD */
		xc->extra.med.wv = WT;
		break;
	    case 0xf1:		/* WAI */
		xc->extra.med.ww = WT;
		break;
	    case 0xf0:		/* SPD */
		xc->extra.med.ws = WT;
		break;
	    default:
		xxi = & m->mod.xxi[xc->ins];
		if (b < xxi->nsm && xxi->sub[b].sid != xc->smp) {
		    xc->smp = xxi->sub[b].sid;
		    virt_setsmp(ctx, chn, xc->smp);
		}
	    }
	}
skip_wav:
	;
	/* xc->period += xc->extra.med.wv; */
    }

    if (jws) {
	xc->extra.med.wp = jws;
	jws = 0;
    }

    if (jvs) {
	xc->extra.med.vp = jvs;
	jvs = 0;
    }
}
