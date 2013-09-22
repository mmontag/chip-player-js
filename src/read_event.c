/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <string.h>
#include "common.h"
#include "player.h"
#include "effects.h"
#include "virtual.h"
#include "period.h"
#include "med_extras.h"


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

static void reset_envelopes(struct context_data *ctx, struct channel_data *xc)
{
	struct xmp_instrument *xxi;

	xxi = get_instrument(ctx, xc->ins);

	/* Reset envelope positions */
	if (~xxi->aei.flg & XMP_ENVELOPE_CARRY) {
		xc->v_idx = 0;
	}
	if (~xxi->pei.flg & XMP_ENVELOPE_CARRY) {
		xc->p_idx = 0;
	}
	if (~xxi->fei.flg & XMP_ENVELOPE_CARRY) {
		xc->f_idx = 0;
	}
}

static void set_effect_defaults(struct context_data *ctx, int note,
				struct xmp_subinstrument *sub,
				struct channel_data *xc, int is_toneporta)
{
	struct module_data *m = &ctx->m;

	if (sub != NULL && note >= 0) {
		xc->pan.val = sub->pan;
		xc->finetune = sub->fin;
		xc->gvl = sub->gvl;

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

		set_lfo_depth(&xc->insvib.lfo, sub->vde);
		set_lfo_rate(&xc->insvib.lfo, sub->vra >> 2);
		set_lfo_waveform(&xc->insvib.lfo, sub->vwf);
		xc->insvib.sweep = sub->vsw;

		set_lfo_phase(&xc->vibrato, 0);
		set_lfo_phase(&xc->tremolo, 0);

		xc->porta.target = note_to_period(note,
				xc->finetune, HAS_QUIRK(QUIRK_LINEAR));
		if (xc->period < 1 || !is_toneporta)
			xc->period = xc->porta.target;
	}

	xc->delay = 0;
	xc->tremor.val = 0;

	/* Reset arpeggio */
	xc->arpeggio.val[0] = 0;
	xc->arpeggio.count = 0;
	xc->arpeggio.size = 1;
}


#define IS_TONEPORTA(x) ((x) == FX_TONEPORTA || (x) == FX_TONE_VSLIDE \
		|| (x) == FX_PER_TPORTA)

#define set_patch(ctx,chn,ins,smp,note) \
	virt_setpatch(ctx, chn, ins, smp, note, 0, 0, 0)

static int read_event_mod(struct context_data *ctx, struct xmp_event *e, int chn)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int note;
	struct xmp_subinstrument *sub;
	int new_invalid_ins = 0;
	int is_toneporta;
	int use_ins_vol;

	xc->flags = 0;
	note = -1;
	is_toneporta = 0;
	use_ins_vol = 0;

	if (IS_TONEPORTA(e->fxt) || IS_TONEPORTA(e->f2t)) {
		is_toneporta = 1;
	}

	/* Check instrument */

	if (e->ins) {
		int ins = e->ins - 1;
		use_ins_vol = 1;
		SET(NEW_INS);
		xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		xc->per_flags = 0;
		xc->offset_val = 0;
		RESET_NOTE(NOTE_RELEASE);

		if (IS_VALID_INSTRUMENT(ins)) {
			if (is_toneporta) {
				/* Get new instrument volume */
				sub = get_subinstrument(ctx, ins, e->note);
				if (sub != NULL) {
					xc->volume = sub->vol;
					use_ins_vol = 0;
				}
			} else {
				xc->ins = ins;
			}
		} else {
			new_invalid_ins = 1;
			virt_resetchannel(ctx, chn);
		}

		if (HAS_MED_CHANNEL_EXTRAS(*xc)) {
			MED_CHANNEL_EXTRAS(*xc)->arp = 0;
			MED_CHANNEL_EXTRAS(*xc)->aidx = 0;
		}
	}

	/* Check note */

	if (e->note) {
		SET(NEW_NOTE);

		if (e->note == XMP_KEY_OFF) {
			SET_NOTE(NOTE_RELEASE);
			use_ins_vol = 0;
		} else if (!is_toneporta) {
			xc->key = e->note - 1;
			RESET_NOTE(NOTE_END);
	
			sub = get_subinstrument(ctx, xc->ins, xc->key);
	
			if (!new_invalid_ins && sub != NULL) {
				int transp = mod->xxi[xc->ins].map[xc->key].xpo;
				int smp;
	
				note = xc->key + sub->xpo + transp;
				smp = sub->sid;
	
				if (mod->xxs[smp].len == 0) {
					smp = -1;
				}
	
				if (smp >= 0 && smp < mod->smp) {
					set_patch(ctx, chn, xc->ins, smp, note);
					xc->smp = smp;
				}
			} else {
				xc->flags = 0;
				use_ins_vol = 0;
			}
		}
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);

	set_effect_defaults(ctx, note, sub, xc, is_toneporta);
	if (e->ins && sub != NULL)
		reset_envelopes(ctx, xc);

	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		SET(NEW_VOL);
	}

	/* Secondary effect handled first */
	process_fx(ctx, xc, chn, e->note, e->f2t, e->f2p, 1);
	process_fx(ctx, xc, chn, e->note, e->fxt, e->fxp, 0);

	if (TEST(NEW_VOL))
		use_ins_vol = 0;

	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;

		virt_voicepos(ctx, chn, xc->offset_val);
		if (TEST(OFFSET) && p->flags & XMP_FLAGS_FX9BUG)
			xc->offset_val <<= 1;
		RESET(OFFSET);
	}

	if (use_ins_vol) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	return 0;
}

