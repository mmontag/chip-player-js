/* SoundSmith/MegaTracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: ssmt_load.c,v 1.13 2007-10-17 13:08:49 cmatsuoka Exp $
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


static int mtp_test (FILE *, char *);
static int mtp_load (struct xmp_mod_context *, FILE *, const int);

struct xmp_loader_info mtp_loader = {
	"MTP",
	"Soundsmith/MegaTracker",
	mtp_test,
	mtp_load
};

static int mtp_test(FILE *f, char *t)
{
	char buf[6];

	fread(buf, 6, 1, f);
	if (memcmp(buf, "SONGOK", 6) && memcmp(buf, "IAN92a", 6))
		return -1;

	read_title(f, t, 0);

	return 0;
}




#define NAME_SIZE 255


static void split_name(char *s, char **d, char **b)
{
	if ((*b = strrchr(s, '/'))) {
		**b = 0;
		(*b)++;
		*d = s;
	} else {
		*d = "";
		*b = s;
	}
}


static int mtp_load(struct xmp_mod_context *m, FILE *f, const int start)
{
	struct xxm_event *event;
	int i, j, k;
	uint8 buffer[25];
	int blocksize;
	char *dirname, *basename;
	char filename[NAME_SIZE];
	char modulename[NAME_SIZE];
	FILE *s;

	LOAD_INIT();

	fread(buffer, 6, 1, f);

	if (!memcmp(buffer, "SONGOK", 6))
		strcpy(m->type, "IIgs SoundSmith");
	else if (!memcmp(buffer, "IAN92a", 8))
		strcpy(m->type, "IIgs MegaTracker");
	else
		return -1;

	strncpy(modulename, m->filename, NAME_SIZE);
	split_name(modulename, &dirname, &basename);

	blocksize = read16l(f);
	m->xxh->tpo = read16l(f);
	fseek(f, 10, SEEK_CUR);		/* skip 10 reserved bytes */
	
	m->xxh->ins = m->xxh->smp = 15;
	INSTRUMENT_INIT();

	for (i = 0; i < m->xxh->ins; i++) {
		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		fread(buffer, 1, 22, f);
		if (buffer[0]) {
			buffer[buffer[0] + 1] = 0;
			copy_adjust(m->xxih[i].name, buffer + 1, 22);
		}
		read16l(f);		/* skip 2 reserved bytes */
		m->xxi[i][0].vol = read8(f) >> 2;
		m->xxi[i][0].pan = 0x80;
		fseek(f, 5, SEEK_CUR);	/* skip 5 bytes */
	}

	m->xxh->len = read8(f) & 0x7f;
	read8(f);
	fread(m->xxo, 1, 128, f);

	MODULE_INFO();

	fseek(f, start + 600, SEEK_SET);

	m->xxh->chn = 14;
	m->xxh->pat = blocksize / (14 * 64);
	m->xxh->trk = m->xxh->pat * m->xxh->chn;

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(0, "Stored patterns: %d ", m->xxh->pat);

	/* Load notes */
	for (i = 0; i < m->xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < m->xxp[i]->rows; j++) {
			for (k = 0; k < m->xxh->chn; k++) {
				event = &EVENT(i, k, j);
				event->note = read8(f);;
				if (event->note)
					event->note += 12;
			}
		}
		reportv(0, ".");
	}

	/* Load fx1 */
	for (i = 0; i < m->xxh->pat; i++) {
		for (j = 0; j < m->xxp[i]->rows; j++) {
			for (k = 0; k < m->xxh->chn; k++) {
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
	for (i = 0; i < m->xxh->pat; i++) {
		for (j = 0; j < m->xxp[i]->rows; j++) {
			for (k = 0; k < m->xxh->chn; k++) {
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

	reportv(0, "\n");

	/* Read instrument data */
	reportv(0, "Instruments    : %d ", m->xxh->ins);
	reportv(1, "\n     Name                   Len  LBeg LEnd L Vol");

	for (i = 0; i < m->xxh->ins; i++) {
		if (!m->xxih[i].name[0])
			continue;

		strncpy(filename, dirname, NAME_SIZE);
		if (*filename)
			strncat(filename, "/", NAME_SIZE);
		strncat(filename, (char *)m->xxih[i].name, NAME_SIZE);

		s = fopen(filename, "rb");
		asif_load(m, s, i);
		fclose(s);

#if 0
		m->xxs[i].lps = 0;
		m->xxs[i].lpe = 0;
		m->xxs[i].flg = m->xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
		m->xxi[i][0].fin = 0;
		m->xxi[i][0].pan = 0x80;
#endif

		if (V(1) && (strlen((char*)m->xxih[i].name) || (m->xxs[i].len > 1))) {
			report("\n[%2X] %-22.22s %04x %04x %04x %c V%02x", i,
				m->xxih[i].name,
				m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
				m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				m->xxi[i][0].vol);
		}
	}
	reportv(0, "\n");

	return 0;
}
