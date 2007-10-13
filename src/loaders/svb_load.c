/* Protracker Studio PSM loader for xmp
 * Copyright (C) 2005-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: svb_load.c,v 1.11 2007-10-13 20:52:19 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"

#define MAGIC_PSM_	MAGIC4('P','S','M',0xfe)


static int svb_test (FILE *, char *);
static int svb_load (FILE *);

struct xmp_loader_info svb_loader = {
	"PSM",
	"Protracker Studio",
	svb_test,
	svb_load
};

static int svb_test(FILE *f, char *t)
{
	if (read32b(f) != MAGIC_PSM_)
		return -1;

	read_title(f, t, 60);

	return 0;
}


/* FIXME: effects translation */

int svb_load(FILE *f)
{
	int c, r, i;
	struct xxm_event *event;
	uint8 buf[1024];
	uint32 p_ord, p_chn, p_pat, p_ins;
	uint32 p_smp[64];
	int type, ver, mode;
 
	LOAD_INIT ();

	read32b(f);

	fread(buf, 1, 60, f);
	strncpy(xmp_ctl->name, (char *)buf, XMP_DEF_NAMESIZE);

	type = read8(f);	/* song type */
	ver = read8(f);		/* song version */
	mode = read8(f);	/* pattern version */

	if (type & 0x01)	/* song mode not supported */
		return -1;

	sprintf(xmp_ctl->type, "PSM %d.%02d (Protracker Studio)",
						MSN(ver), LSN(ver));

	xxh->tpo = read8(f);
	xxh->bpm = read8(f);
	read8(f);		/* master volume */
	read16l(f);		/* song length */
	xxh->len = read16l(f);
	xxh->pat = read16l(f);
	xxh->ins = read16l(f);
	xxh->chn = read16l(f);
	read16l(f);		/* channels used */
	xxh->smp = xxh->ins;
	xxh->trk = xxh->pat * xxh->chn;

	p_ord = read32l(f);
	p_chn = read32l(f);
	p_pat = read32l(f);
	p_ins = read32l(f);

	/* should be this way but fails with Silverball song 6 */
	//xxh->flg |= ~type & 0x02 ? XXM_FLG_MODRNG : 0;

	MODULE_INFO ();

	fseek(f, p_ord, SEEK_SET);
	fread(xxo, 1, xxh->len, f);

	fseek(f, p_chn, SEEK_SET);
	fread(buf, 1, 16, f);

	INSTRUMENT_INIT ();

	reportv(1, "     Sample name           Len   LBeg LEnd L Vol C2Spd\n");

	fseek(f, p_ins, SEEK_SET);
	for (i = 0; i < xxh->ins; i++) {
		uint16 flags, c2spd;
		int finetune;

		xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

		fread(buf, 1, 13, f);		/* sample filename */
		fread(buf, 1, 24, f);		/* sample description */
		strncpy((char *)xxih[i].name, (char *)buf, 24);
		str_adj((char *)xxih[i].name);
		p_smp[i] = read32l(f);
		read32l(f);			/* memory location */
		read16l(f);			/* sample number */
		flags = read8(f);		/* sample type */
		xxs[i].len = read32l(f); 
		xxs[i].lps = read32l(f);
		xxs[i].lpe = read32l(f);
		finetune = (int8)(read8(f) << 4);
		xxi[i][0].vol = read8(f);
		c2spd = 8363 * read16l(f) / 8448;
		xxi[i][0].pan = 0x80;
		xxi[i][0].sid = i;
		xxih[i].nsm = !!xxs[i].len;
		xxs[i].flg = flags & 0x80 ? WAVE_LOOPING : 0;
		xxs[i].flg |= flags & 0x20 ? WAVE_BIDIR_LOOP : 0;
		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
		xxi[i][0].fin += finetune;

		if (V(1) && (strlen((char *)xxih[i].name) || (xxs[i].len > 1))) {
			report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %5d\n",
				i, xxih[i].name, xxs[i].len, xxs[i].lps,
				xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ?
				'L' : ' ', xxi[i][0].vol, c2spd);
		}
	}
	

	PATTERN_INIT ();

	reportv(0, "Stored patterns: %d ", xxh->pat);

	fseek(f, p_pat, SEEK_SET);
	for (i = 0; i < xxh->pat; i++) {
		int len;
		uint8 b, rows, chan;

		len = read16l(f) - 4;
		rows = read8(f);
		chan = read8(f);

		PATTERN_ALLOC (i);
		xxp[i]->rows = rows;
		TRACK_ALLOC (i);

		for (r = 0; r < rows; r++) {
			while (len > 0) {
				b = read8(f);
				len--;

				if (b == 0)
					break;
	
				c = b & 0x0f;
				event = &EVENT(i, c, r);
	
				if (b & 0x80) {
					event->note = read8(f) + 24 + 1;
					event->ins = read8(f);
					len -= 2;
				}
	
				if (b & 0x40) {
					event->vol = read8(f) + 1;
					len--;
				}
	
				if (b & 0x20) {
					event->fxt = read8(f);
					event->fxp = read8(f);
					len -= 2;
/* printf("p%d r%d c%d: %02x %02x\n", i, r, c, event->fxt, event->fxp); */
				}
			}
		}

		if (len > 0)
			fseek(f, len, SEEK_CUR);

		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Read samples */

	reportv(0, "Stored samples : %d ", xxh->smp);

	for (i = 0; i < xxh->ins; i++) {
		fseek(f, p_smp[i], SEEK_SET);
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
			XMP_SMP_DIFF, &xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}