static int read_event_ft2(struct context_data *ctx, struct xmp_event *e, int chn)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int note, key;
	struct xmp_subinstrument *sub;
	int new_invalid_ins;
	int is_toneporta;
	int use_ins_vol;

	xc->flags = 0;
	note = -1;
	key = e->note;
	new_invalid_ins = 0;
	is_toneporta = 0;
	use_ins_vol = 0;

	if (IS_TONEPORTA(e->fxt) || IS_TONEPORTA(e->f2t)) {
		is_toneporta = 1;
	}

	/* Check instrument */

	if (e->ins) {
		int ins = e->ins - 1;
		SET(NEW_INS);
		use_ins_vol = 1;
		xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		xc->per_flags = 0;
		RESET_NOTE(NOTE_RELEASE);

		if (IS_VALID_INSTRUMENT(ins)) {
			if (!is_toneporta)
				xc->ins = ins;
		} else {
			new_invalid_ins = 1;

			/* If no note is set FT2 doesn't cut on invalid
			 * instruments (it keeps playing the previous one).
			 * If a note is set it cuts the current sample.
			 */
			xc->flags = 0;

			if (is_toneporta) {
				key = 0;
			}
		}
	}

	/* Check note */

	if (key) {
		SET(NEW_NOTE);

		if (key == XMP_KEY_OFF) {
			SET_NOTE(NOTE_RELEASE);
			use_ins_vol = 0;
		} else if (is_toneporta) {
			/* set key to 0 so we can have the tone portamento from
			 * the original note (see funky_stars.xm pos 5 ch 9)
			 */
			key = 0;

			/* And do the same if there's no keyoff (see comic
			 * bakery remix.xm pos 1 ch 3)
			 */
		}

		if (new_invalid_ins) {
			virt_resetchannel(ctx, chn);
		}
	}

	/* FT2: Retrieve old instrument volume */
	if (e->ins) {
		struct xmp_subinstrument *sub;

		if (key == 0 || key >= XMP_KEY_OFF) {
			/* Previous instrument */
			sub = get_subinstrument(ctx, xc->ins_oinsvol, xc->key);

			/* No note */
			if (sub != NULL) {
				xc->volume = sub->vol;
				SET(NEW_VOL);
			}
		} else {
			/* Retrieve volume when we have note */

			/* and only if we have instrument, otherwise we're in
			 * case 1: new note and no instrument
			 */

			/* Current instrument */
			sub = get_subinstrument(ctx, xc->ins, key - 1);
			if (sub != NULL) {
				xc->volume = sub->vol;
			} else {
				xc->volume = 0;
			}
			xc->ins_oinsvol = xc->ins;
			SET(NEW_VOL);
		}
	}

	if ((uint32)key <= XMP_MAX_KEYS && key > 0) {
		xc->key = --key;
		xc->fadeout = 0x8000;		/* for Last Battle.xm */
		RESET_NOTE(NOTE_RELEASE | NOTE_END);

		sub = get_subinstrument(ctx, xc->ins, key);

		if (!new_invalid_ins && sub != NULL) {
			int transp = mod->xxi[xc->ins].map[key].xpo;
			int smp;

			note = key + sub->xpo + transp;
			smp = sub->sid;

			if (mod->xxs[smp].len == 0) {
				smp = -1;
			}

			if (smp >= 0 && smp < mod->smp) {
				set_patch(ctx, chn, xc->ins, smp, note);
				xc->smp = smp;
			}
		} else {
			xc->flags = 0;
			use_ins_vol = 0;
		}
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);

	set_effect_defaults(ctx, note, sub, xc, is_toneporta);
	if (e->ins && sub != NULL) {
		/* Reset envelopes on new instrument, see olympic.xm pos 10
		 * But make sure we have an instrument set, see Letting go
		 * pos 4 chn 20
		 */
		reset_envelopes(ctx, xc);
	}

	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		SET(NEW_VOL);
	}

	/* FT2: always reset sample offset */
	xc->offset_val = 0;

	/* Secondary effect handled first */
	process_fx(ctx, xc, chn, e->note, e->f2t, e->f2p, 1);
	process_fx(ctx, xc, chn, e->note, e->fxt, e->fxp, 0);

	if (TEST(NEW_VOL))
		use_ins_vol = 0;

	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;

		/* (From Decibelter - Cosmic 'Wegian Mamas.xm p04 ch7)
		 * We retrigger the sample only if we have a new note without
		 * tone portamento, otherwise we won't play sweeps and loops
		 * correctly.
		 */
		virt_voicepos(ctx, chn, xc->offset_val);
		if (TEST(OFFSET))
			xc->offset_val <<= 1;
		RESET(OFFSET);
	}

	if (use_ins_vol) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	return 0;
}

