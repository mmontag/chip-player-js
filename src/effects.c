/* Extended Module Player core player
 * Copyright (C) 1996-2014 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common.h"
#include "player.h"
#include "effects.h"
#include "period.h"
#include "virtual.h"
#include "mixer.h"
#ifndef LIBXMP_CORE_PLAYER
#include "extras.h"
#endif

#define NOT_IMPLEMENTED
#define HAS_QUIRK(x) (m->quirk & (x))

#define SET_LFO_NOTZERO(lfo, depth, rate) do { \
	if ((depth) != 0) set_lfo_depth(lfo, depth); \
	if ((rate) != 0) set_lfo_rate(lfo, rate); \
} while (0)


static void do_toneporta(struct module_data *m,
                                struct channel_data *xc, int note)
{
	struct xmp_instrument *instrument = &m->mod.xxi[xc->ins];
	int mapped = instrument->map[xc->key].ins;
	struct xmp_subinstrument *sub = &instrument->sub[mapped];

	if (note >= 1 && note <= 0x80 && (uint32)xc->ins < m->mod.ins) {
		note--;
		xc->porta.target = note_to_period(note + sub->xpo +
			instrument->map[xc->key].xpo, sub->fin,
			HAS_QUIRK(QUIRK_LINEAR), xc->per_adj);
	}
	xc->porta.dir = xc->period < xc->porta.target ? 1 : -1;
}


void process_fx(struct context_data *ctx, struct channel_data *xc, int chn,
		uint8 note, uint8 fxt, uint8 fxp, int fnum)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	int h, l;

	switch (fxt) {
	case FX_ARPEGGIO:
	      fx_arpeggio:
		if (fxp != 0) {
			xc->arpeggio.val[0] = 0;
			xc->arpeggio.val[1] = MSN(fxp);
			xc->arpeggio.val[2] = LSN(fxp);
			xc->arpeggio.size = 3;
		}
		break;
	case FX_S3M_ARPEGGIO:
		if (fxp == 0) {
			fxp = xc->arpeggio.memory;
		} else {
			xc->arpeggio.memory = fxp;
		}
		goto fx_arpeggio;
#ifndef LIBXMP_CORE_PLAYER
	case FX_OKT_ARP3:
		if (fxp != 0) {
			xc->arpeggio.val[0] = -MSN(fxp);
			xc->arpeggio.val[1] = 0;
			xc->arpeggio.val[2] = LSN(fxp);
			xc->arpeggio.size = 3;
		}
		break;
	case FX_OKT_ARP4:
		if (fxp != 0) {
			xc->arpeggio.val[0] = 0;
			xc->arpeggio.val[1] = LSN(fxp);
			xc->arpeggio.val[2] = 0;
			xc->arpeggio.val[3] = -MSN(fxp);
			xc->arpeggio.size = 4;
		}
		break;
	case FX_OKT_ARP5:
		if (fxp != 0) {
			xc->arpeggio.val[0] = LSN(fxp);
			xc->arpeggio.val[1] = LSN(fxp);
			xc->arpeggio.val[2] = 0;
			xc->arpeggio.size = 3;
		}
		break;
#endif
	case FX_PORTA_UP:	/* Portamento up */
		if (fxp == 0) {
			fxp = xc->freq.memory;
		} else {
			xc->freq.memory = fxp;
		}

		if (HAS_QUIRK(QUIRK_FINEFX)
		    && (fnum == 0 || !HAS_QUIRK(QUIRK_ITVPOR))) {
			switch (MSN(fxp)) {
			case 0xf:
				fxp &= 0x0f;
				goto fx_f_porta_up;
			case 0xe:
				fxp &= 0x0f;
				fxp |= 0x10;
				goto fx_xf_porta;
			}
		}

		SET(PITCHBEND);

		if (fxp != 0) {
			xc->freq.slide = -fxp;
			if (HAS_QUIRK(QUIRK_UNISLD))
				xc->porta.memory = fxp;
		} else if (xc->freq.slide > 0) {
			xc->freq.slide *= -1;
		}
		break;
	case FX_PORTA_DN:	/* Portamento down */
		if (fxp == 0) {
			fxp = xc->freq.memory;
		} else {
			xc->freq.memory = fxp;
		}

		if (HAS_QUIRK(QUIRK_FINEFX)
		    && (fnum == 0 || !HAS_QUIRK(QUIRK_ITVPOR))) {
			switch (MSN(fxp)) {
			case 0xf:
				fxp &= 0x0f;
				goto fx_f_porta_dn;
			case 0xe:
				fxp &= 0x0f;
				fxp |= 0x20;
				goto fx_xf_porta;
			}
		}

		SET(PITCHBEND);

		if (fxp != 0) {
			xc->freq.slide = fxp;
			if (HAS_QUIRK(QUIRK_UNISLD))
				xc->porta.memory = fxp;
		} else if (xc->freq.slide < 0) {
			xc->freq.slide *= -1;
		}
		break;
	case FX_TONEPORTA:	/* Tone portamento */
		if (HAS_QUIRK(QUIRK_IGSTPOR)) {
			if (note == 0 && xc->porta.dir == 0)
				break;
		}
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;

		do_toneporta(m, xc, note);

		if (fxp) {
			xc->porta.memory = fxp;
			if (HAS_QUIRK(QUIRK_UNISLD)) /* IT compatible Gxx off */
				xc->freq.memory = fxp;
		} else {
			fxp = xc->porta.memory;
		}

		if (fxp) {
			xc->porta.slide = fxp;
		}
		SET(TONEPORTA);
		break;

	case FX_VIBRATO:	/* Vibrato */
		SET(VIBRATO);
		SET_LFO_NOTZERO(&xc->vibrato, LSN(fxp) << 2, MSN(fxp));
		break;
	case FX_FINE_VIBRATO:	/* Fine vibrato (4x) */
		SET(VIBRATO);
		SET_LFO_NOTZERO(&xc->vibrato, LSN(fxp), MSN(fxp));
		break;

	case FX_TONE_VSLIDE:	/* Toneporta + vol slide */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		do_toneporta(m, xc, note);
		SET(TONEPORTA);
		goto fx_volslide;
	case FX_VIBRA_VSLIDE:	/* Vibrato + vol slide */
		SET(VIBRATO);
		goto fx_volslide;

	case FX_TREMOLO:	/* Tremolo */
		SET(TREMOLO);
		SET_LFO_NOTZERO(&xc->tremolo, LSN(fxp), MSN(fxp));
		break;

	case FX_SETPAN:		/* Set pan */
		xc->pan.val = fxp;
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
				goto fx_f_vslide_up;
			} else if (h == 0xf && l != 0) {
				xc->vol.memory = fxp;
				fxp &= 0x0f;
				goto fx_f_vslide_dn;
			} else if (fxp == 0x00) {
				if ((fxp = xc->vol.memory) != 0)
					goto fx_volslide;
			}
		}
		SET(VOL_SLIDE);

		/* Skaven's 2nd reality (S3M) has volslide parameter D7 => pri
		 * down. Other trackers only compute volumes if the other
		 * parameter is 0, Fall from sky.xm has 2C => do nothing.
		 * Also don't assign xc->vol.memory if fxp is 0, see Guild
		 * of Sounds.xm
		 */
		if (fxp) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (HAS_QUIRK(QUIRK_VOLPDN)) {
				if ((xc->vol.memory = fxp))
					xc->vol.slide = l ? -l : h;
			} else {
				if (l == 0)
					xc->vol.slide = h;
				else if (h == 0)
					xc->vol.slide = -l;
				else
					RESET(VOL_SLIDE);
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
		if (fxp) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (l == 0)
				xc->vol.slide2 = h;
			else if (h == 0)
				xc->vol.slide2 = -l;
			else
				RESET(VOL_SLIDE_2);
		}		
		break;
	case FX_JUMP:		/* Order jump */
		p->flow.pbreak = 1;
		p->flow.jump = fxp;
		break;
	case FX_VOLSET:		/* Volume set */
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
		    goto fx_f_porta_up;
		case EX_F_PORTA_DN:	/* Fine portamento down */
		    goto fx_f_porta_dn;
		case EX_GLISS:		/* Glissando toggle */
			xc->gliss = fxp;
			break;
		case EX_VIBRATO_WF:	/* Set vibrato waveform */
			fxp &= 3;
			if (HAS_QUIRK(QUIRK_S3MLFO) && fxp == 2)
				fxp |= 0x10;
			set_lfo_waveform(&xc->vibrato, fxp);
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
			SET(RETRIG);
			xc->retrig.val = fxp;
			xc->retrig.count = LSN(xc->retrig.val) + 1;
			xc->retrig.type = 0;
			break;
		case EX_F_VSLIDE_UP:	/* Fine volume slide up */
			goto fx_f_vslide_up;
		case EX_F_VSLIDE_DN:	/* Fine volume slide down */
			goto fx_f_vslide_dn;
		case EX_CUT:		/* Cut note */
			SET(RETRIG);
			xc->retrig.val = fxp + 1;
			xc->retrig.count = xc->retrig.val;
			xc->retrig.type = 0x10;
			break;
		case EX_DELAY:		/* Note delay */
			/* computed at frame loop */
			break;
		case EX_PATT_DELAY:	/* Pattern delay */
			goto fx_patt_delay;
		case EX_INVLOOP:	/* Invert loop / funk repeat */
			xc->invloop.speed = fxp;
			break;
		}
		break;
	case FX_SPEED:		/* Set speed */
		/* speedup.xm needs BPM = 20 */
		if (p->flags & XMP_FLAGS_VBLANK || fxp < 0x20) {
			goto fx_s3m_speed;
		} else {
			goto fx_s3m_bpm;
		}
		break;

	case FX_FINETUNE:
	      fx_finetune:
		xc->finetune = (int16) (fxp - 0x80);
		break;

	case FX_F_VSLIDE_UP:	/* Fine volume slide up */
	    fx_f_vslide_up:
		SET(FINE_VOLS);
		if (fxp)
			xc->vol.fslide = fxp;
		break;
	case FX_F_VSLIDE_DN:	/* Fine volume slide down */
	    fx_f_vslide_dn:
		SET(FINE_VOLS);
		if (fxp)
			xc->vol.fslide = -fxp;
		break;
	case FX_F_PORTA_UP:	/* Fine portamento up */
	    fx_f_porta_up:
		SET(FINE_BEND);
		if (fxp)
			xc->freq.fslide = -fxp;
		else if (xc->freq.slide > 0)
			xc->freq.slide *= -1;
		break;
	case FX_F_PORTA_DN:	/* Fine portamento down */
	    fx_f_porta_dn:
		SET(FINE_BEND);
		if (fxp)
			xc->freq.fslide = fxp;
		else if (xc->freq.slide < 0)
			xc->freq.slide *= -1;
		break;
	case FX_PATT_DELAY:
	    fx_patt_delay:
		p->flow.delay = fxp;
		break;

	case FX_S3M_SPEED:	/* Set S3M speed */
	    fx_s3m_speed:
		if (fxp)
			p->speed = fxp;
		break;
	case FX_S3M_BPM:	/* Set S3M BPM */
            fx_s3m_bpm:
		if (fxp) {
			if (fxp < XMP_MIN_BPM)
				fxp = XMP_MIN_BPM;
			p->bpm = fxp;
			p->frame_time = m->time_factor * m->rrate / p->bpm;
		}
		break;

