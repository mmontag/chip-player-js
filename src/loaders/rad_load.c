/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include "load.h"
#include "synth.h"

static int rad_test(FILE *, char *, const int);
static int rad_load(struct xmp_context *, FILE *, const int);

struct format_loader rad_loader = {
	"RAD",
	"Reality Adlib Tracker",
	rad_test,
	rad_load
};

static int rad_test(FILE *f, char *t, const int start)
{
	char buf[16];

	if (fread(buf, 1, 16, f) < 16)
		return -1;

	if (memcmp(buf, "RAD by REALiTY!!", 16))
		return -1;

	read_title(f, t, 0);

	return 0;
}

struct rad_instrument {
	uint8 num;		/* Number of instruments that follows */
	uint8 reg[11];		/* Adlib registers */
};


static int rad_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j;
	uint8 sid[11];
	uint16 ppat[32];
	uint8 b, r, c;
	uint8 version;		/* Version in BCD */
	uint8 flags;		/* bit 7=descr,6=slow-time,4..0=tempo */

	LOAD_INIT();

	fseek(f, 16, SEEK_SET);		/* skip magic */
	version = read8(f);
	flags = read8(f);

	mod->chn = 9;
	mod->bpm = 125;
	mod->tpo = flags & 0x1f;
	mod->flg = XXM_FLG_LINEAR;
	/* FIXME: tempo setting in RAD modules */
	if (mod->tpo <= 2)
		mod->tpo = 6;
	mod->smp = 0;

	set_type(m, "RAD %d.%d", MSN(version), LSN(version));

	MODULE_INFO();

	/* Read description */
	if (flags & 0x80) {
		while ((b = read8(f)) != 0);
	}

	/* Read instruments */
	mod->ins = 0;

	_D(_D_INFO "Read instruments");

	while ((b = read8(f)) != 0) {
		mod->ins = b;

		fread(sid, 1, 11, f);
		xmp_cvt_hsc2sbi((char *)sid);
		load_sample(ctx, f, b - 1, SAMPLE_FLAG_ADLIB, NULL,
								(char *)sid);
	}

	INSTRUMENT_INIT();

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		mod->xxi[i].nsm = 1;
		mod->xxi[i].sub[0].vol = 0x40;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].xpo = -1;
		mod->xxi[i].sub[0].sid = i;
	}

	/* Read orders */
	mod->len = read8(f);

	for (j = i = 0; i < mod->len; i++) {
		b = read8(f);
		if (b < 0x80)
			mod->xxo[j++] = b;
	}

	/* Read pattern pointers */
	for (mod->pat = i = 0; i < 32; i++) {
		ppat[i] = read16l(f);
		if (ppat[i])
			mod->pat++;
	}
	mod->trk = mod->pat * mod->chn;

	_D(_D_INFO "Module length: %d", mod->len);
	_D(_D_INFO "Instruments: %d", mod->ins);
	_D(_D_INFO "Stored patterns: %d", mod->pat);

	PATTERN_INIT();

	/* Read and convert patterns */
	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		if (ppat[i] == 0)
			continue;

		fseek(f, start + ppat[i], SEEK_SET);

		do {
			r = read8(f);		/* Row number */

			if ((r & 0x7f) >= 64) {
				_D(_D_CRIT "** Whoops! row = %d\n", r);
			}

			do {
				c = read8(f);	/* Channel number */

				if ((c & 0x7f) >= mod->chn) {
					_D(_D_CRIT "** Whoops! channel = %d\n", c);
				}

				event = &EVENT(i, c & 0x7f, r & 0x7f);

				b = read8(f);	/* Note + octave + inst */
				event->ins = (b & 0x80) >> 3;
				event->note = LSN(b);

				if (event->note == 15)
					event->note = XMP_KEY_OFF;
				else if (event->note)
					event->note += 14 +
						12 * ((b & 0x70) >> 4);

				b = read8(f);	/* Instrument + effect */
				event->ins |= MSN(b);
				event->fxt = LSN(b);
				if (event->fxt) {
					b = read8(f);	/* Effect parameter */
					event->fxp = b;

					/* FIXME: tempo setting */
					if (event->fxt == 0x0f
					    && event->fxp <= 2)
						event->fxp = 6;
				}
			} while (~c & 0x80);
		} while (~r & 0x80);
	}

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = 0x80;
		mod->xxc[i].flg = XXM_CHANNEL_SYNTH;
	}

	m->synth = &synth_adlib;

	return 0;
}