static int read_event_st3(struct context_data *ctx, struct xmp_event *e, int chn)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int note;
	struct xmp_subinstrument *sub;
	int not_same_ins;
	int is_toneporta;
	int use_ins_vol;

	xc->flags = 0;
	note = -1;
	not_same_ins = 0;
	is_toneporta = 0;
	use_ins_vol = 0;

	if (IS_TONEPORTA(e->fxt) || IS_TONEPORTA(e->f2t)) {
		is_toneporta = 1;
	}

	/* Check instrument */

	if (e->ins) {
		int ins = e->ins - 1;
		SET(NEW_INS);
		use_ins_vol = 1;
		xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		xc->per_flags = 0;
		xc->offset_val = 0;
		RESET_NOTE(NOTE_RELEASE);

		if (IS_VALID_INSTRUMENT(ins)) {
			/* valid ins */
			if (xc->ins != ins) {
				not_same_ins = 1;
				if (!is_toneporta) {
					xc->ins = ins;
				} else {
					/* Get new instrument volume */
					sub = get_subinstrument(ctx, ins, e->note);
					if (sub != NULL) {
						xc->volume = sub->vol;
						use_ins_vol = 0;
					}
				}
			}
		} else {
			/* invalid ins */

			/* Ignore invalid instruments */
			xc->flags = 0;
			use_ins_vol = 0;
		}
	}

	/* Check note */

	if (e->note) {
		SET(NEW_NOTE);

		if (e->note == XMP_KEY_OFF) {
			SET_NOTE(NOTE_RELEASE);
			use_ins_vol = 0;
		} else if (is_toneporta) {
			/* Always retrig in tone portamento: Fix portamento in
			 * 7spirits.s3m, mod.Biomechanoid
			 */
			if (not_same_ins) {
				xc->offset_val = 0;
			}
		} else {
			xc->key = e->note - 1;
			RESET_NOTE(NOTE_END);
	
			sub = get_subinstrument(ctx, xc->ins, xc->key);
	
			if (sub != NULL) {
				int transp = mod->xxi[xc->ins].map[xc->key].xpo;
				int smp;
	
				note = xc->key + sub->xpo + transp;
				smp = sub->sid;
	
				if (mod->xxs[smp].len == 0) {
					smp = -1;
				}
	
				if (smp >= 0 && smp < mod->smp) {
					set_patch(ctx, chn, xc->ins, smp, note);
					xc->smp = smp;
				}
			} else {
				xc->flags = 0;
				use_ins_vol = 0;
			}
		}
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);

	set_effect_defaults(ctx, note, sub, xc, is_toneporta);
	if (e->ins && sub != NULL)
		reset_envelopes(ctx, xc);

	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		SET(NEW_VOL);
	}

	/* Secondary effect handled first */
	process_fx(ctx, xc, chn, e->note, e->f2t, e->f2p, 1);
	process_fx(ctx, xc, chn, e->note, e->fxt, e->fxp, 0);

	if (TEST(NEW_VOL))
		use_ins_vol = 0;

	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;
		virt_voicepos(ctx, chn, xc->offset_val);
		if (TEST(OFFSET))
			xc->offset_val <<= 1;
		RESET(OFFSET);
	}

	if (use_ins_vol) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	/* ST3: check QUIRK_ST3GVOL only in ST3 event reader */
	if (HAS_QUIRK(QUIRK_ST3GVOL) && TEST(NEW_VOL)) {
		xc->volume = xc->volume * p->gvol / m->volbase;
	}

	return 0;
}