#ifndef LIBXMP_CORE_DISABLE_IT
	case FX_IT_BPM:		/* Set IT BPM */
		if (MSN(fxp) == 0) {	/* T0x - Tempo slide down by x */
			SET(TEMPO_SLIDE);
			xc->tempo.slide = -LSN(fxp);
		} else if (MSN(fxp) == 1) {	/* T1x - Tempo slide up by x */
			SET(TEMPO_SLIDE);
			xc->tempo.slide = LSN(fxp);
		} else {
			if (fxp < XMP_MIN_BPM)
				fxp = XMP_MIN_BPM;
			p->bpm = fxp;
		}
		p->frame_time = m->time_factor * m->rrate / p->bpm;
		break;
	case FX_IT_ROWDELAY:
		if (!f->rowdelay_set) {
			f->rowdelay = fxp;
			f->rowdelay_set = 1;
		}
		break;
#endif

	case FX_GLOBALVOL:	/* Set global volume */
		if (fxp > m->gvolbase) {
			p->gvol = m->gvolbase;
		} else {
			p->gvol = fxp;
		}
		break;
	case FX_GVOL_SLIDE:	/* Global volume slide */
            fx_gvolslide:
		if (fxp) {
			SET(GVOL_SLIDE);
			xc->gvol.memory = fxp;
			h = MSN(fxp);
			l = LSN(fxp);

			if (HAS_QUIRK(QUIRK_FINEFX)) {
				if (l == 0xf && h != 0) {
					xc->gvol.slide = 0;
					xc->gvol.fslide = h;
				} else if (h == 0xf && l != 0) {
					xc->gvol.slide = 0;
					xc->gvol.fslide = -l;
				} else {
					xc->gvol.slide = h - l;
					xc->gvol.fslide = 0;
				}
			} else {
				xc->gvol.slide = h - l;
				xc->gvol.fslide = 0;
			}
		} else {
			if ((fxp = xc->gvol.memory) != 0) {
				goto fx_gvolslide;
			}
		}
		break;
	case FX_KEYOFF:		/* Key off */
		xc->keyoff = fxp + 1;
		break;
	case FX_ENVPOS:		/* Set envelope position */
		/* FIXME: Add OpenMPT quirk */
		xc->v_idx = fxp;
		break;
	case FX_MASTER_PAN:	/* Set master pan */
		xc->masterpan = fxp;
		break;
	case FX_PANSLIDE:	/* Pan slide (XM) */
		/* TODO: add memory */
		SET(PAN_SLIDE);
		if (fxp) {
			xc->pan.slide = LSN(fxp) - MSN(fxp);
		}
		break;

