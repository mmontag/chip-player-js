/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * MED 1.12 is in Fish disk #255
 */

#ifdef __native_client__
#include <sys/syslimits.h>
#else
#include <limits.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "period.h"
#include "loader.h"

#define MAGIC_MED2	MAGIC4('M','E','D',2)

static int med2_test(FILE *, char *, const int);
static int med2_load (struct module_data *, FILE *, const int);

const struct format_loader med2_loader = {
	"MED 1.12 MED2 (MED)",
	med2_test,
	med2_load
};


static int med2_test(FILE *f, char *t, const int start)
{
	if (read32b(f) !=  MAGIC_MED2)
		return -1;

        read_title(f, t, 0);

        return 0;
}


int med2_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	int sliding;
	struct xmp_event *event;
	uint8 buf[40];

	LOAD_INIT();

	if (read32b(f) !=  MAGIC_MED2)
		return -1;

	set_type(m, "MED 1.12 MED2");

	mod->ins = mod->smp = 32;
	INSTRUMENT_INIT();

	/* read instrument names */
	fread(buf, 1, 40, f);	/* skip 0 */
	for (i = 0; i < 31; i++) {
		fread(buf, 1, 40, f);
		copy_adjust(mod->xxi[i].name, buf, 32);
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	}

	/* read instrument volumes */
	read8(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		mod->xxi[i].sub[0].vol = read8(f);
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].fin = 0;
		mod->xxi[i].sub[0].sid = i;
	}

	/* read instrument loops */
	read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		mod->xxs[i].lps = read16b(f);
	}

	/* read instrument loop length */
	read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		uint32 lsiz = read16b(f);
		mod->xxs[i].lpe = mod->xxs[i].lps + lsiz;
		mod->xxs[i].flg = lsiz > 1 ? XMP_SAMPLE_LOOP : 0;
	}

	mod->chn = 4;
	mod->pat = read16b(f);
	mod->trk = mod->chn * mod->pat;

	fread(mod->xxo, 1, 100, f);
	mod->len = read16b(f);

	mod->spd = 192 / read16b(f);

	read16b(f);			/* flags */
	sliding = read16b(f);		/* sliding */
	read32b(f);			/* jumping mask */
	fseek(f, 16, SEEK_CUR);		/* rgb */

	MODULE_INFO();

	D_(D_INFO "Sliding: %d", sliding);

	if (sliding == 6)
		m->quirk |= QUIRK_VSALL | QUIRK_PBALL;

	PATTERN_INIT();

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		read32b(f);

		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				uint8 x;
				event = &EVENT(i, k, j);
				event->note = period_to_note(read16b(f));
				x = read8(f);
				event->ins = x >> 4;
				event->fxt = x & 0x0f;
				event->fxp = read8(f);

				switch (event->fxt) {
				case 0x00:		/* arpeggio */
				case 0x01:		/* slide up */
				case 0x02:		/* slide down */
				case 0x0c:		/* volume */
					break;		/* ...like protracker */
				case 0x03:
					event->fxt = FX_VIBRATO;
					break;
				case 0x0d:		/* volslide */
				case 0x0e:		/* volslide */
					event->fxt = FX_VOLSLIDE;
					break;
				case 0x0f:
					event->fxt = 192 / event->fxt;
					break;
				}
			}
		}
	}

	/* Load samples */

	D_(D_INFO "Instruments    : %d ", mod->ins);

	for (i = 0; i < 31; i++) {
		char path[PATH_MAX];
		char ins_path[256];
		char name[256];
		FILE *s = NULL;
		struct stat stat;
		int found;

		get_instrument_path(m, ins_path, 256);
		found = check_filename_case(ins_path,
				(char *)mod->xxi[i].name, name, 256);

		if (found) {
			snprintf(path, PATH_MAX, "%s/%s", ins_path, name);
			if ((s = fopen(path, "rb"))) {
				fstat(fileno(s), &stat);
				mod->xxs[i].len = stat.st_size;
			}
		}

		mod->xxi[i].nsm = !!(mod->xxs[i].len);

		if (!strlen((char *)mod->xxi[i].name) && !mod->xxs[i].len)
			continue;

		D_(D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x",
			i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
			mod->xxs[i].lpe,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			mod->xxi[i].sub[0].vol);

		if (found) {
			load_sample(s, 0, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
			fclose(s);
		}
	}

	return 0;
}