static int read_event_it(struct context_data *ctx, struct xmp_event *e, int chn)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int note, key;
	struct xmp_subinstrument *sub;
	int not_same_ins;
	int new_invalid_ins;
	int is_toneporta, is_release;
	int candidate_ins;
	int reset_env;
	int use_ins_vol;
	unsigned char e_ins;

	e_ins = e->ins;

	/* Emulate Impulse Tracker "always read instrument" bug */
	if (e_ins) {
		xc->delayed_ins = 0;
	} else if (e->note && xc->delayed_ins) {
		e_ins = xc->delayed_ins;
		xc->delayed_ins = 0;
	}

	xc->flags = 0;
	note = -1;
	key = e->note;
	not_same_ins = 0;
	new_invalid_ins = 0;
	is_toneporta = 0;
	is_release = 0;
	reset_env = 0;
	use_ins_vol = 0;
	candidate_ins = xc->ins;

	if (IS_TONEPORTA(e->fxt) || IS_TONEPORTA(e->f2t)) {
		is_toneporta = 1;
	}

	if (TEST_NOTE(NOTE_RELEASE | NOTE_FADEOUT)) {
		is_release = 1;
	}

	/* Check instrument */

	if (e_ins) {
		int ins = e_ins - 1;

		if (!is_release || (!is_toneporta || xc->ins != ins)) {
			SET(NEW_INS);
			use_ins_vol = 1;
			reset_env = 1;
			xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
		}
		xc->per_flags = 0;

		if (IS_VALID_INSTRUMENT(ins)) {
			/* valid ins */

			if (!key) {
				/* IT: Reset note for every new != ins */
				if (xc->ins == ins) {
					SET(NEW_INS);
					use_ins_vol = 1;
				} else {
					key = xc->key + 1;
				}
			}
			if (xc->ins != ins) {
				not_same_ins = 1;
				candidate_ins = ins;
				if (is_toneporta) {
					/* Get new instrument volume */
					sub = get_subinstrument(ctx, ins, key);
					if (sub != NULL) {
						xc->volume = sub->vol;
						use_ins_vol = 0;
					}
				}
			}
		} else {
			/* Ignore invalid instruments */
			new_invalid_ins = 1;
			xc->flags = 0;
			use_ins_vol = 0;
		}
	}

	/* Check note */

	if (key && !new_invalid_ins) {
		SET(NEW_NOTE);

		if (key == XMP_KEY_FADE) {
			SET_NOTE(NOTE_FADEOUT);
			reset_env = 0;
			use_ins_vol = 0;
		} else if (key == XMP_KEY_CUT) {
			SET_NOTE(NOTE_END);
			xc->period = 0;
			virt_resetchannel(ctx, chn);
		} else if (key == XMP_KEY_OFF) {
			SET_NOTE(NOTE_RELEASE);
			reset_env = 0;
			use_ins_vol = 0;
			if (HAS_QUIRK(QUIRK_PRENV))
				SET_NOTE(NOTE_END);
		} else if (is_toneporta) {

			/* Always retrig on tone portamento: Fix portamento in
			 * 7spirits.s3m, mod.Biomechanoid
			 */
			if (not_same_ins || TEST_NOTE(NOTE_END)) {
				SET(NEW_INS);
				RESET_NOTE(NOTE_RELEASE);
			} else {
				key = 0;
			}

			if (HAS_QUIRK(QUIRK_PRENV) && e->ins)
				reset_envelopes(ctx, xc);
		}
	}

	if ((uint32)key <= XMP_MAX_KEYS && key > 0 && !new_invalid_ins) {
		xc->key = --key;
		RESET_NOTE(NOTE_END);

		sub = get_subinstrument(ctx, candidate_ins, key);

		if (sub != NULL) {
			int transp = mod->xxi[candidate_ins].map[key].xpo;
			int smp;

			note = key + sub->xpo + transp;
			smp = sub->sid;
			if (smp >= mod->smp || mod->xxs[smp].len == 0) {
				smp = -1;
			}

			if (smp >= 0 && smp < mod->smp) {
				int to = virt_setpatch(ctx, chn, candidate_ins,
					smp, note, sub->nna, sub->dct,
					sub->dca);

				if (to < 0)
					return -1;

				if (to != chn) {
					copy_channel(p, to, chn);
					p->xc_data[to].flags = 0;
				}

				xc->smp = smp;
			}
		} else {
			xc->flags = 0;
			use_ins_vol = 0;
		}
	}

	if (IS_VALID_INSTRUMENT(candidate_ins)) {
		xc->ins = candidate_ins;
	}

	sub = get_subinstrument(ctx, xc->ins, xc->key);

	set_effect_defaults(ctx, note, sub, xc, is_toneporta);
	if (note >= 0 && sub != NULL)
		reset_envelopes(ctx, xc);
	
	/* Process new volume */
	if (e->vol) {
		xc->volume = e->vol - 1;
		SET(NEW_VOL);
	}

	/* IT: always reset sample offset */
	xc->offset_val = 0;

	/* According to Storlek test 25, Impulse Tracker handles the volume
	 * column effects last.
	 */
	process_fx(ctx, xc, chn, e->note, e->fxt, e->fxp, 0);
	process_fx(ctx, xc, chn, e->note, e->f2t, e->f2p, 1);

	if (TEST(NEW_VOL))
		use_ins_vol = 0;

	if (sub == NULL) {
		return 0;
	}

	if (note >= 0) {
		xc->note = note;
		virt_voicepos(ctx, chn, xc->offset_val);
		if (TEST(OFFSET))
			xc->offset_val <<= 1;
		RESET(OFFSET);
	}

	if (reset_env) {
		RESET_NOTE(NOTE_RELEASE | NOTE_FADEOUT);
	}

	if (use_ins_vol) {
		xc->volume = sub->vol;
		SET(NEW_VOL);
	}

	return 0;
}

