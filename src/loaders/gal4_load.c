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

/* Galaxy Music System 4.0 module file loader
 *
 * Based on modules converted using mod2j2b.exe
 */

static int gal4_test(FILE *, char *, const int);
static int gal4_load(struct module_data *, FILE *, const int);

const struct format_loader gal4_loader = {
	"Galaxy Music System 4.0",
	gal4_test,
	gal4_load
};

static int gal4_test(FILE *f, char *t, const int start)
{
        if (read32b(f) != MAGIC4('R', 'I', 'F', 'F'))
		return -1;

	read32b(f);

	if (read32b(f) != MAGIC4('A', 'M', 'F', 'F'))
		return -1;

	if (read32b(f) != MAGIC4('M', 'A', 'I', 'N'))
		return -1;

	read32b(f);		/* skip size */
	read_title(f, t, 64);

	return 0;
}

struct local_data {
    int snum;
};

static void get_main(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	char buf[64];
	int flags;
	
	fread(buf, 1, 64, f);
	strncpy(mod->name, buf, 64);
	set_type(m, "Galaxy Music System 4.0");

	flags = read8(f);
	if (~flags & 0x01)
		m->quirk = QUIRK_LINEAR;
	mod->chn = read8(f);
	mod->spd = read8(f);
	mod->bpm = read8(f);
	read16l(f);		/* unknown - 0x01c5 */
	read16l(f);		/* unknown - 0xff00 */
	read8(f);		/* unknown - 0x80 */
}

static void get_ordr(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	mod->len = read8(f);

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = read8(f);
}

static void get_patt_cnt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	i = read8(f) + 1;		/* pattern number */

	if (i > mod->pat)
		mod->pat = i;
}