#ifndef LIBXMP_CORE_DISABLE_IT
	case FX_IT_PANSLIDE:	/* Pan slide w/ fine pan (IT) */
		SET(PAN_SLIDE);
		if (fxp) {
			if (MSN(fxp) == 0xf) {
				xc->pan.slide = 0;
				xc->pan.fslide = LSN(fxp);
			} else if (LSN(fxp) == 0xf) {
				xc->pan.slide = 0;
				xc->pan.fslide = -MSN(fxp);
			} else {
				SET(PAN_SLIDE);
				xc->pan.slide = LSN(fxp) - MSN(fxp);
				xc->pan.fslide = 0;
			}
		}
		break;
#endif

	case FX_MULTI_RETRIG:	/* Multi retrig */
		if (fxp) {
			xc->retrig.val = fxp;
		}
		if (note) {
			xc->retrig.count = LSN(xc->retrig.val) + 1;
			xc->retrig.type = MSN(xc->retrig.val);
		}
		SET(RETRIG);
		break;
	case FX_TREMOR:		/* Tremor */
		if (fxp != 0) {
			xc->tremor.memory = fxp;
		} else {
			fxp = xc->tremor.memory;
		}
		if (MSN(fxp) == 0)
			fxp |= 0x10;
		if (LSN(fxp) == 0)
			fxp |= 0x01;
		xc->tremor.val = fxp;
		break;
	case FX_XF_PORTA:	/* Extra fine portamento */
	      fx_xf_porta:
		SET(FINE_BEND);
		switch (MSN(fxp)) {
		case 1:
			xc->freq.fslide = -0.25 * LSN(fxp);
			break;
		case 2:
			xc->freq.fslide = 0.25 * LSN(fxp);
			break;
		}
		break;

