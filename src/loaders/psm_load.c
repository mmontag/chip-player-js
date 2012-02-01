/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
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


static int psm_test (FILE *, char *, const int);
static int psm_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info psm_loader = {
	"PSM",
	"Protracker Studio",
	psm_test,
	psm_load
};

static int psm_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != MAGIC_PSM_)
		return -1;

	read_title(f, t, 60);

	return 0;
}


/* FIXME: effects translation */

static int psm_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	int c, r, i;
	struct xxm_event *event;
	uint8 buf[1024];
	uint32 p_ord, p_chn, p_pat, p_ins;
	uint32 p_smp[64];
	int type, ver, mode;
 
	LOAD_INIT();

	read32b(f);

	fread(buf, 1, 60, f);
	strncpy(m->mod.name, (char *)buf, XMP_NAMESIZE);

	type = read8(f);	/* song type */
	ver = read8(f);		/* song version */
	mode = read8(f);	/* pattern version */

	if (type & 0x01)	/* song mode not supported */
		return -1;

	set_type(m, "PSM %d.%02d (Protracker Studio)",
						MSN(ver), LSN(ver));

	m->mod.xxh->tpo = read8(f);
	m->mod.xxh->bpm = read8(f);
	read8(f);		/* master volume */
	read16l(f);		/* song length */
	m->mod.xxh->len = read16l(f);
	m->mod.xxh->pat = read16l(f);
	m->mod.xxh->ins = read16l(f);
	m->mod.xxh->chn = read16l(f);
	read16l(f);		/* channels used */
	m->mod.xxh->smp = m->mod.xxh->ins;
	m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;

	p_ord = read32l(f);
	p_chn = read32l(f);
	p_pat = read32l(f);
	p_ins = read32l(f);

	/* should be this way but fails with Silverball song 6 */
	//m->mod.xxh->flg |= ~type & 0x02 ? XXM_FLG_MODRNG : 0;

	MODULE_INFO();

	fseek(f, start + p_ord, SEEK_SET);
	fread(m->mod.xxo, 1, m->mod.xxh->len, f);

	fseek(f, start + p_chn, SEEK_SET);
	fread(buf, 1, 16, f);

	INSTRUMENT_INIT();

	fseek(f, start + p_ins, SEEK_SET);
	for (i = 0; i < m->mod.xxh->ins; i++) {
		uint16 flags, c2spd;
		int finetune;

		m->mod.xxi[i].sub = calloc(sizeof (struct xxm_subinstrument), 1);

		fread(buf, 1, 13, f);		/* sample filename */
		fread(buf, 1, 24, f);		/* sample description */
		strncpy((char *)m->mod.xxi[i].name, (char *)buf, 24);
		str_adj((char *)m->mod.xxi[i].name);
		p_smp[i] = read32l(f);
		read32l(f);			/* memory location */
		read16l(f);			/* sample number */
		flags = read8(f);		/* sample type */
		m->mod.xxs[i].len = read32l(f); 
		m->mod.xxs[i].lps = read32l(f);
		m->mod.xxs[i].lpe = read32l(f);
		finetune = (int8)(read8(f) << 4);
		m->mod.xxi[i].sub[0].vol = read8(f);
		c2spd = 8363 * read16l(f) / 8448;
		m->mod.xxi[i].sub[0].pan = 0x80;
		m->mod.xxi[i].sub[0].sid = i;
		m->mod.xxi[i].nsm = !!m->mod.xxs[i].len;
		m->mod.xxs[i].flg = flags & 0x80 ? XMP_SAMPLE_LOOP : 0;
		m->mod.xxs[i].flg |= flags & 0x20 ? XMP_SAMPLE_LOOP_BIDIR : 0;
		c2spd_to_note(c2spd, &m->mod.xxi[i].sub[0].xpo, &m->mod.xxi[i].sub[0].fin);
		m->mod.xxi[i].sub[0].fin += finetune;

		_D(_D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %5d",
			i, m->mod.xxi[i].name, m->mod.xxs[i].len, m->mod.xxs[i].lps,
			m->mod.xxs[i].lpe, m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ?
			'L' : ' ', m->mod.xxi[i].sub[0].vol, c2spd);
	}
	

	PATTERN_INIT();

	_D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

	fseek(f, start + p_pat, SEEK_SET);
	for (i = 0; i < m->mod.xxh->pat; i++) {
		int len;
		uint8 b, rows, chan;

		len = read16l(f) - 4;
		rows = read8(f);
		chan = read8(f);

		PATTERN_ALLOC (i);
		m->mod.xxp[i]->rows = rows;
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
	}

	/* Read samples */

	_D(_D_INFO "Stored samples: %d", m->mod.xxh->smp);

	for (i = 0; i < m->mod.xxh->ins; i++) {
		fseek(f, start + p_smp[i], SEEK_SET);
		xmp_drv_loadpatch(ctx, f, m->mod.xxi[i].sub[0].sid,
			XMP_SMP_DIFF, &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
