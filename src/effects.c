/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include "common.h"
#include "player.h"
#include "effects.h"
#include "period.h"
#include "virtual.h"
#include "mixer.h"

#define NOT_IMPLEMENTED

#define DO_TONEPORTA() do { \
	struct xmp_instrument *instrument = &m->mod.xxi[xc->ins]; \
	int mapped = instrument->map[xc->key].ins; \
	struct xmp_subinstrument *sub = &instrument->sub[mapped]; \
	if (note-- && note < 0x80 && (uint32)xc->ins < m->mod.ins) { \
	    xc->freq.s_end = note_to_period(note + sub->xpo + \
	    instrument->map[xc->key].xpo, sub->fin, HAS_QUIRK(QUIRK_LINEAR)); \
	} \
	xc->freq.s_sgn = xc->period < xc->freq.s_end ? 1 : -1; \
} while (0)

#define HAS_QUIRK(x) (m->quirk & (x))


static inline void set_lfo_notzero(struct lfo *lfo, int depth, int rate)
{
	if (depth != 0) {
		set_lfo_depth(lfo, depth);
	}

	if (rate != 0) {
		set_lfo_rate(lfo, rate);
	}
}

void process_fx(struct context_data *ctx, int chn, uint8 note, uint8 fxt,
		uint8 fxp, struct channel_data *xc, int fnum)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	int h, l;

	if (fxt >= FX_SYNTH_0 && fxt <= FX_SYNTH_F) {
		virt_seteffect(ctx, chn, fxt, fxp);
		return;
	}

	switch (fxt) {
	case FX_ARPEGGIO:
		if (fxp != 0) {
			set_stepper(&xc->arpeggio, 3,
				    0, 100 * MSN(fxp), 100 * LSN(fxp));
		}
		break;
	case FX_OKT_ARP3:
		if (fxp != 0) {
			set_stepper(&xc->arpeggio,
				    3, -100 * MSN(fxp), 0, 100 * LSN(fxp));
		}
		break;
	case FX_OKT_ARP4:
		if (fxp != 0) {
			set_stepper(&xc->arpeggio,
				    4, 0, 100 * LSN(fxp), 0, -100 * MSN(fxp));
		}
		break;
	case FX_OKT_ARP5:
		if (fxp != 0) {
			set_stepper(&xc->arpeggio,
				    3, 100 * LSN(fxp), 100 * LSN(fxp), 0);
		}
		break;

	case FX_PORTA_UP:	/* Portamento up */
		if (fxp == 0)
			fxp = xc->freq.porta;

		if (HAS_QUIRK(QUIRK_FINEFX)
		    && (fnum == 0 || !HAS_QUIRK(QUIRK_ITVPOR))) {
			switch (MSN(fxp)) {
			case 0xf:
				xc->freq.porta = fxp;
				fxp &= 0x0f;
				goto ex_f_porta_up;
			case 0xe:
				xc->freq.porta = fxp;
				fxp &= 0x0e;
				fxp |= 0x10;
				goto fx_xf_porta;
			}
		}
		SET(PITCHBEND);
		if ((xc->freq.porta = fxp) != 0) {
			xc->freq.slide = -fxp;
			if (HAS_QUIRK(QUIRK_UNISLD))
				xc->freq.s_val = -fxp;
		} else if (xc->freq.slide > 0) {
			xc->freq.slide *= -1;
		}
		break;
	case FX_PORTA_DN:	/* Portamento down */
		if (fxp == 0)
			fxp = xc->freq.porta;

		if (HAS_QUIRK(QUIRK_FINEFX)
		    && (fnum == 0 || !HAS_QUIRK(QUIRK_ITVPOR))) {
			switch (MSN(fxp)) {
			case 0xf:
				xc->freq.porta = fxp;
				fxp &= 0x0f;
				goto ex_f_porta_dn;
			case 0xe:
				xc->freq.porta = fxp;
				fxp &= 0x0e;
				fxp |= 0x20;
				goto fx_xf_porta;
			}
		}
		SET(PITCHBEND);
		if ((xc->freq.porta = fxp) != 0) {
			xc->freq.slide = fxp;
			if (HAS_QUIRK(QUIRK_UNISLD))
				xc->freq.s_val = fxp;
		} else if (xc->freq.slide < 0) {
			xc->freq.slide *= -1;
		}
		break;
	case FX_TONEPORTA:	/* Tone portamento */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		DO_TONEPORTA();
		if (fxp) {
			xc->freq.s_val = fxp;
			if (HAS_QUIRK(QUIRK_UNISLD)) /* IT compatible Gxx off */
				xc->freq.porta = fxp;
		}
		SET(TONEPORTA);
		break;
	case FX_PER_PORTA_UP:	/* Persistent portamento up */
		SET_PER(PITCHBEND);
		xc->freq.slide = -fxp;
		if ((xc->freq.porta = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_PER_PORTA_DN:	/* Persistent portamento down */
		SET(PITCHBEND);
		xc->freq.slide = fxp;
		if ((xc->freq.porta = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_PER_TPORTA:	/* Persistent tone portamento */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		SET_PER(TONEPORTA);
		DO_TONEPORTA();
		xc->freq.s_val = fxp;
		if (fxp == 0)
			RESET_PER(TONEPORTA);
		break;
	case FX_PER_VSLD_UP:	/* Persistent volslide up */
		SET_PER(VOL_SLIDE);
		xc->vol.slide = fxp;
		if (fxp == 0)
			RESET_PER(VOL_SLIDE);
		break;
	case FX_PER_VSLD_DN:	/* Persistent volslide down */
		SET_PER(VOL_SLIDE);
		xc->vol.slide = -fxp;
		if (fxp == 0)
			RESET_PER(VOL_SLIDE);
		break;
	case FX_SPEED_CP:	/* Set speed and ... */
		if (fxp)
			p->speed = fxp;
		/* fall through */
	case FX_PER_CANCEL:	/* Cancel persistent effects */
		xc->per_flags = 0;
		break;

	case FX_VIBRATO:	/* Vibrato */
		SET(VIBRATO);
		set_lfo_notzero(&xc->vibrato, LSN(fxp) * 4, MSN(fxp));
		break;
	case FX_FINE2_VIBRA:	/* Fine vibrato (2x) */
		SET(VIBRATO);
		set_lfo_notzero(&xc->vibrato, LSN(fxp) * 2, MSN(fxp));
		break;
	case FX_FINE4_VIBRA:	/* Fine vibrato (4x) */
		SET(VIBRATO);
		set_lfo_notzero(&xc->vibrato, LSN(fxp), MSN(fxp));
		break;
	case FX_PER_VIBRATO:	/* Persistent vibrato */
		if (LSN(fxp) != 0) {
			SET_PER(VIBRATO);
		} else {
			RESET_PER(VIBRATO);
		}
		set_lfo_notzero(&xc->vibrato, LSN(fxp) * 4, MSN(fxp));
		break;

	case FX_TONE_VSLIDE:	/* Toneporta + vol slide */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		DO_TONEPORTA();
		SET(TONEPORTA);
		goto fx_volslide;
	case FX_VIBRA_VSLIDE:	/* Vibrato + vol slide */
		SET(VIBRATO);
		goto fx_volslide;

	case FX_TREMOLO:	/* Tremolo */
		SET(TREMOLO);
		set_lfo_notzero(&xc->tremolo, LSN(fxp), MSN(fxp));
		break;

	case FX_SETPAN:		/* Set pan */
		xc->pan = fxp;
		break;
	case FX_OFFSET:		/* Set sample offset */
		SET(OFFSET);
		if (fxp) {
			xc->offset_val = xc->offset = fxp << 8;
		} else {
			xc->offset_val = xc->offset;
		}
		break;
	case FX_VOLSLIDE:	/* Volume slide */
	      fx_volslide:
		/* S3M file volume slide note:
		 * DFy  Fine volume down by y (...) If y is 0, the command will
		 *      be treated as a volume slide up with a value of f (15).
		 *      If a DFF command is specified, the volume will be slid
		 *      up.
		 */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (l == 0xf && h != 0) {
				xc->vol.memory = fxp;
				fxp >>= 4;
				goto ex_f_vslide_up;
			} else if (h == 0xf && l != 0) {
				xc->vol.memory = fxp;
				fxp &= 0x0f;
				goto ex_f_vslide_dn;
			} else if (!fxp) {
				if ((fxp = xc->vol.memory) != 0)
					goto fx_volslide;
			}
		}
		SET(VOL_SLIDE);

		/* Skaven's 2nd reality (S3M) has volslide parameter D7 => pri
		 * down. Stargazer's Red Dream (MOD) uses volslide DF =>
		 * compute both. Also don't assign xc->vol.memory if fxp is 0,
		 * see Guild of Sounds.xm
		 */
		if (fxp) {
			if (HAS_QUIRK(QUIRK_VOLPDN)) {
				if ((xc->vol.memory = fxp))
					xc->vol.slide =
					    LSN(fxp) ? -LSN(fxp) : MSN(fxp);
			} else {
				xc->vol.slide = -LSN(fxp) + MSN(fxp);
			}
		}

		/* Mirko reports that a S3M with D0F effects created with ST321
		 * should process volume slides in all frames like ST300. I
		 * suspect ST3/IT could be handling D0F effects like this.
		 */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			if (MSN(xc->vol.memory) == 0xf
			    || LSN(xc->vol.memory) == 0xf) {
				SET(FINE_VOLS);
				xc->vol.fslide = xc->vol.slide;
			}
		}
		break;
	case FX_VOLSLIDE_2:	/* Secondary volume slide */
		SET(VOL_SLIDE_2);
		if (fxp)
			xc->vol.slide2 = -LSN(fxp) + MSN(fxp);
		break;
	case FX_VOLSLIDE_UP:	/* Vol slide with uint8 arg */
		if (fxp)
			xc->vol.slide = fxp;
		SET(VOL_SLIDE);
		break;
	case FX_VOLSLIDE_DN:	/* Vol slide with uint8 arg */
		if (fxp)
			xc->vol.slide = -fxp;
		SET(VOL_SLIDE);
		break;
	case FX_F_VSLIDE:	/* Fine volume slide */
		SET(FINE_VOLS);
		if (fxp)
			xc->vol.fslide = MSN(fxp) - LSN(fxp);
		break;
	case FX_JUMP:		/* Order jump */
		p->flow.pbreak = 1;
		p->flow.jump = fxp;
		break;
	case FX_VOLSET:		/* Volume set */
		RESET(RESET_VOL);
		SET(NEW_VOL);
		xc->volume = fxp;
		break;
	case FX_BREAK:		/* Pattern break */
		p->flow.pbreak = 1;
		p->flow.jumpline = 10 * MSN(fxp) + LSN(fxp);
		break;
	case FX_EXTENDED:	/* Extended effect */
		fxt = fxp >> 4;
		fxp &= 0x0f;
		switch (fxt) {
		case EX_F_PORTA_UP:	/* Fine portamento up */
		      ex_f_porta_up:
			SET(FINE_BEND);
			if (fxp)
				xc->freq.fslide = -fxp * 4;
			else if (xc->freq.slide > 0)
				xc->freq.slide *= -1;
			break;
		case EX_F_PORTA_DN:	/* Fine portamento down */
		      ex_f_porta_dn:
			SET(FINE_BEND);
			if (fxp)
				xc->freq.fslide = fxp * 4;
			else if (xc->freq.slide < 0)
				xc->freq.slide *= -1;
			break;
		case EX_GLISS:		/* Glissando toggle */
			xc->gliss = fxp;
			break;
		case EX_VIBRATO_WF:	/* Set vibrato waveform */
			set_lfo_waveform(&xc->vibrato, fxp & 3);
			break;
		case EX_FINETUNE:	/* Set finetune */
			fxp <<= 4;
			goto fx_finetune;
		case EX_PATTERN_LOOP:	/* Loop pattern */
			if (fxp == 0) {
				/* mark start of loop */
				f->loop[chn].start = p->row;
			} else {
				/* end of loop */
				if (f->loop[chn].count) {
					if (--f->loop[chn].count) {
						/* **** H:FIXME **** */
						f->loop_chn = ++chn;
					} else {
						if (HAS_QUIRK(QUIRK_S3MLOOP))
							f->loop[chn].start =
							    p->row + 1;
					}
				} else {
					if (f->loop[chn].start <= p->row) {
						f->loop[chn].count = fxp;
						f->loop_chn = ++chn;
					}
				}
			}
			break;
		case EX_TREMOLO_WF:	/* Set tremolo waveform */
			set_lfo_waveform(&xc->tremolo, fxp & 3);
			break;
		case EX_RETRIG:		/* Retrig note */
			if (fxp > 0) {
				xc->retrig.delay = fxp;
			} else {
				xc->retrig.delay = xc->retrig.val;
			}
			xc->retrig.count = xc->retrig.delay;
			xc->retrig.val = xc->retrig.delay;
			xc->retrig.type = 0;
			break;
		case EX_F_VSLIDE_UP:	/* Fine volume slide up */
		      ex_f_vslide_up:
			SET(FINE_VOLS);
			if (fxp)
				xc->vol.fslide = fxp;
			break;
		case EX_F_VSLIDE_DN:	/* Fine volume slide down */
		      ex_f_vslide_dn:
			SET(FINE_VOLS);
			if (fxp)
				xc->vol.fslide = -fxp;
			break;
		case EX_CUT:		/* Cut note */
			xc->retrig.delay = fxp + 1;
			xc->retrig.count = xc->retrig.delay;
			xc->retrig.type = 0x10;
			break;

		case EX_DELAY:		/* Note delay */
			/* computed at frame loop */
			break;
		case EX_PATT_DELAY:	/* Pattern delay */
			p->flow.delay = fxp;
			break;
		case EX_INVLOOP:	/* Invert loop / funk repeat */
			xc->invloop.speed = fxp;
			break;
		}
		break;
	case FX_SPEED:		/* Set speed */
		if (fxp) {
			if (fxp < 0x20) {	/* speedup.xm needs BPM = 20 */
				p->speed = fxp;
			} else {
				p->bpm = fxp;
				p->frame_time =
				    m->time_factor * m->rrate / p->bpm;
			}
		}
		break;
	case FX_S3M_SPEED:	/* Set S3M speed */
		if (fxp)
			p->speed = fxp;
		break;
	case FX_S3M_BPM:	/* Set S3M BPM */
		if (fxp) {
			if (fxp < SMIX_MINBPM)
				fxp = SMIX_MINBPM;
			p->bpm = fxp;
			p->frame_time = m->time_factor * m->rrate / p->bpm;
		}
		break;
	case FX_IT_BPM:		/* Set IT BPM */
		if (MSN(fxp) == 0) {	/* T0x - Tempo slide down by x */
			p->bpm -= LSN(fxp);
			if (p->bpm < 0x20)
				p->bpm = 0x20;
		} else if (MSN(fxp) == 1) {	/* T1x - Tempo slide up by x */
			p->bpm += LSN(fxp);
			if ((int32) p->bpm > 0xff)
				p->bpm = 0xff;
		} else {
			if (fxp < SMIX_MINBPM)
				fxp = SMIX_MINBPM;
			p->bpm = fxp;
		}
		p->frame_time = m->time_factor * m->rrate / p->bpm;
		break;
	case FX_GLOBALVOL:	/* Set global volume */
		if (fxp > m->gvolbase) {
			p->gvol.volume = m->gvolbase;
		} else {
			p->gvol.volume = fxp;
		}
		break;
	case FX_G_VOLSLIDE:	/* Global volume slide */
		p->gvol.flag = 1;
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				fxp = 0x01;	/* FIXME: file global vslide */
			} else if (l == 0xf && h != 0) {
				fxp = 0x10;	/* FIXME: file global vslide */
			}
		}
		if (fxp) {
			p->gvol.slide = MSN(fxp) - LSN(fxp);
			xc->gvol.memory = p->gvol.slide;
		} else {
			p->gvol.slide = xc->gvol.memory;
		}
		break;
	case FX_KEYOFF:		/* Key off */
		xc->keyoff = fxp;
		break;
	case FX_ENVPOS:		/* Set envelope position */
		NOT_IMPLEMENTED;
		break;
	case FX_MASTER_PAN:	/* Set master pan */
		xc->masterpan = fxp;
		break;
	case FX_PANSLIDE:	/* Pan slide */
		SET(PAN_SLIDE);
		if (fxp) {
			xc->p_val = MSN(fxp) - LSN(fxp);
		}
		break;
	case FX_MULTI_RETRIG:	/* Multi retrig */
		if (LSN(fxp)) {
			xc->retrig.delay = LSN(fxp);
			xc->retrig.val = xc->retrig.delay;
		} else {
			xc->retrig.delay = xc->retrig.val;
		}
		xc->retrig.count = xc->retrig.delay;
		if (MSN(fxp)) {
			xc->retrig.type = MSN(fxp);
		}
		break;
	case FX_TREMOR:		/* Tremor */
		if (fxp != 0) {
			xc->tremor.val = fxp;
			xc->tremor.memory = xc->tremor.val;
		} else {
			xc->tremor.val = xc->tremor.memory;
		}
		if (MSN(xc->tremor.val) == 0)
			xc->tremor.val |= 0x10;
		if (LSN(xc->tremor.val) == 0)
			xc->tremor.val |= 0x01;
		break;
	case FX_XF_PORTA:	/* Extra fine portamento */
	      fx_xf_porta:
		SET(FINE_BEND);
		switch (MSN(fxp)) {
		case 1:
			xc->freq.fslide = -LSN(fxp);
			break;
		case 2:
			xc->freq.fslide = LSN(fxp);
			break;
		}
		break;
	case FX_TRK_VOL:	/* Track volume setting */
		if (fxp <= m->volbase) {
			xc->mastervol = fxp;
		}
		break;
	case FX_TRK_VSLIDE:	/* Track volume slide */
	      fx_trk_vslide:
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0x0f;
				goto fx_trk_fvslide;
			} else if (h == 0xe && l != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0x0f;
				goto fx_trk_fvslide;	/* FIXME */
			} else if (l == 0xf && h != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0xf0;
				goto fx_trk_fvslide;
			} else if (l == 0xe && h != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0xf0;
				goto fx_trk_fvslide;	/* FIXME */
			} else if (!fxp) {
				if ((fxp = xc->trackvol.memory) != 0)
					goto fx_trk_vslide;
			}
		}
		SET(TRK_VSLIDE);
		if ((xc->trackvol.memory = fxp))
			xc->trackvol.slide = MSN(fxp) - LSN(fxp);
		break;
	case FX_TRK_FVSLIDE:	/* Track fine volume slide */
	      fx_trk_fvslide:
		SET(TRK_FVSLIDE);
		if (fxp) {
			xc->trackvol.fslide = MSN(fxp) - LSN(fxp);
		}
		break;
	case FX_FINETUNE:
	      fx_finetune:
		xc->finetune = (int16) (fxp - 0x80);
		break;
	case FX_IT_INSTFUNC:
		switch (fxp) {
		case 0:	/* Past note cut */
			virt_pastnote(ctx, chn, XMP_INST_NNA_CUT);
			break;
		case 1:	/* Past note off */
			virt_pastnote(ctx, chn, XMP_INST_NNA_OFF);
			break;
		case 2:	/* Past note fade */
			virt_pastnote(ctx, chn, XMP_INST_NNA_FADE);
			break;
		case 3:	/* Set NNA to note cut */
			virt_setnna(ctx, chn, XMP_INST_NNA_CUT);
			break;
		case 4:	/* Set NNA to continue */
			virt_setnna(ctx, chn, XMP_INST_NNA_CONT);
			break;
		case 5:	/* Set NNA to note off */
			virt_setnna(ctx, chn, XMP_INST_NNA_OFF);
			break;
		case 6:	/* Set NNA to note fade */
			virt_setnna(ctx, chn, XMP_INST_NNA_FADE);
			break;
		case 7:	/* Turn off volume envelope */
			break;
		case 8:	/* Turn on volume envelope */
			break;
		}
		break;
	case FX_FLT_CUTOFF:
		xc->filter.cutoff = fxp;
		break;
	case FX_FLT_RESN:
		xc->filter.resonance = fxp;
		break;
	case FX_CHORUS:
