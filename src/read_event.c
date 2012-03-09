#include <string.h>
#include "common.h"
#include "player.h"
#include "effects.h"
#include "virtual.h"
#include "period.h"


static inline void copy_channel(struct player_data *p, int to, int from)
{
	if (to > 0 && to != from) {
		memcpy(&p->xc_data[to], &p->xc_data[from],
					sizeof (struct channel_data));
	}
}

static struct xmp_subinstrument *get_subinstrument(struct context_data *ctx,
						   int ins, int key)
{
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;

	if (ins >= 0 && key >= 0) {
		if (mod->xxi[ins].map[key].ins != 0xff) {
			int mapped = mod->xxi[ins].map[key].ins;
			return &mod->xxi[ins].sub[mapped];
		}
	}

	return NULL;
}


#define IS_VALID_INSTRUMENT(x) ((uint32)(x) < mod->ins && mod->xxi[(x)].nsm)

static int read_event_mod(struct context_data *ctx, struct xmp_event *e, int chn, int ctl)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int xins, ins, note, key, flg;
	int cont_sample;
	struct xmp_subinstrument *sub;

	flg = 0;
	ins = note = -1;
	xins = xc->ins;
	key = e->note;
	cont_sample = 0;

	/* Check instrument */

	if (e->ins) {
		ins = e->ins - 1;
		flg = NEW_INS | RESET_VOL | RESET_ENV;
		xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		xc->per_flags = 0;

		if (IS_VALID_INSTRUMENT(ins)) {
			/* valid ins */
			xins = ins;
		} else {
			/* invalid ins */
			virtch_resetchannel(ctx, chn);
		}

		xc->insdef = ins;
		xc->med.arp = xc->med.aidx = 0;
	}

	/* Check note */

	if (key) {
		flg |= NEW_NOTE;

		if (key == XMP_KEY_FADE) {
			SET(FADEOUT);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (key == XMP_KEY_CUT) {
			virtch_resetchannel(ctx, chn);
		} else if (key == XMP_KEY_OFF) {
			SET(RELEASE);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (e->fxt == FX_TONEPORTA || e->f2t == FX_TONEPORTA
			   || e->fxt == FX_TONE_VSLIDE
			   || e->f2t == FX_TONE_VSLIDE) {

			/* When a toneporta is issued after a keyoff event,
			 * retrigger the instrument (xr-nc.xm, bug #586377)
			 *
			 *   flg |= NEW_INS;
			 *   xins = ins;
			 *
			 * (From Decibelter - Cosmic 'Wegian Mamas.xm p04 ch7)
			 * We don't retrigger the sample, it simply continues.
			 * This is important to play sweeps and loops correctly.
			 */
			cont_sample = 1;

			/* set key to 0 so we can have the tone portamento from
			 * the original note (see funky_stars.xm pos 5 ch 9)
			 */
			key = 0;

			/* And do the same if there's no keyoff (see
			 * comic bakery remix.xm pos 1 ch 3)
			 */
		} else if (flg & NEW_INS) {
			xins = ins;
		} else {
			ins = xc->insdef;
			flg |= IS_READY;
		}
	}

	if (!key || key >= XMP_KEY_OFF) {
		ins = xins;
	}

	if (IS_VALID_INSTRUMENT(ins)) {
		flg |= IS_VALID;
	}

	if ((uint32)key < XMP_KEY_OFF && key > 0) {
		key--;

		if (key < XMP_MAX_KEYS) {
			xc->key = key;
		} else {
			flg &= ~IS_VALID;
		}

		if (flg & IS_VALID) {
			struct xmp_subinstrument *sub;

			sub = get_subinstrument(ctx, ins, key);

			if (sub != NULL) {
				int transp = mod->xxi[ins].map[key].xpo;
				int smp;

				note = key + sub->xpo + transp;
				smp = sub->sid;

				if (mod->xxs[smp].len == 0) {
					smp = -1;
				}

				if (smp >= 0 && smp < mod->smp) {
					int to = virtch_setpatch(ctx, chn, ins,
						smp, note, sub->nna, sub->dct,
						sub->dca, ctl, cont_sample);

					if (to < 0) {
						return -1;
					}

					copy_channel(p, to, chn);

					xc->smp = smp;
				}
			} else {
				flg &= ~(RESET_VOL | RESET_ENV | NEW_INS |
				      NEW_NOTE);
			}
		} else {
			virtch_resetchannel(ctx, chn);
		}
	}

	/* Reset flags */
	xc->delay = xc->retrig.delay = 0;
	xc->flags = flg | (xc->flags & 0xff000000); /* keep persistent flags */

	reset_stepper(&xc->arpeggio);
	xc->tremor.val = 0;

	if (IS_VALID_INSTRUMENT(xins)) {
		SET(IS_VALID);
	} else {
		RESET(IS_VALID);
	}

	/* update instrument */
	xc->ins = xins;

	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		RESET(RESET_VOL);
		SET(NEW_VOL);
	}

	if (TEST(NEW_INS) || HAS_QUIRK(QUIRK_OFSRST))
		xc->offset_val = 0;

	/* Secondary effect is processed _first_ and can be overriden
	 * by the primary effect.
	 */
	process_fx(ctx, chn, e->note, e->f2t, e->f2p, xc, 1);
	process_fx(ctx, chn, e->note, e->fxt, e->fxp, xc, 0);

	if (!TEST(IS_VALID)) {
		xc->volume = 0;
		return 0;
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);
	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;

		if (cont_sample == 0) {
			virtch_voicepos(ctx, chn, xc->offset_val);
			if (TEST(OFFSET) && HAS_QUIRK(QUIRK_FX9BUG)) {
				xc->offset_val <<= 1;
			}
		}
		RESET(OFFSET);

		/* Fixed by Frederic Bujon <lvdl@bigfoot.com> */
		if (!TEST(NEW_PAN)) {
			xc->pan = sub->pan;
		}
		if (!TEST(FINETUNE)) {
			xc->finetune = sub->fin;
		}
		xc->freq.s_end = xc->period = note_to_period(note,
				xc->finetune, HAS_QUIRK (QUIRK_LINEAR));

		xc->gvl = sub->gvl;

		set_lfo_depth(&xc->insvib.lfo, sub->vde);
		set_lfo_rate(&xc->insvib.lfo, sub->vra >> 2);
		set_lfo_waveform(&xc->insvib.lfo, sub->vwf);
		xc->insvib.sweep = sub->vsw;

		xc->v_idx = xc->p_idx = xc->f_idx = 0;

		if (sub->ifc & 0x80) {
			xc->filter.cutoff = (sub->ifc - 0x80) * 2;
		} else {
			xc->filter.cutoff = 0xff;
		}

		if (sub->ifr & 0x80) {
			xc->filter.resonance = (sub->ifr - 0x80) * 2;
		} else {
			xc->filter.resonance = 0;
		}

		set_lfo_phase(&xc->vibrato, 0);
		set_lfo_phase(&xc->tremolo, 0);
	}

	if (xc->key < 0) {
		return 0;
	}

	if (TEST(RESET_ENV)) {
		RESET(RELEASE | FADEOUT);
	}

	if (TEST(RESET_VOL)) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	if (HAS_QUIRK(QUIRK_ST3GVOL) && TEST(NEW_VOL)) {
		xc->volume = xc->volume * p->gvol.volume / m->volbase;
	}

	return 0;
}

