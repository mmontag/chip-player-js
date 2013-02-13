/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "period.h"

#define MAGIC_PSM_	MAGIC4('P','S','M',0xfe)


static int psm_test (FILE *, char *, const int);
static int psm_load (struct module_data *, FILE *, const int);

const struct format_loader psm_loader = {
	"Protracker Studio (PSM)",
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

static int psm_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int c, r, i;
	struct xmp_event *event;
	uint8 buf[1024];
	uint32 p_ord, p_chn, p_pat, p_ins;
	uint32 p_smp[64];
	int type, ver, mode;
 
	LOAD_INIT();

	read32b(f);

	fread(buf, 1, 60, f);
	strncpy(mod->name, (char *)buf, XMP_NAME_SIZE);

	type = read8(f);	/* song type */
	ver = read8(f);		/* song version */
	mode = read8(f);	/* pattern version */

	if (type & 0x01)	/* song mode not supported */
		return -1;

	set_type(m, "Protracker Studio PSM %d.%02d", MSN(ver), LSN(ver));

	mod->spd = read8(f);
	mod->bpm = read8(f);
	read8(f);		/* master volume */
	read16l(f);		/* song length */
	mod->len = read16l(f);
	mod->pat = read16l(f);
	mod->ins = read16l(f);
	mod->chn = read16l(f);
	read16l(f);		/* channels used */
	mod->smp = mod->ins;
	mod->trk = mod->pat * mod->chn;

	p_ord = read32l(f);
	p_chn = read32l(f);
	p_pat = read32l(f);
	p_ins = read32l(f);

	/* should be this way but fails with Silverball song 6 */
	//mod->flg |= ~type & 0x02 ? XXM_FLG_MODRNG : 0;

	MODULE_INFO();

	fseek(f, start + p_ord, SEEK_SET);
	fread(mod->xxo, 1, mod->len, f);

	fseek(f, start + p_chn, SEEK_SET);
	fread(buf, 1, 16, f);

	INSTRUMENT_INIT();

	fseek(f, start + p_ins, SEEK_SET);
	for (i = 0; i < mod->ins; i++) {
		uint16 flags, c2spd;
		int finetune;

		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		fread(buf, 1, 13, f);		/* sample filename */
		fread(buf, 1, 24, f);		/* sample description */
		strncpy((char *)mod->xxi[i].name, (char *)buf, 24);
		str_adj((char *)mod->xxi[i].name);
		p_smp[i] = read32l(f);
		read32l(f);			/* memory location */
		read16l(f);			/* sample number */
		flags = read8(f);		/* sample type */
		mod->xxs[i].len = read32l(f); 
		mod->xxs[i].lps = read32l(f);
		mod->xxs[i].lpe = read32l(f);
		finetune = (int8)(read8(f) << 4);
		mod->xxi[i].sub[0].vol = read8(f);
		c2spd = 8363 * read16l(f) / 8448;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].nsm = !!mod->xxs[i].len;
		mod->xxs[i].flg = flags & 0x80 ? XMP_SAMPLE_LOOP : 0;
		mod->xxs[i].flg |= flags & 0x20 ? XMP_SAMPLE_LOOP_BIDIR : 0;
		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
		mod->xxi[i].sub[0].fin += finetune;

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %5d",
			i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
			mod->xxs[i].lpe, mod->xxs[i].flg & XMP_SAMPLE_LOOP ?
			'L' : ' ', mod->xxi[i].sub[0].vol, c2spd);
	}
	

	PATTERN_INIT();

	D_(D_INFO "Stored patterns: %d", mod->pat);

	fseek(f, start + p_pat, SEEK_SET);
	for (i = 0; i < mod->pat; i++) {
		int len;
		uint8 b, rows, chan;

		len = read16l(f) - 4;
		rows = read8(f);
		chan = read8(f);

		PATTERN_ALLOC (i);
		mod->xxp[i]->rows = rows;
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
					event->note = read8(f) + 36 + 1;
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

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		fseek(f, start + p_smp[i], SEEK_SET);
		load_sample(f, SAMPLE_FLAG_DIFF, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