#if 0
		/* FIXME */
		m->mod.xxc[chn].cho = fxp;
#endif
		break;
	case FX_REVERB:
#if 0
		/* FIXME */
		m->mod.xxc[chn].rvb = fxp;
#endif
		break;
	case FX_NSLIDE_R_DN:
		if (MSN(fxp)) {
			xc->retrig.delay = MSN(fxp);
		} else {
			xc->retrig.delay = xc->retrig.val;
		}
		xc->retrig.count = xc->retrig.delay;
		xc->retrig.val = xc->retrig.delay;
		xc->retrig.type = 0;
		/* fall through */
	case FX_NSLIDE_DN:
		SET(NOTE_SLIDE);
		xc->noteslide.slide = -LSN(fxp);
		xc->noteslide.count = xc->noteslide.speed = MSN(fxp);
		break;
	case FX_NSLIDE_R_UP:
		if (MSN(fxp)) {
			xc->retrig.delay = MSN(fxp);
		} else {
			xc->retrig.delay = xc->retrig.val;
		}
		xc->retrig.count = xc->retrig.delay;
		xc->retrig.val = xc->retrig.delay;
		xc->retrig.type = 0;
		/* fall through */
	case FX_NSLIDE_UP:
		SET(NOTE_SLIDE);
		xc->noteslide.slide = LSN(fxp);
		xc->noteslide.count = xc->noteslide.speed = MSN(fxp);
		break;
	case FX_NSLIDE2_DN:
		SET(NOTE_SLIDE);
		xc->noteslide.slide = -fxp;
		xc->noteslide.count = xc->noteslide.speed = 1;
		break;
	case FX_NSLIDE2_UP:
		SET(NOTE_SLIDE);
		xc->noteslide.slide = fxp;
		xc->noteslide.count = xc->noteslide.speed = 1;
		break;
	case FX_F_NSLIDE_DN:
		SET(FINE_NSLIDE);
		xc->noteslide.fslide = -fxp;
		break;
	case FX_F_NSLIDE_UP:
		SET(FINE_NSLIDE);
		xc->noteslide.fslide = fxp;
		break;
	}
}
