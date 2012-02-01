/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Format specs from MTPng 4.3.1 by Ian Schmitd and deMODifier by Bret Victor
 * http://home.cfl.rr.com/ischmidt/warez.html
 * http://worrydream.com/media/demodifier.tgz
 */

/* From the deMODifier readme:
 *
 * SoundSmith was arguably the most popular music authoring tool for the
 * Apple IIgs.  Introduced in the IIgs's heyday (which was, accurately 
 * enough, just about one day), this software inspired the creation
 * of countless numbers of IIgs-specific tunes, several of which were 
 * actually worth listening to.  
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "load.h"
#include "asif.h"


static int mtp_test (FILE *, char *, const int);
static int mtp_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info mtp_loader = {
	"MTP",
	"Soundsmith/MegaTracker",
	mtp_test,
	mtp_load
};

static int mtp_test(FILE *f, char *t, const int start)
{
	char buf[6];

	if (fread(buf, 1, 6, f) < 6)
		return -1;

	if (memcmp(buf, "SONGOK", 6) && memcmp(buf, "IAN92a", 6))
		return -1;

	read_title(f, t, 0);

	return 0;
}




#define NAME_SIZE 255


static int mtp_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_event *event;
	int i, j, k;
	uint8 buffer[25];
	int blocksize;
	FILE *s;

	LOAD_INIT();

	fread(buffer, 6, 1, f);

	if (!memcmp(buffer, "SONGOK", 6))
		strcpy(m->mod.type, "IIgs SoundSmith");
	else if (!memcmp(buffer, "IAN92a", 8))
		strcpy(m->mod.type, "IIgs MegaTracker");
	else
		return -1;

	blocksize = read16l(f);
	m->mod.xxh->tpo = read16l(f);
	fseek(f, 10, SEEK_CUR);		/* skip 10 reserved bytes */
	
	m->mod.xxh->ins = m->mod.xxh->smp = 15;
	INSTRUMENT_INIT();

	for (i = 0; i < m->mod.xxh->ins; i++) {
		m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		fread(buffer, 1, 22, f);
		if (buffer[0]) {
			buffer[buffer[0] + 1] = 0;
			copy_adjust(m->mod.xxi[i].name, buffer + 1, 22);
		}
		read16l(f);		/* skip 2 reserved bytes */
		m->mod.xxi[i].sub[0].vol = read8(f) >> 2;
		m->mod.xxi[i].sub[0].pan = 0x80;
		fseek(f, 5, SEEK_CUR);	/* skip 5 bytes */
	}

	m->mod.xxh->len = read8(f) & 0x7f;
	read8(f);
	fread(m->mod.xxo, 1, 128, f);

	MODULE_INFO();

	fseek(f, start + 600, SEEK_SET);

	m->mod.xxh->chn = 14;
	m->mod.xxh->pat = blocksize / (14 * 64);
	m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;

	PATTERN_INIT();

	/* Read and convert patterns */
	_D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

	/* Load notes */
	for (i = 0; i < m->mod.xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < m->mod.xxp[i]->rows; j++) {
			for (k = 0; k < m->mod.xxh->chn; k++) {
				event = &EVENT(i, k, j);
				event->note = read8(f);;
				if (event->note)
					event->note += 12;
			}
		}
	}

	/* Load fx1 */
	for (i = 0; i < m->mod.xxh->pat; i++) {
		for (j = 0; j < m->mod.xxp[i]->rows; j++) {
			for (k = 0; k < m->mod.xxh->chn; k++) {
				uint8 x;
				event = &EVENT(i, k, j);
				x = read8(f);;
				event->ins = x >> 4;

				switch (x & 0x0f) {
				case 0x00:
					event->fxt = FX_ARPEGGIO;
					break;
				case 0x03:
					event->fxt = FX_VOLSET;
					break;
				case 0x05:
					event->fxt = FX_VOLSLIDE_DN;
					break;
				case 0x06:
					event->fxt = FX_VOLSLIDE_UP;
					break;
				case 0x0f:
					event->fxt = FX_TEMPO;
					break;
				}
			}
		}
	}

	/* Load fx2 */
	for (i = 0; i < m->mod.xxh->pat; i++) {
		for (j = 0; j < m->mod.xxp[i]->rows; j++) {
			for (k = 0; k < m->mod.xxh->chn; k++) {
				event = &EVENT(i, k, j);
				event->fxp = read8(f);;

				switch (event->fxt) {
				case FX_VOLSET:
				case FX_VOLSLIDE_DN:
				case FX_VOLSLIDE_UP:
					event->fxp >>= 2;
				}
			}
		}
	}

	/* Read instrument data */
	_D(_D_INFO "Instruments    : %d ", m->mod.xxh->ins);

	for (i = 0; i < m->mod.xxh->ins; i++) {
		char filename[1024];

		if (!m->mod.xxi[i].name[0])
			continue;

		strncpy(filename, m->dirname, NAME_SIZE);
		if (*filename)
			strncat(filename, "/", NAME_SIZE);
		strncat(filename, (char *)m->mod.xxi[i].name, NAME_SIZE);

		if ((s = fopen(filename, "rb")) != NULL) {
			asif_load(ctx, s, i);
			fclose(s);
		}

#if 0
		m->mod.xxs[i].lps = 0;
		m->mod.xxs[i].lpe = 0;
		m->mod.xxs[i].flg = m->mod.xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
		m->mod.xxi[i].sub[0].fin = 0;
		m->mod.xxi[i].sub[0].pan = 0x80;
#endif

		_D(_D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x", i,
				m->mod.xxi[i].name,
				m->mod.xxs[i].len, m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
				m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				m->mod.xxi[i].sub[0].vol);
	}

	return 0;
}
