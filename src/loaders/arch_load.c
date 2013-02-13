/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "iff.h"

#define MAGIC_MUSX	MAGIC4('M','U','S','X')
#define MAGIC_MNAM	MAGIC4('M','N','A','M')


static int arch_test (FILE *, char *, const int);
static int arch_load (struct module_data *, FILE *, const int);


const struct format_loader arch_loader = {
	"Archimedes Tracker",
	arch_test,
	arch_load
};

/* 
 * Linear (0 to 0x40) to logarithmic volume conversion.
 * This is only used for the Protracker-compatible "linear volume" effect in
 * Andy Southgate's StasisMod.  In this implementation linear and logarithmic
 * volumes can be freely intermixed.
 */
static const uint8 lin_table[65]={
	0x00, 0x48, 0x64, 0x74, 0x82, 0x8a, 0x92, 0x9a,
	0xa2, 0xa6, 0xaa, 0xae, 0xb2, 0xb6, 0xea, 0xbe,
	0xc2, 0xc4, 0xc6, 0xc8, 0xca, 0xcc, 0xce, 0xd0,
	0xd2, 0xd4, 0xd6, 0xd8, 0xda, 0xdc, 0xde, 0xe0,
	0xe2, 0xe2, 0xe4, 0xe4, 0xe6, 0xe6, 0xe8, 0xe8,
	0xea, 0xea, 0xec, 0xec, 0xee, 0xee, 0xf0, 0xf0,
	0xf2, 0xf2, 0xf4, 0xf4, 0xf6, 0xf6, 0xf8, 0xf8,
	0xfa, 0xfa, 0xfc, 0xfc, 0xfe, 0xfe, 0xfe, 0xfe,
	0xfe
};

#if 0
static uint8 convert_vol(uint8 vol) {
/*	return pow(2,6.0-(255.0-vol)/32)+.5; */
	return vol_table[vol];
}
#endif

static int arch_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != MAGIC_MUSX)
		return -1;

	read32l(f);

	while (!feof(f)) {
		uint32 id = read32b(f);
		uint32 len = read32l(f);

		if (id == MAGIC_MNAM) {
			read_title(f, t, 32);
			return 0;
		}

		fseek(f, len, SEEK_CUR);
	}

	read_title(f, t, 0);

	return 0;
}


struct local_data {
    int year, month, day;
    int pflag, sflag, max_ins;
    uint8 ster[8], rows[64];
};

static void fix_effect(struct xmp_event *e)
{
#if 0
	/* for debugging */
	printf ("%c%02x ", e->fxt["0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"], e->fxp);
#endif
	switch (e->fxt) {
	case 0x00:			/* 00 xy Normal play or Arpeggio */
		e->fxt = FX_ARPEGGIO;
		/* x: first halfnote to add
		   y: second halftone to subtract */
		break;
	case 0x01:			/* 01 xx Slide Up */
		e->fxt = FX_PORTA_UP;
		break;
	case 0x02:			/* 02 xx Slide Down */
		e->fxt = FX_PORTA_DN;
		break;
	case 0x03:			/* 03 xx Tone Portamento */
		e->fxt = FX_TONEPORTA;
		break;
	case 0x0b:			/* 0B xx Break Pattern */
		e->fxt = FX_BREAK;
		break;
	case 0x0c:
		/* Set linear volume */
		if (e->fxp <= 64) {
			e->fxt = FX_VOLSET;
			e->fxp = lin_table[e->fxp];
		} else {
			e->fxp = e->fxt = 0;
		}
		break;
	case 0x0e:			/* 0E xy Set Stereo */
	case 0x19: /* StasisMod's non-standard set panning effect */
		/* y: stereo position (1-7,ignored). 1=left 4=center 7=right */
		if (e->fxp>0 && e->fxp<8) {
			e->fxt = FX_SETPAN;
			e->fxp = 42*e->fxp-40;
		} else
			e->fxt = e->fxp = 0;
		break;
	case 0x10:			/* 10 xx Volume Slide Up */
		e->fxt = FX_VOLSLIDE_UP;
		break;
	case 0x11:			/* 11 xx Volume Slide Down */
		e->fxt = FX_VOLSLIDE_DN;
		break;
	case 0x13:			/* 13 xx Position Jump */
		e->fxt = FX_JUMP;
		break;
	case 0x15:			/* 15 xy Line Jump. (not in manual) */
		e->fxt = e->fxp = 0;
		/* Jump to line 10*x+y in same pattern. (10*x+y>63 ignored) */
		break;
	case 0x1c:			/* 1C xy Set Speed */
		e->fxt = FX_SPEED;
		break;
	case 0x1f:			/* 1F xx Set Volume */
		e->fxt = FX_VOLSET;
		/* all volumes are logarithmic */
		/* e->fxp = convert_vol (e->fxp); */
		break;
	default:
		e->fxt = e->fxp = 0;
	}
}

static void get_tinf(struct module_data *m, int size, FILE *f, void *parm)
{
	struct local_data *data = (struct local_data *)parm;
	int x;

	x = read8(f);
	data->year = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);
	x = read8(f);
	data->year += ((x & 0xf0) >> 4) * 1000 + (x & 0x0f) * 100;

	x = read8(f);
	data->month = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);

	x = read8(f);
	data->day = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);
}

static void get_mvox(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	mod->chn = read32l(f);
}

