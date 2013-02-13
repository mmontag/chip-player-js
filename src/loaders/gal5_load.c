/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <unistd.h>
#include <limits.h>
#include "loader.h"
#include "iff.h"
#include "period.h"

/* Galaxy Music System 5.0 module file loader
 *
 * Based on the format description by Dr.Eggman
 * (http://www.jazz2online.com/J2Ov2/articles/view.php?articleID=288)
 * and Jazz Jackrabbit modules by Alexander Brandon from Lori Central
 * (http://www.loricentral.com/jj2music.html)
 */

static int gal5_test(FILE *, char *, const int);
static int gal5_load(struct module_data *, FILE *, const int);

const struct format_loader gal5_loader = {
	"Galaxy Music System 5.0 (J2B)",
	gal5_test,
	gal5_load
};


struct local_data {
    uint8 chn_pan[64];
};

static int gal5_test(FILE *f, char *t, const int start)
{
        if (read32b(f) != MAGIC4('R', 'I', 'F', 'F'))
		return -1;

	read32b(f);

	if (read32b(f) != MAGIC4('A', 'M', ' ', ' '))
		return -1;

	if (read32b(f) != MAGIC4('I', 'N', 'I', 'T'))
		return -1;

	read32b(f);		/* skip size */
	read_title(f, t, 64);

	return 0;
}

static void get_init(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	char buf[64];
	int flags;
	
	fread(buf, 1, 64, f);
	strncpy(mod->name, buf, 64);
	set_type(m, "Galaxy Music System 5.0");
	flags = read8(f);	/* bit 0: Amiga period */
	if (~flags & 0x01)
		m->quirk |= QUIRK_LINEAR;
	mod->chn = read8(f);
	mod->spd = read8(f);
	mod->bpm = read8(f);
	read16l(f);		/* unknown - 0x01c5 */
	read16l(f);		/* unknown - 0xff00 */
	read8(f);		/* unknown - 0x80 */
	fread(data->chn_pan, 1, 64, f);
}

static void get_ordr(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	mod->len = read8(f) + 1;
	/* Don't follow Dr.Eggman's specs here */

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = read8(f);
}

static void get_patt_cnt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	i = read8(f) + 1;	/* pattern number */

	if (i > mod->pat)
		mod->pat = i;
}

static void get_inst_cnt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	read32b(f);		/* 42 01 00 00 */
	read8(f);		/* 00 */
	i = read8(f) + 1;	/* instrument number */

	if (i > mod->ins)
		mod->ins = i;
}

static void get_patt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event, dummy;
	int i, len, chan;
	int rows, r;
	uint8 flag;
	
	i = read8(f);	/* pattern number */
	len = read32l(f);
	
	rows = read8(f) + 1;

	PATTERN_ALLOC(i);
	mod->xxp[i]->rows = rows;
	TRACK_ALLOC(i);

	for (r = 0; r < rows; ) {
		if ((flag = read8(f)) == 0) {
			r++;
			continue;
		}

		chan = flag & 0x1f;

		event = chan < mod->chn ? &EVENT(i, chan, r) : &dummy;

		if (flag & 0x80) {
			uint8 fxp = read8(f);
			uint8 fxt = read8(f);

			switch (fxt) {
			case 0x14:		/* speed */
				fxt = FX_S3M_SPEED;
				break;
			default:
				if (fxt > 0x0f) {
					printf("unknown effect %02x %02x\n", fxt, fxp);
					fxt = fxp = 0;
				}
			}

			event->fxt = fxt;
			event->fxp = fxp;
		}

		if (flag & 0x40) {
			event->ins = read8(f);
			event->note = read8(f);

			if (event->note == 128) {
				event->note = XMP_KEY_OFF;
			}
		}

		if (flag & 0x20) {
			event->vol = 1 + read8(f) / 2;
		}
	}
}