#ifndef LIBXMP_CORE_DISABLE_IT
	case FX_TRK_VOL:	/* Track volume setting */
		if (fxp <= m->volbase) {
			xc->mastervol = fxp;
		}
		break;
	case FX_TRK_VSLIDE:	/* Track volume slide */
		if (fxp == 0) {
			if ((fxp = xc->trackvol.memory) == 0)
				break;
		}

		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0x0f;
				goto fx_trk_fvslide;
			} else if (l == 0xf && h != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0xf0;
				goto fx_trk_fvslide;
			}
		}
		SET(TRK_VSLIDE);

		if (fxp) {
			h = MSN(fxp);
			l = LSN(fxp);

			xc->trackvol.memory = fxp;
			if (l == 0)
				xc->trackvol.slide = h;
			else if (h == 0)
				xc->trackvol.slide = -l;
			else
				RESET(TRK_VSLIDE);
		}

		break;
	case FX_TRK_FVSLIDE:	/* Track fine volume slide */
	      fx_trk_fvslide:
		SET(TRK_FVSLIDE);
		if (fxp) {
			xc->trackvol.fslide = MSN(fxp) - LSN(fxp);
		}
		break;
	case FX_IT_INSTFUNC:
		switch (fxp) {
		case 0:	/* Past note cut */
			virt_pastnote(ctx, chn, VIRT_ACTION_CUT);
			break;
		case 1:	/* Past note off */
			virt_pastnote(ctx, chn, VIRT_ACTION_OFF);
			break;
		case 2:	/* Past note fade */
			virt_pastnote(ctx, chn, VIRT_ACTION_FADE);
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
	case FX_PANBRELLO:	/* Panbrello */
		SET(PANBRELLO);
		SET_LFO_NOTZERO(&xc->panbrello, LSN(fxp) << 4, MSN(fxp));
		break;
	case FX_PANBRELLO_WF:	/* Panbrello waveform */
		set_lfo_waveform(&xc->panbrello, fxp & 3);
		break;
#endif

#ifndef LIBXMP_CORE_PLAYER
	case FX_VOLSLIDE_UP:	/* Vol slide with uint8 arg */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				fxp &= 0x0f;
				goto fx_f_vslide_up;
			}
		}

		if (fxp)
			xc->vol.slide = fxp;
		SET(VOL_SLIDE);
		break;
	case FX_VOLSLIDE_DN:	/* Vol slide with uint8 arg */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				fxp &= 0x0f;
				goto fx_f_vslide_dn;
			}
		}

		if (fxp)
			xc->vol.slide = -fxp;
		SET(VOL_SLIDE);
		break;
	case FX_F_VSLIDE:	/* Fine volume slide */
		SET(FINE_VOLS);
		if (fxp) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (l == 0)
				xc->vol.fslide = h;
			else if (h == 0)
				xc->vol.fslide = -l;
			else
				RESET(FINE_VOLS);
		}		
		break;
	case FX_NSLIDE_DN:
	case FX_NSLIDE_UP:
	case FX_NSLIDE_R_DN:
	case FX_NSLIDE_R_UP:
		if (fxp != 0) {
			if (fxt == FX_NSLIDE_R_DN || fxt == FX_NSLIDE_R_UP) {
				xc->retrig.val = MSN(fxp);
				xc->retrig.count = MSN(fxp) + 1;
				xc->retrig.type = 0;
			}

			if (fxt == FX_NSLIDE_UP || fxt == FX_NSLIDE_R_UP)
				xc->noteslide.slide = LSN(fxp);
			else
				xc->noteslide.slide = -LSN(fxp);

			xc->noteslide.count = xc->noteslide.speed = MSN(fxp);
		}
		if (fxt == FX_NSLIDE_R_DN || fxt == FX_NSLIDE_R_UP)
			SET(RETRIG);
		SET(NOTE_SLIDE);
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

	case FX_PER_VIBRATO:	/* Persistent vibrato */
		if (LSN(fxp) != 0) {
			SET_PER(VIBRATO);
		} else {
			RESET_PER(VIBRATO);
		}
		SET_LFO_NOTZERO(&xc->vibrato, LSN(fxp) << 2, MSN(fxp));
		break;
	case FX_PER_PORTA_UP:	/* Persistent portamento up */
		SET_PER(PITCHBEND);
		xc->freq.slide = -fxp;
		if ((xc->freq.memory = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_PER_PORTA_DN:	/* Persistent portamento down */
		SET_PER(PITCHBEND);
		xc->freq.slide = fxp;
		if ((xc->freq.memory = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_PER_TPORTA:	/* Persistent tone portamento */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		SET_PER(TONEPORTA);
		do_toneporta(m, xc, note);
		xc->porta.slide = fxp;
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
	case FX_VIBRATO2:	/* Deep vibrato (2x) */
		SET(VIBRATO);
		SET_LFO_NOTZERO(&xc->vibrato, LSN(fxp) << 3, MSN(fxp));
		break;
	case FX_SPEED_CP:	/* Set speed and ... */
		if (fxp)
			p->speed = fxp;
		/* fall through */
	case FX_PER_CANCEL:	/* Cancel persistent effects */
		xc->per_flags = 0;
		break;
#endif
	default:
#ifndef LIBXMP_CORE_PLAYER
		extras_process_fx(ctx, xc, chn, note, fxt, fxp, fnum);
#endif
		break;
	}
}
