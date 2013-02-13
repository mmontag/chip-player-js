/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * Based on AMF->MOD converter written by Mr. P / Powersource, 1995
 */

#include "loader.h"
#include "period.h"

static int asylum_test(FILE *, char *, const int);
static int asylum_load(struct module_data *, FILE *, const int);

const struct format_loader asylum_loader = {
	"Asylum Music Format (AMF)",
	asylum_test,
	asylum_load
};

static int asylum_test(FILE *f, char *t, const int start)
{
	char buf[32];

	if (fread(buf, 1, 32, f) < 32)
		return -1;

	if (memcmp(buf, "ASYLUM Music Format V1.0\0\0\0\0\0\0\0\0", 32))
		return -1;

	read_title(f, t, 0);

	return 0;
}

static int asylum_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j;

	LOAD_INIT();

	fseek(f, 32, SEEK_CUR);			/* skip magic */
	mod->spd = read8(f);			/* initial speed */
	mod->bpm = read8(f);			/* initial BPM */
	mod->ins = read8(f);			/* number of instruments */
	mod->pat = read8(f);			/* number of patterns */
	mod->len = read8(f);			/* module length */
	read8(f);

	fread(mod->xxo, 1, mod->len, f);	/* read orders */
	fseek(f, start + 294, SEEK_SET);

	mod->chn = 8;
	mod->smp = mod->ins;
	mod->trk = mod->pat * mod->chn;

	snprintf(mod->type, XMP_NAME_SIZE, "Asylum Music Format V1.0");

	MODULE_INFO();

	INSTRUMENT_INIT();

	/* Read and convert instruments and samples */
	for (i = 0; i < mod->ins; i++) {
		uint8 insbuf[37];

		mod->xxi[i].sub = calloc(sizeof(struct xmp_subinstrument), 1);

		fread(insbuf, 1, 37, f);
		copy_adjust(mod->xxi[i].name, insbuf, 22);
		mod->xxi[i].sub[0].fin = (int8)(insbuf[22] << 4);
		mod->xxi[i].sub[0].vol = insbuf[23];
		mod->xxi[i].sub[0].xpo = (int8)insbuf[24];
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;

		mod->xxs[i].len = readmem32l(insbuf + 25);
		mod->xxs[i].lps = readmem32l(insbuf + 29);
		mod->xxs[i].lpe = mod->xxs[i].lps + readmem32l(insbuf + 33);
		
		mod->xxi[i].nsm = !!mod->xxs[i].len;
		mod->xxs[i].flg = mod->xxs[i].lpe > 2 ? XMP_SAMPLE_LOOP : 0;

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %d", i,
		   mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		   mod->xxs[i].lpe,
		   mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		   mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin);
	}

	fseek(f, 37 * (64 - mod->ins), SEEK_CUR);

	D_(D_INFO "Module length: %d", mod->len);

	PATTERN_INIT();

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < 64 * mod->chn; j++) {
			event = &EVENT(i, j % mod->chn, j / mod->chn);
			memset(event, 0, sizeof(struct xmp_event));
			uint8 note = read8(f);

			if (note != 0) {
				event->note = note + 13;
			}

			event->ins = read8(f);
			event->fxt = read8(f);
			event->fxp = read8(f);
		}
	}

	/* Read samples */
	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxs[i].len > 1) {
			load_sample(f, 0, &mod->xxs[i], NULL);
		} else {
			mod->xxi[i].nsm = 0;
		}
	}

	return 0;
}