static int read_event_ft2(struct context_data *ctx, struct xmp_event *e, int chn, int ctl)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int xins, ins, note, key, flg;
	int cont_sample;
	struct xmp_subinstrument *sub;

	flg = 0;
	ins = note = -1;
	xins = xc->ins;
	key = e->note;
	cont_sample = 0;

	/* Check instrument */

	if (e->ins) {
		ins = e->ins - 1;
		flg = NEW_INS | RESET_VOL | RESET_ENV;
		xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		xc->per_flags = 0;

		if (IS_VALID_INSTRUMENT(ins)) {
			/* valid ins */
			xins = ins;
		} else {
			/* invalid ins */

			/* FT2 doesn't cut on invalid instruments (it keeps
			 * playing the previous one)
			 */
			ins = -1;
			flg = 0;
		}

		xc->insdef = ins;
		xc->med.arp = xc->med.aidx = 0;
	}

	/* Check note */

	if (key) {
		flg |= NEW_NOTE;

		if (key == XMP_KEY_FADE) {
			SET(FADEOUT);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (key == XMP_KEY_CUT) {
			virtch_resetchannel(ctx, chn);
		} else if (key == XMP_KEY_OFF) {
			SET(RELEASE);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (e->fxt == FX_TONEPORTA || e->f2t == FX_TONEPORTA
			   || e->fxt == FX_TONE_VSLIDE
			   || e->f2t == FX_TONE_VSLIDE) {

			/* When a toneporta is issued after a keyoff event,
			 * retrigger the instrument (xr-nc.xm, bug #586377)
			 *
			 *   flg |= NEW_INS;
			 *   xins = ins;
			 *
			 * (From Decibelter - Cosmic 'Wegian Mamas.xm p04 ch7)
			 * We don't retrigger the sample, it simply continues.
			 * This is important to play sweeps and loops correctly
			 */
			cont_sample = 1;

			/* set key to 0 so we can have the tone portamento from
			 * the original note (see funky_stars.xm pos 5 ch 9)
			 */
			key = 0;

			/* And do the same if there's no keyoff (see
			 * comic bakery remix.xm pos 1 ch 3)
			 */
		} else if (flg & NEW_INS) {
			xins = ins;
		} else {
			ins = xc->insdef;
			flg |= IS_READY;
		}
	}

	/* Process QUIRK_OINSVOL */
	if (e->ins) {
		struct xmp_subinstrument *sub;

		if (key == 0 || key >= XMP_KEY_OFF) {
			/* Previous instrument */
			sub = get_subinstrument(ctx, xc->ins_oinsvol, xc->key);

			/* No note */
			if (sub != NULL) {
				xc->volume = sub->vol;
				flg |= NEW_VOL;
				flg &= ~RESET_VOL;
			}
			if (!IS_VALID_INSTRUMENT(ins)) {
				/* If instrument is invalid, make it valid */
				xins = xc->ins;
			}
			ins = xins;
		} else {
			/* Retrieve volume when we have note */

			/* and only if we have instrument, otherwise we're in
			 * case 1: new note and no instrument
			 */
			if (e->ins) {
				/* Current instrument */
				sub = get_subinstrument(ctx, ins, key);
				if (sub != NULL) {
					xc->volume = sub->vol;
				} else {
					xc->volume = 0;
				}
				xc->ins_oinsvol = ins;
				flg |= NEW_VOL;
				flg &= ~RESET_VOL;
			}
		}
	}

	if (IS_VALID_INSTRUMENT(ins)) {
		flg |= IS_VALID;
	}

	if ((uint32)key < XMP_KEY_OFF && key > 0) {
		key--;

		if (key < XMP_MAX_KEYS) {
			xc->key = key;
		} else {
			flg &= ~IS_VALID;
		}

		if (flg & IS_VALID) {
			struct xmp_subinstrument *sub;

			sub = get_subinstrument(ctx, ins, key);

			if (sub != NULL) {
				int transp = mod->xxi[ins].map[key].xpo;
				int smp;

				note = key + sub->xpo + transp;
				smp = sub->sid;

				if (mod->xxs[smp].len == 0) {
					smp = -1;
				}

				if (smp >= 0 && smp < mod->smp) {
					int to = virtch_setpatch(ctx, chn, ins,
						smp, note, sub->nna, sub->dct,
						sub->dca, ctl, cont_sample);

					if (to < 0) {
						return -1;
					}

					copy_channel(p, to, chn);

					xc->smp = smp;
				}
			} else {
				flg &= ~(RESET_VOL | RESET_ENV | NEW_INS |
				      NEW_NOTE);
			}
		}
		/* cut only when note + invalid ins */
	}

	/* Reset flags */
	xc->delay = xc->retrig.delay = 0;
	xc->flags = flg | (xc->flags & 0xff000000); /* keep persistent flags */

	reset_stepper(&xc->arpeggio);
	xc->tremor.val = 0;

	if (IS_VALID_INSTRUMENT(xins)) {
		SET(IS_VALID);
	} else {
		RESET(IS_VALID);
	}

	/* update instrument */
	xc->ins = xins;

	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		RESET(RESET_VOL);
		SET(NEW_VOL);
	}

	if (TEST(NEW_INS) || HAS_QUIRK(QUIRK_OFSRST))
		xc->offset_val = 0;

	/* Secondary effect is processed _first_ and can be overriden
	 * by the primary effect.
	 */
	process_fx(ctx, chn, e->note, e->f2t, e->f2p, xc, 1);
	process_fx(ctx, chn, e->note, e->fxt, e->fxp, xc, 0);

	if (!TEST(IS_VALID)) {
		xc->volume = 0;
		return 0;
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);
	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;

		if (cont_sample == 0) {
			virtch_voicepos(ctx, chn, xc->offset_val);
			if (TEST(OFFSET) && HAS_QUIRK(QUIRK_FX9BUG)) {
				xc->offset_val <<= 1;
			}
		}
		RESET(OFFSET);

		/* Fixed by Frederic Bujon <lvdl@bigfoot.com> */
		if (!TEST(NEW_PAN)) {
			xc->pan = sub->pan;
		}
		if (!TEST(FINETUNE)) {
			xc->finetune = sub->fin;
		}
		xc->freq.s_end = xc->period = note_to_period(note,
				xc->finetune, HAS_QUIRK (QUIRK_LINEAR));

		xc->gvl = sub->gvl;

		set_lfo_depth(&xc->insvib.lfo, sub->vde);
		set_lfo_rate(&xc->insvib.lfo, sub->vra >> 2);
		set_lfo_waveform(&xc->insvib.lfo, sub->vwf);
		xc->insvib.sweep = sub->vsw;

		xc->v_idx = xc->p_idx = xc->f_idx = 0;

		if (sub->ifc & 0x80) {
			xc->filter.cutoff = (sub->ifc - 0x80) * 2;
		} else {
			xc->filter.cutoff = 0xff;
		}

		if (sub->ifr & 0x80) {
			xc->filter.resonance = (sub->ifr - 0x80) * 2;
		} else {
			xc->filter.resonance = 0;
		}

		set_lfo_phase(&xc->vibrato, 0);
		set_lfo_phase(&xc->tremolo, 0);
	}

	if (xc->key < 0) {
		return 0;
	}

	if (TEST(RESET_ENV)) {
		RESET(RELEASE | FADEOUT);
	}

	if (TEST(RESET_VOL)) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	if (HAS_QUIRK(QUIRK_ST3GVOL) && TEST(NEW_VOL)) {
		xc->volume = xc->volume * p->gvol.volume / m->volbase;
	}

	return 0;
}

