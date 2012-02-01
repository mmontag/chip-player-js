/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * MED 1.12 is in Fish disk #255
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __native_client__
#include <sys/syslimits.h>
#else
#include <limits.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "period.h"
#include "load.h"

#define MAGIC_MED2	MAGIC4('M','E','D',2)

static int med2_test(FILE *, char *, const int);
static int med2_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info med2_loader = {
	"MED2",
	"MED 1.12",
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


int med2_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, j, k;
	int sliding;
	struct xxm_event *event;
	uint8 buf[40];

	LOAD_INIT();

	if (read32b(f) !=  MAGIC_MED2)
		return -1;

	strcpy(m->mod.type, "MED2 (MED 1.12)");

	m->mod.xxh->ins = m->mod.xxh->smp = 32;
	INSTRUMENT_INIT();

	/* read instrument names */
	fread(buf, 1, 40, f);	/* skip 0 */
	for (i = 0; i < 31; i++) {
		fread(buf, 1, 40, f);
		copy_adjust(m->mod.xxi[i].name, buf, 32);
		m->mod.xxi[i].sub = calloc(sizeof (struct xxm_subinstrument), 1);
	}

	/* read instrument volumes */
	read8(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		m->mod.xxi[i].sub[0].vol = read8(f);
		m->mod.xxi[i].sub[0].pan = 0x80;
		m->mod.xxi[i].sub[0].fin = 0;
		m->mod.xxi[i].sub[0].sid = i;
	}

	/* read instrument loops */
	read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		m->mod.xxs[i].lps = read16b(f);
	}

	/* read instrument loop length */
	read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		uint32 lsiz = read16b(f);
		m->mod.xxs[i].lpe = m->mod.xxs[i].lps + lsiz;
		m->mod.xxs[i].flg = lsiz > 1 ? XMP_SAMPLE_LOOP : 0;
	}

	m->mod.xxh->chn = 4;
	m->mod.xxh->pat = read16b(f);
	m->mod.xxh->trk = m->mod.xxh->chn * m->mod.xxh->pat;

	fread(m->mod.xxo, 1, 100, f);
	m->mod.xxh->len = read16b(f);

	m->mod.xxh->tpo = 192 / read16b(f);

	read16b(f);			/* flags */
	sliding = read16b(f);		/* sliding */
	read32b(f);			/* jumping mask */
	fseek(f, 16, SEEK_CUR);		/* rgb */

	MODULE_INFO();

	_D(_D_INFO, "Sliding: %d", sliding);

	if (sliding == 6)
		m->quirk |= XMP_QRK_VSALL | XMP_QRK_PBALL;

	PATTERN_INIT();

	/* Load and convert patterns */
	_D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

	for (i = 0; i < m->mod.xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = 64;
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

	_D(_D_INFO "Instruments    : %d ", m->mod.xxh->ins);

	for (i = 0; i < 31; i++) {
		char path[PATH_MAX];
		char ins_path[256];
		char name[256];
		FILE *s = NULL;
		struct stat stat;
		int found;

		get_instrument_path(ctx, "XMP_MED2_INSTRUMENT_PATH",
				ins_path, 256);
		found = check_filename_case(ins_path,
				(char *)m->mod.xxi[i].name, name, 256);

		if (found) {
			snprintf(path, PATH_MAX, "%s/%s", ins_path, name);
			if ((s = fopen(path, "rb"))) {
				fstat(fileno(s), &stat);
				m->mod.xxs[i].len = stat.st_size;
			}
		}

		m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len);

		if (!strlen((char *)m->mod.xxi[i].name) && !m->mod.xxs[i].len)
			continue;

		_D(_D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x",
			i, m->mod.xxi[i].name, m->mod.xxs[i].len, m->mod.xxs[i].lps,
			m->mod.xxs[i].lpe,
			m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			m->mod.xxi[i].sub[0].vol);

		if (found) {
			xmp_drv_loadpatch(ctx, s, m->mod.xxi[i].sub[0].sid,
				0, &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
			fclose(s);
		}
	}

	return 0;
}