static void get_inst_cnt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	read8(f);			/* 00 */
	i = read8(f) + 1;		/* instrument number */
	
	if (i > mod->ins)
		mod->ins = i;

	fseek(f, 28, SEEK_CUR);		/* skip name */

	mod->smp += read8(f);
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
	struct local_data *data = (struct local_data *)parm;
	int i, j;
	int srate, finetune, flags;
	int val, vwf, vra, vde, vsw, fade;
	uint8 buf[30];

	read8(f);		/* 00 */
	i = read8(f);		/* instrument number */

	fread(&mod->xxi[i].name, 1, 28, f);
	str_adj((char *)mod->xxi[i].name);

	mod->xxi[i].nsm = read8(f);

	for (j = 0; j < 108; j++) {
		mod->xxi[i].map[j].ins = read8(f);
	}

	fseek(f, 11, SEEK_CUR);		/* unknown */
	vwf = read8(f);			/* vibrato waveform */
	vsw = read8(f);			/* vibrato sweep */
	read8(f);			/* unknown */
	read8(f);			/* unknown */
	vde = read8(f) / 4;		/* vibrato depth */
	vra = read16l(f) / 16;		/* vibrato speed */
	read8(f);			/* unknown */

	val = read8(f);			/* PV envelopes flags */
	if (LSN(val) & 0x01)
		mod->xxi[i].aei.flg |= XMP_ENVELOPE_ON;
	if (LSN(val) & 0x02)
		mod->xxi[i].aei.flg |= XMP_ENVELOPE_SUS;
	if (LSN(val) & 0x04)
		mod->xxi[i].aei.flg |= XMP_ENVELOPE_LOOP;
	if (MSN(val) & 0x01)
		mod->xxi[i].pei.flg |= XMP_ENVELOPE_ON;
	if (MSN(val) & 0x02)
		mod->xxi[i].pei.flg |= XMP_ENVELOPE_SUS;
	if (MSN(val) & 0x04)
		mod->xxi[i].pei.flg |= XMP_ENVELOPE_LOOP;

	val = read8(f);			/* PV envelopes points */
	mod->xxi[i].aei.npt = LSN(val) + 1;
	mod->xxi[i].pei.npt = MSN(val) + 1;

	val = read8(f);			/* PV envelopes sustain point */
	mod->xxi[i].aei.sus = LSN(val);
	mod->xxi[i].pei.sus = MSN(val);

	val = read8(f);			/* PV envelopes loop start */
	mod->xxi[i].aei.lps = LSN(val);
	mod->xxi[i].pei.lps = MSN(val);

	read8(f);			/* PV envelopes loop end */
	mod->xxi[i].aei.lpe = LSN(val);
	mod->xxi[i].pei.lpe = MSN(val);

	if (mod->xxi[i].aei.npt <= 0 || mod->xxi[i].aei.npt >= XMP_MAX_ENV_POINTS)
		mod->xxi[i].aei.flg &= ~XMP_ENVELOPE_ON;

	if (mod->xxi[i].pei.npt <= 0 || mod->xxi[i].pei.npt >= XMP_MAX_ENV_POINTS)
		mod->xxi[i].pei.flg &= ~XMP_ENVELOPE_ON;

	fread(buf, 1, 30, f);		/* volume envelope points */;
	for (j = 0; j < mod->xxi[i].aei.npt; j++) {
		mod->xxi[i].aei.data[j * 2] = readmem16l(buf + j * 3) / 16;
		mod->xxi[i].aei.data[j * 2 + 1] = buf[j * 3 + 2];
	}

	fread(buf, 1, 30, f);		/* pan envelope points */;
	for (j = 0; j < mod->xxi[i].pei.npt; j++) {
		mod->xxi[i].pei.data[j * 2] = readmem16l(buf + j * 3) / 16;
		mod->xxi[i].pei.data[j * 2 + 1] = buf[j * 3 + 2];
	}

	fade = read8(f);		/* fadeout - 0x80->0x02 0x310->0x0c */
	read8(f);			/* unknown */

	D_(D_INFO "[%2X] %-28.28s  %2d ", i, mod->xxi[i].name, mod->xxi[i].nsm);

	if (mod->xxi[i].nsm == 0)
		return;

	mod->xxi[i].sub = calloc(sizeof(struct xmp_subinstrument), mod->xxi[i].nsm);

	for (j = 0; j < mod->xxi[i].nsm; j++, data->snum++) {
		read32b(f);	/* SAMP */
		read32b(f);	/* size */
	
		fread(&mod->xxs[data->snum].name, 1, 28, f);
		str_adj((char *)mod->xxs[data->snum].name);
	
		mod->xxi[i].sub[j].pan = read8(f) * 4;
		if (mod->xxi[i].sub[j].pan == 0)	/* not sure about this */
			mod->xxi[i].sub[j].pan = 0x80;
		
		mod->xxi[i].sub[j].vol = read8(f);
		flags = read8(f);
		read8(f);	/* unknown - 0x80 */

		mod->xxi[i].sub[j].vwf = vwf;
		mod->xxi[i].sub[j].vde = vde;
		mod->xxi[i].sub[j].vra = vra;
		mod->xxi[i].sub[j].vsw = vsw;
		mod->xxi[i].sub[j].sid = data->snum;
	
		mod->xxs[data->snum].len = read32l(f);
		mod->xxs[data->snum].lps = read32l(f);
		mod->xxs[data->snum].lpe = read32l(f);
	
		mod->xxs[data->snum].flg = 0;
		if (flags & 0x04)
			mod->xxs[data->snum].flg |= XMP_SAMPLE_16BIT;
		if (flags & 0x08)
			mod->xxs[data->snum].flg |= XMP_SAMPLE_LOOP;
		if (flags & 0x10)
			mod->xxs[data->snum].flg |= XMP_SAMPLE_LOOP_BIDIR;
		/* if (flags & 0x80)
			mod->xxs[data->snum].flg |= ? */
	
		srate = read32l(f);
		finetune = 0;
		c2spd_to_note(srate, &mod->xxi[i].sub[j].xpo, &mod->xxi[i].sub[j].fin);
		mod->xxi[i].sub[j].fin += finetune;
	
		read32l(f);			/* 0x00000000 */
		read32l(f);			/* unknown */
	
		D_(D_INFO "  %X: %05x%c%05x %05x %c V%02x P%02x %5d",
			j, mod->xxs[data->snum].len,
			mod->xxs[data->snum].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			mod->xxs[data->snum].lps,
			mod->xxs[data->snum].lpe,
			mod->xxs[data->snum].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : 
			mod->xxs[data->snum].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			mod->xxi[i].sub[j].vol,
			mod->xxi[i].sub[j].pan,
			srate);
	
		if (mod->xxs[data->snum].len > 1) {
			load_sample(f, 0, &mod->xxs[data->snum], NULL);
		}
	}
}

static int gal4_load(struct module_data *m, FILE *f, const int start)
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
	iff_register(handle, "MAIN", get_main);
	iff_register(handle, "ORDR", get_ordr);
	iff_register(handle, "PATT", get_patt_cnt);
	iff_register(handle, "INST", get_inst_cnt);
	iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	iff_set_quirk(handle, IFF_CHUNK_TRUNC4);

	/* Load IFF chunks */
	while (!feof(f)) {
		iff_chunk(handle, m, f, &data);
	}

	iff_release(handle);

	mod->trk = mod->pat * mod->chn;

	MODULE_INFO();
	INSTRUMENT_INIT();
	PATTERN_INIT();

	D_(D_INFO "Stored patterns: %d\n", mod->pat);
	D_(D_INFO "Stored samples : %d ", mod->smp);

	fseek(f, start + offset, SEEK_SET);
	data.snum = 0;

	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	iff_register(handle, "PATT", get_patt);
	iff_register(handle, "INST", get_inst);
	iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	iff_set_quirk(handle, IFF_CHUNK_TRUNC4);

	/* Load IFF chunks */
	while (!feof (f)) {
		iff_chunk(handle, m, f, &data);
	}

	iff_release(handle);

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = 0x80;

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