static int read_event_st3(struct context_data *ctx, struct xmp_event *e, int chn, int ctl)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int xins, ins, note, key, flg;
	int cont_sample;
	struct xmp_subinstrument *sub;

	flg = 0;
	ins = note = -1;
	xins = xc->ins;
	key = e->note;
	cont_sample = 0;

	/* Check instrument */

	if (e->ins) {
		ins = e->ins - 1;
		flg = NEW_INS | RESET_VOL | RESET_ENV;
		xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		xc->per_flags = 0;

		if (IS_VALID_INSTRUMENT(ins)) {
			/* valid ins */
			xins = ins;
		} else {
			/* invalid ins */

			/* Ignore invalid instruments */
			ins = -1;
			flg = 0;
		}

		xc->insdef = ins;
		xc->med.arp = xc->med.aidx = 0;
	}

	/* Check note */

	if (key) {
		flg |= NEW_NOTE;

		if (key == XMP_KEY_FADE) {
			SET(FADEOUT);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (key == XMP_KEY_CUT) {
			virtch_resetchannel(ctx, chn);
		} else if (key == XMP_KEY_OFF) {
			SET(RELEASE);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (e->fxt == FX_TONEPORTA || e->f2t == FX_TONEPORTA
			   || e->fxt == FX_TONE_VSLIDE
			   || e->f2t == FX_TONE_VSLIDE) {

			/* Always retrig in tone portamento: Fix portamento in
			 * 7spirits.s3m, mod.Biomechanoid
			 */
			if (e->ins && xc->ins != ins) {
				flg |= NEW_INS;
				xins = ins;
			}
		} else if (flg & NEW_INS) {
			xins = ins;
		} else {
			ins = xc->insdef;
			flg |= IS_READY;
		}
	}

	if (!key || key >= XMP_KEY_OFF) {
		ins = xins;
	}

	if (IS_VALID_INSTRUMENT(ins)) {
		flg |= IS_VALID;
	}

	if ((uint32)key < XMP_KEY_OFF && key > 0) {
		key--;

		if (key < XMP_MAX_KEYS) {
			xc->key = key;
		} else {
			flg &= ~IS_VALID;
		}

		if (flg & IS_VALID) {
			struct xmp_subinstrument *sub;

			sub = get_subinstrument(ctx, ins, key);

			if (sub != NULL) {
				int transp = mod->xxi[ins].map[key].xpo;
				int smp;

				note = key + sub->xpo + transp;
				smp = sub->sid;

				if (mod->xxs[smp].len == 0) {
					smp = -1;
				}

				if (smp >= 0 && smp < mod->smp) {
					int to = virtch_setpatch(ctx, chn, ins,
						smp, note, sub->nna, sub->dct,
						sub->dca, ctl, cont_sample);

					if (to < 0) {
						return -1;
					}

					copy_channel(p, to, chn);

					xc->smp = smp;
				}
			} else {
				flg &= ~(RESET_VOL | RESET_ENV | NEW_INS |
				      NEW_NOTE);
			}
		} else {
			virtch_resetchannel(ctx, chn);
		}
	}

	/* Reset flags */
	xc->delay = xc->retrig.delay = 0;
	xc->flags = flg | (xc->flags & 0xff000000); /* keep persistent flags */

	reset_stepper(&xc->arpeggio);
	xc->tremor.val = 0;

	if (IS_VALID_INSTRUMENT(xins)) {
		SET(IS_VALID);
	} else {
		RESET(IS_VALID);
	}

	/* update instrument */
	xc->ins = xins;

	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		RESET(RESET_VOL);
		SET(NEW_VOL);
	}

	if (TEST(NEW_INS) || HAS_QUIRK(QUIRK_OFSRST))
		xc->offset_val = 0;

	/* Secondary effect is processed _first_ and can be overriden
	 * by the primary effect.
	 */
	process_fx(ctx, chn, e->note, e->f2t, e->f2p, xc, 1);
	process_fx(ctx, chn, e->note, e->fxt, e->fxp, xc, 0);

	if (!TEST(IS_VALID)) {
		xc->volume = 0;
		return 0;
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);
	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;

		if (cont_sample == 0) {
			virtch_voicepos(ctx, chn, xc->offset_val);
			if (TEST(OFFSET) && HAS_QUIRK(QUIRK_FX9BUG)) {
				xc->offset_val <<= 1;
			}
		}
		RESET(OFFSET);

		/* Fixed by Frederic Bujon <lvdl@bigfoot.com> */
		if (!TEST(NEW_PAN)) {
			xc->pan = sub->pan;
		}
		if (!TEST(FINETUNE)) {
			xc->finetune = sub->fin;
		}
		xc->freq.s_end = xc->period = note_to_period(note,
				xc->finetune, HAS_QUIRK (QUIRK_LINEAR));

		xc->gvl = sub->gvl;

		set_lfo_depth(&xc->insvib.lfo, sub->vde);
		set_lfo_rate(&xc->insvib.lfo, sub->vra >> 2);
		set_lfo_waveform(&xc->insvib.lfo, sub->vwf);
		xc->insvib.sweep = sub->vsw;

		xc->v_idx = xc->p_idx = xc->f_idx = 0;

		if (sub->ifc & 0x80) {
			xc->filter.cutoff = (sub->ifc - 0x80) * 2;
		} else {
			xc->filter.cutoff = 0xff;
		}

		if (sub->ifr & 0x80) {
			xc->filter.resonance = (sub->ifr - 0x80) * 2;
		} else {
			xc->filter.resonance = 0;
		}

		set_lfo_phase(&xc->vibrato, 0);
		set_lfo_phase(&xc->tremolo, 0);
	}

	if (xc->key < 0) {
		return 0;
	}

	if (TEST(RESET_ENV)) {
		RESET(RELEASE | FADEOUT);
	}

	if (TEST(RESET_VOL)) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	if (HAS_QUIRK(QUIRK_ST3GVOL) && TEST(NEW_VOL)) {
		xc->volume = xc->volume * p->gvol.volume / m->volbase;
	}

	return 0;
}