static void get_inst(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, srate, finetune, flags;
	int has_unsigned_sample;

	read32b(f);		/* 42 01 00 00 */
	read8(f);		/* 00 */
	i = read8(f);		/* instrument number */
	
	fread(&mod->xxi[i].name, 1, 28, f);
	str_adj((char *)mod->xxi[i].name);

	fseek(f, 290, SEEK_CUR);	/* Sample/note map, envelopes */
	mod->xxi[i].nsm = read16l(f);

	D_(D_INFO "[%2X] %-28.28s  %2d ", i, mod->xxi[i].name, mod->xxi[i].nsm);

	if (mod->xxi[i].nsm == 0)
		return;

	mod->xxi[i].sub = calloc(sizeof(struct xmp_subinstrument), mod->xxi[i].nsm);

	/* FIXME: Currently reading only the first sample */

	read32b(f);	/* RIFF */
	read32b(f);	/* size */
	read32b(f);	/* AS   */
	read32b(f);	/* SAMP */
	read32b(f);	/* size */
	read32b(f);	/* unknown - usually 0x40000000 */

	fread(&mod->xxs[i].name, 1, 28, f);
	str_adj((char *)mod->xxs[i].name);

	read32b(f);	/* unknown - 0x0000 */
	read8(f);	/* unknown - 0x00 */

	mod->xxi[i].sub[0].sid = i;
	mod->xxi[i].vol = read8(f);
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].vol = (read16l(f) + 1) / 512;
	flags = read16l(f);
	read16l(f);			/* unknown - 0x0080 */
	mod->xxs[i].len = read32l(f);
	mod->xxs[i].lps = read32l(f);
	mod->xxs[i].lpe = read32l(f);

	mod->xxs[i].flg = 0;
	has_unsigned_sample = 0;
	if (flags & 0x04)
		mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
	if (flags & 0x08)
		mod->xxs[i].flg |= XMP_SAMPLE_LOOP;
	if (flags & 0x10)
		mod->xxs[i].flg |= XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR;
	if (~flags & 0x80)
		has_unsigned_sample = 1;

	srate = read32l(f);
	finetune = 0;
	c2spd_to_note(srate, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
	mod->xxi[i].sub[0].fin += finetune;

	read32l(f);			/* 0x00000000 */
	read32l(f);			/* unknown */

	D_(D_INFO "  %x: %05x%c%05x %05x %c V%02x %04x %5d",
		0, mod->xxs[i].len,
		mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		mod->xxs[i].lps,
		mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : 
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, flags, srate);

	if (mod->xxs[i].len > 1) {
		load_sample(f, has_unsigned_sample ?
			SAMPLE_FLAG_UNS : 0, &mod->xxs[i], NULL);
	}
}

static int gal5_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	iff_handle handle;
	int i, offset;
	struct local_data data;

	LOAD_INIT();

	read32b(f);	/* Skip RIFF */
	read32b(f);	/* Skip size */
	read32b(f);	/* Skip AM   */

	offset = ftell(f);

	mod->smp = mod->ins = 0;

	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	iff_register(handle, "INIT", get_init);		/* Galaxy 5.0 */
	iff_register(handle, "ORDR", get_ordr);
	iff_register(handle, "PATT", get_patt_cnt);
	iff_register(handle, "INST", get_inst_cnt);
	iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	iff_set_quirk(handle, IFF_SKIP_EMBEDDED);
	iff_set_quirk(handle, IFF_CHUNK_ALIGN2);

	/* Load IFF chunks */
	while (!feof(f)) {
		iff_chunk(handle, m, f, &data);
	}

	iff_release(handle);

	mod->trk = mod->pat * mod->chn;
	mod->smp = mod->ins;

	MODULE_INFO();
	INSTRUMENT_INIT();
	PATTERN_INIT();

	D_(D_INFO "Stored patterns: %d", mod->pat);
	D_(D_INFO "Stored samples: %d ", mod->smp);

	fseek(f, start + offset, SEEK_SET);

	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	iff_register(handle, "PATT", get_patt);
	iff_register(handle, "INST", get_inst);
	iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	iff_set_quirk(handle, IFF_SKIP_EMBEDDED);
	iff_set_quirk(handle, IFF_CHUNK_ALIGN2);

	/* Load IFF chunks */
	while (!feof (f)) {
		iff_chunk(handle, m, f, &data);
	}

	iff_release(handle);

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = data.chn_pan[i] * 2;
	}

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
