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
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	int i, j, k;
	int sliding;
	struct xxm_event *event;
	uint8 buf[40];

	LOAD_INIT();

	if (read32b(f) !=  MAGIC_MED2)
		return -1;

	strcpy(m->type, "MED2 (MED 1.12)");

	m->xxh->ins = m->xxh->smp = 32;
	INSTRUMENT_INIT();

	/* read instrument names */
	fread(buf, 1, 40, f);	/* skip 0 */
	for (i = 0; i < 31; i++) {
		fread(buf, 1, 40, f);
		copy_adjust(m->xxih[i].name, buf, 32);
		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
	}

	/* read instrument volumes */
	read8(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		m->xxi[i][0].vol = read8(f);
		m->xxi[i][0].pan = 0x80;
		m->xxi[i][0].fin = 0;
		m->xxi[i][0].sid = i;
	}

	/* read instrument loops */
	read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		m->xxs[i].lps = read16b(f);
	}

	/* read instrument loop length */
	read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		uint32 lsiz = read16b(f);
		m->xxs[i].lpe = m->xxs[i].lps + lsiz;
		m->xxs[i].flg = lsiz > 1 ? WAVE_LOOPING : 0;
	}

	m->xxh->chn = 4;
	m->xxh->pat = read16b(f);
	m->xxh->trk = m->xxh->chn * m->xxh->pat;

	fread(m->xxo, 1, 100, f);
	m->xxh->len = read16b(f);

	m->xxh->tpo = 192 / read16b(f);

	read16b(f);			/* flags */
	sliding = read16b(f);		/* sliding */
	read32b(f);			/* jumping mask */
	fseek(f, 16, SEEK_CUR);		/* rgb */

	MODULE_INFO();

	reportv(ctx, 0, "Sliding        : %d\n", sliding);

	if (sliding == 6)
		m->quirk |= XMP_QRK_VSALL | XMP_QRK_PBALL;

	PATTERN_INIT();

	/* Load and convert patterns */
	reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

	for (i = 0; i < m->xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->xxp[i]->rows = 64;
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

		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	/* Load samples */

	reportv(ctx, 0, "Instruments    : %d ", m->xxh->ins);
	reportv(ctx, 1, "\n     Instrument name                  Len  LBeg LEnd L Vol");

	for (i = 0; i < 31; i++) {
		char path[PATH_MAX];
		char ins_path[256];
		char name[256];
		FILE *s = NULL;
		struct stat stat;
		int found;
		char c;

		get_instrument_path(ctx, "XMP_MED2_INSTRUMENT_PATH",
				ins_path, 256);
		found = check_filename_case(ins_path,
				(char *)m->xxih[i].name, name, 256);

		c = 'x';
		if (found) {
			snprintf(path, PATH_MAX, "%s/%s", ins_path, name);
			if ((s = fopen(path, "rb"))) {
				fstat(fileno(s), &stat);
				m->xxs[i].len = stat.st_size;
				c = '.';
			}
		}

		m->xxih[i].nsm = !!(m->xxs[i].len);

		if (!strlen((char *)m->xxih[i].name) && !m->xxs[i].len)
			continue;

		reportv(ctx, 1, "\n[%2X] %-32.32s %04x %04x %04x %c V%02x ",
			i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
			m->xxs[i].lpe,
			m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			m->xxi[i][0].vol);

		if (found) {
			xmp_drv_loadpatch(ctx, s, m->xxi[i][0].sid, m->c4rate,
				0, &m->xxs[m->xxi[i][0].sid], NULL);
			fclose(s);
		}

		reportv(ctx, 0, "%c", c);
	}
	reportv(ctx, 0, "\n");

	return 0;
}