static int read_event_it(struct context_data *ctx, struct xmp_event *e, int chn, int ctl)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int xins, ins, note, key, flg;
	int cont_sample;
	struct xmp_subinstrument *sub;

	/* Emulate Impulse Tracker "always read instrument" bug */
	if (e->note && !e->ins && xc->delayed_ins) {
		e->ins = xc->delayed_ins;
		xc->delayed_ins = 0;
	}

	flg = 0;
	ins = note = -1;
	xins = xc->ins;
	key = e->note;
	cont_sample = 0;

	/* Check instrument */

	if (e->ins) {
		ins = e->ins - 1;
		flg = NEW_INS | RESET_VOL | RESET_ENV;
		xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		xc->per_flags = 0;

		/* Benjamin Shadwick <benshadwick@gmail.com> informs that
		 * Vestiges.xm has sticky note issues around time 0:51.
		 * Tested it with FT2 and a new note with invalid instrument
		 * should always cut current note
		 */
		/*if (HAS_QUIRK(QUIRK_OINSMOD)) {
			if (TEST(IS_READY)) {
				xins = xc->insdef;
				RESET(IS_READY);
			}
		} else */

		if (IS_VALID_INSTRUMENT(ins)) {
			/* valid ins */

			if (!key) {
				/* IT: Reset note for every new != ins */
				if (xins == ins) {
					flg = NEW_INS | RESET_VOL;
				} else {
					key = xc->key + 1;
				}
			}
			xins = ins;
		} else {
			/* invalid ins */

			/* Ignore invalid instruments */
			ins = -1;
			flg = 0;
		}

		xc->insdef = ins;
		xc->med.arp = xc->med.aidx = 0;
	}

	/* Check note */

	if (key) {
		flg |= NEW_NOTE;

		if (key == XMP_KEY_FADE) {
			SET(FADEOUT);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (key == XMP_KEY_CUT) {
			virtch_resetchannel(ctx, chn);
		} else if (key == XMP_KEY_OFF) {
			SET(RELEASE);
			flg &= ~(RESET_VOL | RESET_ENV);
		} else if (e->fxt == FX_TONEPORTA || e->f2t == FX_TONEPORTA
			   || e->fxt == FX_TONE_VSLIDE
			   || e->f2t == FX_TONE_VSLIDE) {

			/* Always retrig on tone portamento: Fix portamento in
			 * 7spirits.s3m, mod.Biomechanoid
			 */
			if (e->ins && xc->ins != ins) {
				flg |= NEW_INS;
				xins = ins;
			}
		} else if (flg & NEW_INS) {
			xins = ins;
		} else {
			ins = xc->insdef;
			flg |= IS_READY;
		}
	}

	if (!key || key >= XMP_KEY_OFF) {
		ins = xins;
	}

	if (IS_VALID_INSTRUMENT(ins)) {
		flg |= IS_VALID;
	}

	if ((uint32)key < XMP_KEY_OFF && key > 0) {
		key--;

		if (key < XMP_MAX_KEYS) {
			xc->key = key;
		} else {
			flg &= ~IS_VALID;
		}

		if (flg & IS_VALID) {
			struct xmp_subinstrument *sub;

			sub = get_subinstrument(ctx, ins, key);

			if (sub != NULL) {
				int transp = mod->xxi[ins].map[key].xpo;
				int smp;

				note = key + sub->xpo + transp;
				smp = sub->sid;

				if (mod->xxs[smp].len == 0) {
					smp = -1;
				}

				if (smp >= 0 && smp < mod->smp) {
					int to = virtch_setpatch(ctx, chn, ins,
						smp, note, sub->nna, sub->dct,
						sub->dca, ctl, cont_sample);

					if (to < 0) {
						return -1;
					}

					copy_channel(p, to, chn);

					xc->smp = smp;
				}
			} else {
				flg &= ~(RESET_VOL | RESET_ENV | NEW_INS |
				      NEW_NOTE);
			}
		} else {
			virtch_resetchannel(ctx, chn);
		}
	}

	/* Reset flags */
	xc->delay = xc->retrig.delay = 0;
	xc->flags = flg | (xc->flags & 0xff000000); /* keep persistent flags */

	reset_stepper(&xc->arpeggio);
	xc->tremor.val = 0;

	if (IS_VALID_INSTRUMENT(xins)) {
		SET(IS_VALID);
	} else {
		RESET(IS_VALID);
	}

	/* update instrument */
	xc->ins = xins;

	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		RESET(RESET_VOL);
		SET(NEW_VOL);
	}

	if (TEST(NEW_INS) || HAS_QUIRK(QUIRK_OFSRST))
		xc->offset_val = 0;

	/* Secondary effect is processed _first_ and can be overriden
	 * by the primary effect.
	 */
	process_fx(ctx, chn, e->note, e->f2t, e->f2p, xc, 1);
	process_fx(ctx, chn, e->note, e->fxt, e->fxp, xc, 0);

	if (!TEST(IS_VALID)) {
		xc->volume = 0;
		return 0;
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);
	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;

		if (cont_sample == 0) {
			virtch_voicepos(ctx, chn, xc->offset_val);
			if (TEST(OFFSET) && HAS_QUIRK(QUIRK_FX9BUG)) {
				xc->offset_val <<= 1;
			}
		}
		RESET(OFFSET);

		/* Fixed by Frederic Bujon <lvdl@bigfoot.com> */
		if (!TEST(NEW_PAN)) {
			xc->pan = sub->pan;
		}
		if (!TEST(FINETUNE)) {
			xc->finetune = sub->fin;
		}
		xc->freq.s_end = xc->period = note_to_period(note,
				xc->finetune, HAS_QUIRK (QUIRK_LINEAR));

		xc->gvl = sub->gvl;

		set_lfo_depth(&xc->insvib.lfo, sub->vde);
		set_lfo_rate(&xc->insvib.lfo, sub->vra >> 2);
		set_lfo_waveform(&xc->insvib.lfo, sub->vwf);
		xc->insvib.sweep = sub->vsw;

		xc->v_idx = xc->p_idx = xc->f_idx = 0;

		if (sub->ifc & 0x80) {
			xc->filter.cutoff = (sub->ifc - 0x80) * 2;
		} else {
			xc->filter.cutoff = 0xff;
		}

		if (sub->ifr & 0x80) {
			xc->filter.resonance = (sub->ifr - 0x80) * 2;
		} else {
			xc->filter.resonance = 0;
		}

		set_lfo_phase(&xc->vibrato, 0);
		set_lfo_phase(&xc->tremolo, 0);
	}

	if (xc->key < 0) {
		return 0;
	}

	if (TEST(RESET_ENV)) {
		RESET(RELEASE | FADEOUT);
	}

	if (TEST(RESET_VOL)) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	if (HAS_QUIRK(QUIRK_ST3GVOL) && TEST(NEW_VOL)) {
		xc->volume = xc->volume * p->gvol.volume / m->volbase;
	}

	return 0;
}


int read_event(struct context_data *ctx, struct xmp_event *e, int chn, int ctl)
{
	struct module_data *m = &ctx->m;

	switch (m->read_event_type) {
	case READ_EVENT_MOD:
		return read_event_mod(ctx, e, chn, ctl);
	case READ_EVENT_FT2:
		return read_event_ft2(ctx, e, chn, ctl);
	case READ_EVENT_ST3:
		return read_event_st3(ctx, e, chn, ctl);
	case READ_EVENT_IT:
		return read_event_it(ctx, e, chn, ctl);
	default:
		return read_event_mod(ctx, e, chn, ctl);
	}
}
