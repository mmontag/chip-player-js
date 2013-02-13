/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <assert.h>

#include "loader.h"
#include "iff.h"
#include "period.h"

#define MAGIC_D_T_	MAGIC4('D','.','T','.')


static int dt_test(FILE *, char *, const int);
static int dt_load (struct module_data *, FILE *, const int);

const struct format_loader dt_loader = {
	"Digital Tracker (DTM)",
	dt_test,
	dt_load
};

static int dt_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != MAGIC_D_T_)
		return -1;

	read32b(f);			/* chunk size */
	read16b(f);			/* type */
	read16b(f);			/* 0xff then mono */
	read16b(f);			/* reserved */
	read16b(f);			/* tempo */
	read16b(f);			/* bpm */
	read32b(f);			/* undocumented */

	read_title(f, t, 32);

	return 0;
}


struct local_data {
    int pflag, sflag;
    int realpat;
};


static void get_d_t_(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int b;

	read16b(f);			/* type */
	read16b(f);			/* 0xff then mono */
	read16b(f);			/* reserved */
	mod->spd = read16b(f);
	if ((b = read16b(f)) > 0)	/* RAMBO.DTM has bpm 0 */
		mod->bpm = b;
	read32b(f);			/* undocumented */

	fread(mod->name, 32, 1, f);
	set_type(m, "Digital Tracker DTM");

	MODULE_INFO();
}

static void get_s_q_(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, maxpat;

	mod->len = read16b(f);
	mod->rst = read16b(f);
	read32b(f);	/* reserved */

	for (maxpat = i = 0; i < 128; i++) {
		mod->xxo[i] = read8(f);
		if (mod->xxo[i] > maxpat)
			maxpat = mod->xxo[i];
	}
	mod->pat = maxpat + 1;
}

static void get_patt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;

	mod->chn = read16b(f);
	data->realpat = read16b(f);
	mod->trk = mod->chn * mod->pat;
}

static void get_inst(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, c2spd;
	uint8 name[30];

	mod->ins = mod->smp = read16b(f);

	D_(D_INFO "Instruments    : %d ", mod->ins);

	INSTRUMENT_INIT();

	for (i = 0; i < mod->ins; i++) {
		int fine, replen, flag;

		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		read32b(f);		/* reserved */
		mod->xxs[i].len = read32b(f);
		mod->xxi[i].nsm = !!mod->xxs[i].len;
		fine = read8s(f);	/* finetune */
		mod->xxi[i].sub[0].vol = read8(f);
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxs[i].lps = read32b(f);
		replen = read32b(f);
		mod->xxs[i].lpe = mod->xxs[i].lps + replen - 1;
		mod->xxs[i].flg = replen > 2 ?  XMP_SAMPLE_LOOP : 0;

		fread(name, 22, 1, f);
		copy_adjust(mod->xxi[i].name, name, 22);

		flag = read16b(f);	/* bit 0-7:resol 8:stereo */
		if ((flag & 0xff) > 8) {
			mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
			mod->xxs[i].len >>= 1;
			mod->xxs[i].lps >>= 1;
			mod->xxs[i].lpe >>= 1;
		}

		read32b(f);		/* midi note (0x00300000) */
		c2spd = read32b(f);	/* frequency */
		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);

		/* It's strange that we have both c2spd and finetune */
		mod->xxi[i].sub[0].fin += fine;

		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-22.22s %05x%c%05x %05x %c%c %2db V%02x F%+03d %5d",
			i, mod->xxi[i].name,
			mod->xxs[i].len,
			mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			mod->xxs[i].lps,
			replen,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			flag & 0x100 ? 'S' : ' ',
			flag & 0xff,
			mod->xxi[i].sub[0].vol,
			fine,
			c2spd);
	}
}

static void get_dapt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int pat, i, j, k;
	struct xmp_event *event;
	static int last_pat;
	int rows;

	if (!data->pflag) {
		D_(D_INFO "Stored patterns: %d", mod->pat);
		data->pflag = 1;
		last_pat = 0;
		PATTERN_INIT();
	}

	read32b(f);	/* 0xffffffff */
	i = pat = read16b(f);
	rows = read16b(f);

	for (i = last_pat; i <= pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = rows;
		TRACK_ALLOC(i);
	}
	last_pat = pat + 1;

	for (j = 0; j < rows; j++) {
		for (k = 0; k < mod->chn; k++) {
			uint8 a, b, c, d;

			event = &EVENT(pat, k, j);
			a = read8(f);
			b = read8(f);
			c = read8(f);
			d = read8(f);
			if (a) {
				a--;
				event->note = 12 * (a >> 4) + (a & 0x0f) + 12;
			}
			event->vol = (b & 0xfc) >> 2;
			event->ins = ((b & 0x03) << 4) + (c >> 4);
			event->fxt = c & 0xf;
			event->fxp = d;
		}
	}
}

static void get_dait(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	static int i = 0;

	if (!data->sflag) {
		D_(D_INFO "Stored samples : %d ", mod->smp);
		data->sflag = 1;
		i = 0;
	}

	if (size > 2) {
		load_sample(f, SAMPLE_FLAG_BIGEND,
				&mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	i++;
}

static int dt_load(struct module_data *m, FILE *f, const int start)
{
	iff_handle handle;
	struct local_data data;

	LOAD_INIT();

	data.pflag = data.sflag = 0;
	
	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	iff_register(handle, "D.T.", get_d_t_);
	iff_register(handle, "S.Q.", get_s_q_);
	iff_register(handle, "PATT", get_patt);
	iff_register(handle, "INST", get_inst);
	iff_register(handle, "DAPT", get_dapt);
	iff_register(handle, "DAIT", get_dait);

	/* Load IFF chunks */
	while (!feof(f)) {
		iff_chunk(handle, m, f , &data);
	}

	iff_release(handle);

	return 0;
}