static void get_ster(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i;

	fread(data->ster, 1, 8, f);
	
	for (i=0; i < mod->chn; i++) {
		if (data->ster[i] > 0 && data->ster[i] < 8) {
			mod->xxc[i].pan = 42 * data->ster[i] - 40;
		}
	}
}

static void get_mnam(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	fread(mod->name, 1, 32, f);
}

static void get_anam(struct module_data *m, int size, FILE *f, void *parm)
{
	/*fread(m->author, 1, 32, f); */
}

static void get_mlen(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	mod->len = read32l(f);
}

static void get_pnum(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	mod->pat = read32l(f);
}

static void get_plen(struct module_data *m, int size, FILE *f, void *parm)
{
	struct local_data *data = (struct local_data *)parm;

	fread(data->rows, 1, 64, f);
}

static void get_sequ(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	fread(mod->xxo, 1, 128, f);

	set_type(m, "Archimedes Tracker");

	MODULE_INFO();
}

static void get_patt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	static int i = 0;
	int j, k;
	struct xmp_event *event;

	if (!data->pflag) {
		D_(D_INFO "Stored patterns: %d", mod->pat);
		data->pflag = 1;
		i = 0;
		mod->trk = mod->pat * mod->chn;
		PATTERN_INIT();
	}

	PATTERN_ALLOC(i);
	mod->xxp[i]->rows = data->rows[i];
	TRACK_ALLOC(i);

	for (j = 0; j < data->rows[i]; j++) {
		for (k = 0; k < mod->chn; k++) {
			event = &EVENT(i, k, j);

			event->fxp = read8(f);
			event->fxt = read8(f);
			event->ins = read8(f);
			event->note = read8(f);

			if (event->note)
				event->note += 48;

			fix_effect(event);
		}
	}

	i++;
}

static void get_samp(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	static int i = 0;

	if (!data->sflag) {
		mod->smp = mod->ins = 36;
		INSTRUMENT_INIT();

		D_(D_INFO "Instruments: %d", mod->ins);

		data->sflag = 1;
		data->max_ins = 0;
		i = 0;
	}

	/* FIXME: More than 36 sample slots used.  Unfortunately we
	 * have no way to handle this without two passes, and there's
	 * only officially supposed to be 36, so ignore the rest.
	 */
	if (i >= 36)
		return;

	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	read32l(f);	/* SNAM */
	{
		/* should usually be 0x14 but zero is not unknown */
		int name_len = read32l(f);
		if (name_len < 32)
			fread(mod->xxi[i].name, 1, name_len, f);
	}
	read32l(f);	/* SVOL */
	read32l(f);
	/* mod->xxi[i].sub[0].vol = convert_vol(read32l(f)); */
	mod->xxi[i].sub[0].vol = read32l(f) & 0xff;
	read32l(f);	/* SLEN */
	read32l(f);
	mod->xxs[i].len = read32l(f);
	read32l(f);	/* ROFS */
	read32l(f);
	mod->xxs[i].lps = read32l(f);
	read32l(f);	/* RLEN */
	read32l(f);
	mod->xxs[i].lpe = read32l(f);

	read32l(f);	/* SDAT */
	read32l(f);
	read32l(f);	/* 0x00000000 */

	mod->xxi[i].nsm = 1;
	mod->xxi[i].sub[0].sid = i;
	mod->xxi[i].sub[0].pan = 0x80;

	m->vol_table = (int *)arch_vol_table;
	m->volbase = 0xff;

	if (mod->xxs[i].lpe > 2) {
		mod->xxs[i].flg = XMP_SAMPLE_LOOP;
		mod->xxs[i].lpe = mod->xxs[i].lps + mod->xxs[i].lpe;
	} else if (mod->xxs[i].lpe == 2 && mod->xxs[i].lps > 0) {
		/* non-zero repeat offset and repeat length of 2
		 * means loop to end of sample */
		mod->xxs[i].flg = XMP_SAMPLE_LOOP;
		mod->xxs[i].lpe = mod->xxs[i].len;
	}

	load_sample(f, SAMPLE_FLAG_VIDC, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);

	D_(D_INFO "[%2X] %-20.20s %05x %05x %05x %c V%02x",
				i, mod->xxi[i].name,
				mod->xxs[i].len,
				mod->xxs[i].lps,
				mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol);

	i++;
	data->max_ins++;
}

static int arch_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	iff_handle handle;
	int i;
	struct local_data data;

	LOAD_INIT();

	read32b(f);	/* MUSX */
	read32b(f);

	data.pflag = data.sflag = 0;
	data.year = data.month = data.day = 0;

	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	iff_register(handle, "TINF", get_tinf);
	iff_register(handle, "MVOX", get_mvox);
	iff_register(handle, "STER", get_ster);
	iff_register(handle, "MNAM", get_mnam);
	iff_register(handle, "ANAM", get_anam);
	iff_register(handle, "MLEN", get_mlen);
	iff_register(handle, "PNUM", get_pnum);
	iff_register(handle, "PLEN", get_plen);
	iff_register(handle, "SEQU", get_sequ);
	iff_register(handle, "PATT", get_patt);
	iff_register(handle, "SAMP", get_samp);

	iff_set_quirk(handle, IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof(f)) {
		iff_chunk(handle, m, f, &data);
	}

	iff_release(handle);

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = (((i + 3) / 2) % 2) * 0xff;

	return 0;
}

