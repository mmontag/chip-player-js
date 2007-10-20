/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#define NOT_IMPLEMENTED

#define DO_TONEPORTA() { \
	if (note-- && note < 0x60 && (uint32)xc->ins < p->m.xxh->ins) \
	    xc->s_end = note_to_period (note + XXI[XXIM.ins[note]].xpo + \
		XXIM.xpo[note] /*, XXI[XXIM.ins[note]].fin*/, \
	 	p->m.xxh->flg & XXM_FLG_LINEAR); \
	xc->s_sgn = xc->period < xc->s_end ? 1 : -1; \
}


/* Values for multi-retrig */
static struct retrig_t rval[] = {
    {   0,  1,  1 }, {  -1,  1,  1 }, {  -2,  1,  1 }, {  -4,  1,  1 },
    {  -8,  1,  1 }, { -16,  1,  1 }, {   0,  2,  3 }, {   0,  1,  2 },
    {   0,  1,  1 }, {   1,  1,  1 }, {   2,  1,  1 }, {   4,  1,  1 },
    {   8,  1,  1 }, {  16,  1,  1 }, {   0,  3,  2 }, {   0,  2,  1 },
    {   0,  0,  1 }	/* Note cut */
};
    

void process_fx(struct xmp_context *ctx, int chn, uint8 note, uint8 fxt, uint8 fxp, struct xmp_channel *xc)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &p->m;
    int h, l;

    switch (fxt) {
    case FX_ARPEGGIO:
	if (!fxp)
	    break;
	xc->a_val[0] = 0;
	xc->a_val[1] = 100 * MSN(fxp);
	xc->a_val[2] = 100 * LSN(fxp);
	xc->a_size = 3;
	break;
    case FX_OKT_ARP3:
	if (!fxp)
	    break;
	xc->a_val[0] = -100 * MSN(fxp);
	xc->a_val[1] = 0;
	xc->a_val[2] = 100 * LSN(fxp);;
	xc->a_size = 3;
    case FX_OKT_ARP4:
	if (!fxp)
	    break;
	xc->a_val[0] = 0;
	xc->a_val[1] = 100 * LSN(fxp);;
	xc->a_val[2] = 0;
	xc->a_val[3] = -100 * MSN(fxp);
	xc->a_size = 4;
	break;
    case FX_OKT_ARP5:
	if (!fxp)
	    break;
	xc->a_val[0] = 100 * LSN(fxp);;
	xc->a_val[1] = 100 * LSN(fxp);;
	xc->a_val[2] = 0;
	xc->a_size = 3;
	break;
    case FX_PORTA_UP:				/* Portamento up */
fx_porta_up:
	if (m->fetch & XMP_CTL_FINEFX) {
	    switch (MSN(fxp)) {
	    case 0xf:
		xc->porta = fxp;
		fxp &= 0x0f;
		goto ex_f_porta_up;
	    case 0xe:
		xc->porta = fxp;
		fxp &= 0x0e;
		fxp |= 0x10;
		goto fx_xf_porta;
	    }
	    if (!fxp) {
		if ((fxp = xc->porta) != 0)
		    goto fx_porta_up;
	    }
	}
	SET(PITCHBEND);
	if ((xc->porta = fxp) != 0)
	    xc->f_val = -fxp;
	else if (xc->f_val > 0)
	    xc->f_val *= -1;
	break;
    case FX_PORTA_DN:				/* Portamento down */
fx_porta_dn:
	if (m->fetch & XMP_CTL_FINEFX) {
	    switch (MSN(fxp)) {
	    case 0xf:
		xc->porta = fxp;
		fxp &= 0x0f;
		goto ex_f_porta_dn;
	    case 0xe:
		xc->porta = fxp;
		fxp &= 0x0e;
		fxp |= 0x20;
		goto fx_xf_porta;
	    }
	    if (!fxp) {
		if ((fxp = xc->porta) != 0)
		    goto fx_porta_dn;
	    }
	}
	SET(PITCHBEND);
	if ((xc->porta = fxp) != 0)
	    xc->f_val = fxp;
	else if (xc->f_val < 0)
	    xc->f_val *= -1;
	break;
    case FX_TONEPORTA:				/* Tone portamento */
	if (!TEST (IS_VALID))
	    break;
	DO_TONEPORTA();
	if (fxp)
	    xc->s_val = fxp;
	SET(TONEPORTA);
	break;
    case FX_PER_PORTA_UP:			/* Persistent portamento up */
	SET_PER(PITCHBEND);
	if ((xc->porta = fxp) != 0)
	    xc->f_val = -fxp;
	else
	    RESET_PER(PITCHBEND);
	break;
    case FX_PER_PORTA_DN:			/* Persistent portamento down */
	SET(PITCHBEND);
	if ((xc->porta = fxp) != 0)
	    xc->f_val = fxp;
	else
	    RESET_PER(PITCHBEND);
	break;
    case FX_PER_TPORTA:				/* Persistent tone portamento */
	if (!TEST(IS_VALID))
	    break;
	SET_PER(TONEPORTA);
	DO_TONEPORTA();
	if (fxp)
	    xc->s_val = fxp;
	else
	    RESET_PER(TONEPORTA);
	break;
    case FX_VIBRATO:				/* Vibrato */
	SET(VIBRATO);
	if (LSN(fxp))
	    xc->y_depth = LSN(fxp) * 4;
	if (MSN(fxp))
	    xc->y_rate = MSN(fxp);
	break;
    case FX_FINE2_VIBRA:			/* Fine vibrato (2x) */
	SET(VIBRATO);
	if (LSN(fxp))
	    xc->y_depth = LSN(fxp) * 2;
	if (MSN(fxp))
	    xc->y_rate = MSN(fxp);
	break;
    case FX_FINE4_VIBRA:			/* Fine vibrato (4x) */
	SET(VIBRATO);
	if (LSN(fxp))
	    xc->y_depth = LSN(fxp);
	if (MSN(fxp))
	    xc->y_rate = MSN(fxp);
	break;
    case FX_PER_VIBRATO:			/* Persistent vibrato */
	SET_PER(VIBRATO);
	if (LSN(fxp))
	    xc->y_depth = LSN(fxp) * 4;
	else
	    RESET_PER(VIBRATO);
	if (MSN(fxp))
	    xc->y_rate = MSN(fxp);
	break;
    case FX_TONE_VSLIDE:			/* Toneporta + vol slide */
	if (!TEST (IS_VALID))
	    break;
	DO_TONEPORTA ();
	SET(TONEPORTA);
	goto fx_volslide;
    case FX_VIBRA_VSLIDE:			/* Vibrato + vol slide */
	SET(VIBRATO);
	goto fx_volslide;
    case FX_TREMOLO:				/* Tremolo */
	SET(TREMOLO);
	if (MSN(fxp))
	    xc->t_rate = MSN(fxp);
	if (LSN(fxp))
	    xc->t_depth = LSN(fxp);
	break;
    case FX_SETPAN:				/* Set pan */
	SET(NEW_PAN);
	xc->pan = fxp;
	break;
    case FX_OFFSET:				/* Set sample offset */
	SET(OFFSET);
	if (fxp)
	    xc->offset = fxp << 8;
	break;
    case FX_VOLSLIDE:				/* Volume slide */
fx_volslide:
	if (m->fetch & XMP_CTL_FINEFX) {
	    h = MSN(fxp);
	    l = LSN(fxp);
	    if (h == 0xf && l != 0) {
		xc->volslide = fxp;
		fxp &= 0x0f;
		goto ex_f_vslide_dn;
	    } else if (h == 0xe && l != 0) {
		xc->volslide = fxp;
		fxp &= 0x0f;
		goto ex_f_vslide_dn;		/* FIXME */
	    } else if (l == 0xf && h != 0) {
		xc->volslide = fxp;
		fxp >>= 4;
		goto ex_f_vslide_up;
	    } else if (l == 0xe && h != 0) {
		xc->volslide = fxp;
		fxp >>= 4;
		goto ex_f_vslide_up;		/* FIXME */
	    } else if (!fxp) {
		if ((fxp = xc->volslide) != 0)
		    goto fx_volslide;
	    }
	}
	SET(VOL_SLIDE);
	if ((xc->volslide = fxp))
	    xc->v_val = MSN(fxp) - LSN(fxp);
	break;
    case FX_VOLSLIDE_2:				/* Secondary volume slide */
	SET(VOL_SLIDE_2);
	if (fxp)
	    xc->v_val2 = MSN(fxp) - LSN(fxp);
	break;
    case FX_VOLSLIDE_UP:			/* Vol slide with uint8 arg */
	xc->v_val = fxp;
	SET(VOL_SLIDE);
	break;
    case FX_VOLSLIDE_DN:			/* Vol slide with uint8 arg */
	xc->v_val = -fxp;
	SET(VOL_SLIDE);
	break;
    case FX_F_VSLIDE:				/* Fine volume slide */
	SET(FINE_VOLS);
	if ((xc->fvolslide = fxp))
	    xc->v_fval = MSN(fxp) - LSN(fxp);
	break;
    case FX_JUMP:				/* Order jump */
	p->flow.pbreak = 1;
	p->flow.jump = fxp;
	break;
    case FX_VOLSET:				/* Volume set */
	RESET(RESET_VOL);
	SET(NEW_VOL);
	xc->volume = fxp;
	break;
    case FX_BREAK:				/* Pattern break */
	p->flow.pbreak = 1;
	p->flow.jumpline = 10 * MSN(fxp) + LSN(fxp);
	break;
    case FX_EXTENDED:				/* Extended effect */
	fxt = fxp >> 4;
	fxp &= 0x0f;
	switch (fxt) {
	case EX_F_PORTA_UP:			/* Fine portamento up */
ex_f_porta_up:
	    SET(FINE_BEND);
	    if (fxp)
		xc->f_fval = -fxp * 4;
	    else if (xc->f_val > 0)
		xc->f_val *= -1;
	    break;
	case EX_F_PORTA_DN:			/* Fine portamento down */
ex_f_porta_dn:
	    SET(FINE_BEND);
	    if (fxp)
		xc->f_fval = fxp * 4;
	    else if (xc->f_val < 0)
		xc->f_val *= -1;
	    break;
	case EX_GLISS:				/* Glissando toggle */
	    xc->gliss = fxp;
	    break;
	case EX_VIBRATO_WF:			/* Set vibrato waveform */
	    xc->y_type = fxp & 3;
	    break;
	case EX_FINETUNE:			/* Set finetune */
	    fxp <<= 4;
	    goto fx_finetune;
	case EX_PATTERN_LOOP:			/* Loop pattern */
	    if (fxp) {
		if (p->flow.loop_stack[chn]) {
		    if (--p->flow.loop_stack[chn])
			p->flow.loop_chn = ++chn;	/* **** H:FIXME **** */
		    else
			if (m->fetch & XMP_CTL_S3MLOOP)
			    p->flow.loop_row[chn] = p->flow.row_cnt + 1;
		} else {
		    if (p->flow.loop_row[chn] <= p->flow.row_cnt) {
			p->flow.loop_stack[chn] = fxp;
			p->flow.loop_chn = ++chn;
		    }
		}
	    } else {
		p->flow.loop_row[chn] = p->flow.row_cnt;
	    }
	    break;
	case EX_TREMOLO_WF:			/* Set tremolo waveform */
	    xc->t_type = fxp & 3;
	    break;
	case EX_RETRIG:				/* Retrig note */
	    xc->retrig = xc->rcount = fxp ? fxp : xc->rval;
	    xc->rval = xc->retrig;
	    xc->rtype = 0;
	    break;
	case EX_F_VSLIDE_UP:			/* Fine volume slide up */
ex_f_vslide_up:
	    SET(FINE_VOLS);
	    if (fxp)
		xc->v_fval = fxp;
	    break;
	case EX_F_VSLIDE_DN:			/* Fine volume slide down */
ex_f_vslide_dn:
	    SET(FINE_VOLS);
	    if (fxp)
		xc->v_fval = -fxp;
	    break;
	case EX_CUT:				/* Cut note */
	    xc->retrig = xc->rcount = fxp + 1;
	    xc->rtype = 0x10;
	    break;
	case EX_DELAY:				/* Note delay */
	    xc->delay = fxp + 1;
	    break;
	case EX_PATT_DELAY:			/* Pattern delay */
	    p->flow.delay = fxp;
	    break;
	}
	break;
    case FX_TEMPO:				/* Set tempo */
	if (fxp) {
	    if (fxp < 0x20)	/* speedup.xm needs BPM = 20 */
		p->tempo = fxp;
	    else
		p->tick_time = m->rrate / (p->xmp_bpm = fxp);
	}
	break;
    case FX_S3M_TEMPO:				/* Set S3M tempo */
	if (fxp)
	    p->tempo = fxp;
	break;
    case FX_S3M_BPM:				/* Set S3M BPM */
	if (fxp >= 0x20)	/* Panic uses 0x14 */
	    p->tick_time = m->rrate / (p->xmp_bpm = fxp);
	break;
    case FX_GLOBALVOL:				/* Set global volume */
	m->volume = fxp > m->volbase ? m->volbase : fxp;
	break;
    case FX_G_VOLSLIDE:				/* Global volume slide */
	p->gvol_flag = 1;
	if (m->fetch & XMP_CTL_FINEFX) {
	    h = MSN(fxp);
	    l = LSN(fxp);
	    if (h == 0xf && l != 0) {
		fxp = 0x01;			/* FIXME: file global vslide */
	    } else if (l == 0xf && h != 0) {
		fxp = 0x10;			/* FIXME: file global vslide */
	    }
	}
	if (fxp)
	    p->gvol_slide = MSN(fxp) - LSN(fxp);
	break;
    case FX_KEYOFF:				/* Key off */
	xc->keyoff = fxp;
	break;
    case FX_ENVPOS:				/* Set envelope position */
	NOT_IMPLEMENTED;
	break;
    case FX_MASTER_PAN:				/* Set master pan */
	xc->masterpan = fxp;
	break;
    case FX_PANSLIDE:				/* Pan slide */
	SET(PAN_SLIDE);
	if (fxp)
	    xc->p_val = MSN(fxp) - LSN(fxp);
	break;
    case FX_MULTI_RETRIG:			/* Multi retrig */
	if (LSN(fxp)) {
		xc->retrig = xc->rcount = LSN(fxp);
		xc->rval = xc->retrig;
	} else {
		xc->retrig = xc->rcount = xc->rval;
	}
	if (MSN(fxp)) {
		xc->rtype = MSN(fxp);
	}
	break;
    case FX_TREMOR:				/* Tremor */
	xc->tremor = fxp;
	xc->tcnt_up = MSN(fxp);
	xc->tcnt_dn = -1;
	break;
    case FX_XF_PORTA:				/* Extra fine portamento */
fx_xf_porta:
	SET(FINE_BEND);
	switch (MSN(fxp)) {
	case 1:
	    xc->f_fval = -LSN(fxp);
	    break;
	case 2:
	    xc->f_fval = LSN(fxp);
	    break;
	}
	break;
    case FX_TRK_VOL:				/* Track volume setting */
	if (fxp <= m->volbase)
	    xc->mastervol = fxp;
	break;
    case FX_TRK_VSLIDE:				/* Track volume slide */
fx_trk_vslide:
	if (m->fetch & XMP_CTL_FINEFX) {
	    h = MSN(fxp);
	    l = LSN(fxp);
	    if (h == 0xf && l != 0) {
		xc->trkvsld = fxp;
		fxp &= 0x0f;
		goto fx_trk_fvslide;
	    } else if (h == 0xe && l != 0) {
		xc->trkvsld = fxp;
		fxp &= 0x0f;
		goto fx_trk_fvslide;		/* FIXME */
	    } else if (l == 0xf && h != 0) {
		xc->trkvsld = fxp;
		fxp &= 0xf0;
		goto fx_trk_fvslide;
	    } else if (l == 0xe && h != 0) {
		xc->trkvsld = fxp;
		fxp &= 0xf0;
		goto fx_trk_fvslide;		/* FIXME */
	    } else if (!fxp) {
		if ((fxp = xc->trkvsld) != 0)
		    goto fx_trk_vslide;
	    }
	}
	SET(TRK_VSLIDE);
	if ((xc->trkvsld = fxp))
	    xc->trk_val = MSN(fxp) - LSN(fxp);
	break;
    case FX_TRK_FVSLIDE:			/* Track fine volume slide */
fx_trk_fvslide:
	SET(TRK_FVSLIDE);
	if (fxp)
	    xc->trk_fval = MSN(fxp) - LSN(fxp);
	break;
    case FX_FINETUNE:
fx_finetune:
	SET(FINETUNE);
	xc->finetune = (int16)(fxp - 0x80);
	break;
    case FX_IT_INSTFUNC:
	switch (fxp) {
	case 0:			/* Past note cut */
	    xmp_drv_pastnote(ctx, chn, XXM_NNA_CUT);
	    break;
	case 1:			/* Past note off */
	    xmp_drv_pastnote(ctx, chn, XXM_NNA_OFF);
	    break;
	case 2:			/* Past note fade */
	    xmp_drv_pastnote(ctx, chn, XXM_NNA_FADE);
	    break;
	case 3:			/* Set NNA to note cut */
	    xmp_drv_setnna(chn, XXM_NNA_CUT);
	    break;
	case 4:			/* Set NNA to continue */
	    xmp_drv_setnna(chn, XXM_NNA_CONT);
	    break;
	case 5:			/* Set NNA to note off */
	    xmp_drv_setnna(chn, XXM_NNA_OFF);
	    break;
	case 6:			/* Set NNA to note fade */
	    xmp_drv_setnna(chn, XXM_NNA_FADE);
	    break;
	case 7:			/* Turn off volume envelope */
	    break;
	case 8:			/* Turn on volume envelope */
	    break;
	}
	break;
    case FX_FLT_CUTOFF:
	xc->cutoff = fxp;
	break;
    case FX_FLT_RESN:
	xc->resonance = fxp;
	break;
    case FX_CHORUS:
	m->xxc[chn].cho = fxp;
	break;
    case FX_REVERB:
	m->xxc[chn].rvb = fxp;
	break;
    case FX_NSLIDE_R_DN:
	xc->retrig = xc->rcount = MSN(fxp) ? MSN(fxp) : xc->rval;
	xc->rval = xc->retrig;
	xc->rtype = 0;
	/* fall through */
    case FX_NSLIDE_DN:
	SET(NOTE_SLIDE);
	xc->ns_val = -LSN(fxp);
	xc->ns_count = xc->ns_speed = MSN(fxp);
	break;
    case FX_NSLIDE_R_UP:
	xc->retrig = xc->rcount = MSN(fxp) ? MSN(fxp) : xc->rval;
	xc->rval = xc->retrig;
	xc->rtype = 0;
	/* fall through */
    case FX_NSLIDE_UP:
	SET(NOTE_SLIDE);
	xc->ns_val = LSN(fxp);
	xc->ns_count = xc->ns_speed = MSN(fxp);
	break;
    case FX_NSLIDE2_DN:
	SET(NOTE_SLIDE);
	xc->ns_val = -fxp;
	xc->ns_count = xc->ns_speed = 1;
	break;
    case FX_NSLIDE2_UP:
	SET(NOTE_SLIDE);
	xc->ns_val = fxp;
	xc->ns_count = xc->ns_speed = 1;
	break;
    case FX_F_NSLIDE_DN:
	SET(FINE_NSLIDE);
	xc->ns_fval = -fxp;
	break;
    case FX_F_NSLIDE_UP:
	SET(FINE_NSLIDE);
	xc->ns_fval = fxp;
	break;
    }
}