static int read_event_smix(struct context_data *ctx, struct xmp_event *e, int chn)
{
	struct player_data *p = &ctx->p;
	struct smix_data *smix = &ctx->smix;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	struct xmp_subinstrument *sub;
	int is_smix_ins;
	int ins, note, transp, smp;

	xc->flags = 0;
	note = -1;

	if (!e->ins)
		return 0;

	is_smix_ins = 0;
	ins = e->ins - 1;
	SET(NEW_INS);
	xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
	xc->per_flags = 0;
	xc->offset_val = 0;
	RESET_NOTE(NOTE_RELEASE);

	xc->ins = ins;

	if (ins >= mod->ins && ins < mod->ins + smix->ins)
		is_smix_ins = 1;

	SET(NEW_NOTE);

	if (e->note == XMP_KEY_OFF) {
		SET_NOTE(NOTE_RELEASE);
		return 0;
	}

	xc->key = e->note - 1;
	RESET_NOTE(NOTE_END);

	if (is_smix_ins) {
		sub = &smix->xxi[xc->ins - mod->ins].sub[0];
		if (sub == NULL)
			return 0;

		note = xc->key + sub->xpo;
		smp = sub->sid;
		if (smix->xxs[smp].len == 0)
			smp = -1;
		if (smp >= 0 && smp < smix->smp) {
			smp += mod->smp;
			virt_setpatch(ctx, chn, xc->ins, smp, note, 0, 0, 0);
			xc->smp = smp;
		}
	} else {
		transp = mod->xxi[xc->ins].map[xc->key].xpo;
		sub = get_subinstrument(ctx, xc->ins, xc->key);
		note = xc->key + sub->xpo + transp;
		smp = sub->sid;
		if (mod->xxs[smp].len == 0)
			smp = -1;
		if (smp >= 0 && smp < mod->smp) {
			set_patch(ctx, chn, xc->ins, smp, note);
			xc->smp = smp;
		}
	}

	set_effect_defaults(ctx, note, sub, xc, 0);
	if (e->ins && sub != NULL)
		reset_envelopes(ctx, xc);

	xc->volume = e->vol - 1;
	SET(NEW_VOL);

	xc->note = note;
	virt_voicepos(ctx, chn, xc->offset_val);

	return 0;
}

int read_event(struct context_data *ctx, struct xmp_event *e, int chn)
{
	struct module_data *m = &ctx->m;

	if (chn >= m->mod.chn) {
		return read_event_smix(ctx, e, chn);
	} else switch (m->read_event_type) {
	case READ_EVENT_MOD:
		return read_event_mod(ctx, e, chn);
	case READ_EVENT_FT2:
		return read_event_ft2(ctx, e, chn);
	case READ_EVENT_ST3:
		return read_event_st3(ctx, e, chn);
	case READ_EVENT_IT:
		return read_event_it(ctx, e, chn);
	default:
		return read_event_mod(ctx, e, chn);
	}
}
