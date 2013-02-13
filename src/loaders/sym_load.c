/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "depackers/readlzw.h"


static int sym_test(FILE *, char *, const int);
static int sym_load (struct module_data *, FILE *, const int);

const struct format_loader sym_loader = {
	"Digital Symphony",
	sym_test,
	sym_load
};

static int sym_test(FILE * f, char *t, const int start)
{
	uint32 a, b;
	int i, ver;

	a = read32b(f);
	b = read32b(f);

	if (a != 0x02011313 || b != 0x1412010B)		/* BASSTRAK */
		return -1;

	ver = read8(f);

	/* v1 files are the same as v0 but may contain strange compression
	 * formats. Deal with that problem later if it arises.
	 */
	if (ver > 1) {
		return -1;
	}

	read8(f);		/* chn */
	read16l(f);		/* pat */
	read16l(f);		/* trk */
	read24l(f);		/* infolen */

	for (i = 0; i < 63; i++) {
		if (~read8(f) & 0x80)
			read24l(f);
	}

	read_title(f, t, read8(f));

	return 0;
}



static void fix_effect(struct xmp_event *e, int parm)
{
	switch (e->fxt) {
	case 0x00:	/* 00 xyz Normal play or Arpeggio + Volume Slide Up */
	case 0x01:	/* 01 xyy Slide Up + Volume Slide Up */
	case 0x02:	/* 01 xyy Slide Up + Volume Slide Up */
		e->fxp = parm & 0xff;
		if (parm >> 8) {
			e->f2t = FX_VOLSLIDE_UP;
			e->f2p = parm >> 8;
		}
		break;
	case 0x03:	/* 03 xyy Tone Portamento */
	case 0x04:	/* 04 xyz Vibrato */
	case 0x07:	/* 07 xyz Tremolo */
		e->fxp = parm;
		break;
	case 0x05:	/* 05 xyz Tone Portamento + Volume Slide */
	case 0x06:	/* 06 xyz Vibrato + Volume Slide */
		e->fxp = parm;
		if (!parm)
			e->fxt -= 2;
		break;
	case 0x09:	/* 09 xxx Set Sample Offset */
		e->fxp = parm >> 1;
		break;
	case 0x0a:	/* 0A xyz Volume Slide + Fine Slide Up */
		if (parm & 0xff) {
			e->fxp = parm & 0xff;
		} else {
			e->fxt = 0;
		}
		e->f2t = FX_EXTENDED;
		e->f2p = (EX_F_PORTA_UP << 4) | ((parm & 0xf00) >> 8);
		break;
	case 0x0b:	/* 0B xxx Position Jump */
	case 0x0c:	/* 0C xyy Set Volume */
	case 0x0d:	/* 0D xyy Pattern Break */
	case 0x0f:	/* 0F xxx Set Speed */
		e->fxp = parm;
		break;
	case 0x13:	/* 13 xxy Glissando Control */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_GLISS << 4) | (parm & 0x0f);
		break;
	case 0x14:	/* 14 xxy Set Vibrato Waveform */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_VIBRATO_WF << 4) | (parm & 0x0f);
		break;
	case 0x15:	/* 15 xxy Set Fine Tune */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_FINETUNE << 4) | (parm & 0x0f);
		break;
	case 0x16:	/* 16 xxx Jump to Loop */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_PATTERN_LOOP << 4) | (parm & 0x0f);
		break;
	case 0x17:	/* 17 xxy Set Tremolo Waveform */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_TREMOLO_WF << 4) | (parm & 0x0f);
		break;
	case 0x19:	/* 19 xxx Retrig Note */
		if (parm < 0x10) {
			e->fxt = FX_EXTENDED;
			e->fxp = (EX_RETRIG << 4) | (parm & 0x0f);
		} else {
			/* ignore */
			e->fxt = 0;
		}
		break;
	case 0x11:	/* 11 xyy Fine Slide Up + Fine Volume Slide Up */
	case 0x12:	/* 12 xyy Fine Slide Down + Fine Volume Slide Up */
	case 0x1a:	/* 1A xyy Fine Slide Up + Fine Volume Slide Down */
	case 0x1b:	/* 1B xyy Fine Slide Down + Fine Volume Slide Down */
	{
		uint8 pitch_effect = ((e->fxt == 0x11 || e->fxt == 0x1a) ?
				      EX_F_PORTA_UP : EX_F_PORTA_DN);
		uint8 vol_effect = ((e->fxt == 0x11 || e->fxt == 0x12) ?
				      EX_F_VSLIDE_UP : EX_F_VSLIDE_DN);

		if ((parm & 0xff) && ((parm & 0xff) < 0x10)) {
			e->fxt = FX_EXTENDED;
			e->fxp = (pitch_effect << 4) | (parm & 0x0f);
		} else
			e->fxt = 0;
		if (parm >> 8) {
			e->f2t = FX_EXTENDED;
			e->f2p = (vol_effect << 4) | (parm >> 8);
		}
		break;
	}
	case 0x1c:	/* 1C xxx Note Cut */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_CUT << 4) | (parm & 0x0f);
		break;
	case 0x1d:	/* 1D xxx Note Delay */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_DELAY << 4) | (parm & 0x0f);
		break;
	case 0x1e:	/* 1E xxx Pattern Delay */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_PATT_DELAY << 4) | (parm & 0x0f);
		break;
	case 0x1f:	/* 1F xxy Invert Loop */
		e->fxt = 0;
		break;
	case 0x20:	/* 20 xyz Normal play or Arpeggio + Volume Slide Down */
		e->fxt = FX_ARPEGGIO;
		e->fxp = parm & 0xff;
		if (parm >> 8) {
			e->f2t = FX_VOLSLIDE_DN;
			e->f2p = parm >> 8;
		}
		break;
	case 0x21:	/* 21 xyy Slide Up + Volume Slide Down */
		e->fxt = FX_PORTA_UP;
		e->fxp = parm & 0xff;
		if (parm >> 8) {
			e->f2t = FX_VOLSLIDE_DN;
			e->f2p = parm >> 8;
		}
		break;
	case 0x22:	/* 22 xyy Slide Down + Volume Slide Down */
		e->fxt = FX_PORTA_DN;
		e->fxp = parm & 0xff;
		if (parm >> 8) {
			e->f2t = FX_VOLSLIDE_DN;
			e->f2p = parm >> 8;
		}
		break;
	case 0x2f:	/* 2F xxx Set Tempo */
		if (parm >= 0x100 && parm <= 0x800) {
			e->fxt = FX_SPEED;
			e->fxp = (parm+4) >> 3; /* round to nearest */
		} else {
			/* umm... */
		}
		break;
	case 0x2a:	/* 2A xyz Volume Slide + Fine Slide Down */
	case 0x2b:	/* 2B xyy Line Jump */
	case 0x30:	/* 30 xxy Set Stereo */
	case 0x31:	/* 31 xxx Song Upcall */
	case 0x32:	/* 32 xxx Unset Sample Repeat */
	default:
		e->fxt = 0;
	}
}

static uint32 readptr32l(uint8 *p)
{
	uint32 a, b, c, d;

	a = *p++;
	b = *p++;
	c = *p++;
	d = *p++;

	return (d << 24) | (c << 16) | (b << 8) | a;
}

static uint32 readptr16l(uint8 *p)
{
	uint32 a, b;

	a = *p++;
	b = *p++;

	return (b << 8) | a;
}

static int sym_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j;
	int ver, infolen, sn[64];
	uint32 a, b;
	uint8 *buf;
	int size;
	uint8 allowed_effects[8];

	LOAD_INIT();

	fseek(f, 8, SEEK_CUR);			/* BASSTRAK */

	ver = read8(f);
	set_type(m, "Digital Symphony");

	mod->chn = read8(f);
	mod->len = mod->pat = read16l(f);
	mod->trk = read16l(f);	/* Symphony patterns are actually tracks */
	infolen = read24l(f);

	mod->ins = mod->smp = 63;

	INSTRUMENT_INIT();

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		sn[i] = read8(f);	/* sample name length */

		if (~sn[i] & 0x80)
			mod->xxs[i].len = read24l(f) << 1;
	}

	a = read8(f);			/* track name length */

	fread(mod->name, 1, a, f);
	fread(&allowed_effects, 1, 8, f);

	MODULE_INFO();

	mod->trk++;			/* alloc extra empty track */
	PATTERN_INIT();

	/* Sequence */
	a = read8(f);			/* packing */

	if (a != 0 && a != 1)
		return -1;

	D_(D_INFO "Packed sequence: %s", a ? "yes" : "no");

	size = mod->len * mod->chn * 2;
	if ((buf = malloc(size)) == NULL)
		return -1;

	if (a) {
		unsigned char *x = read_lzw_dynamic(f, buf, 13, 0, size,
						size, XMP_LZW_QUIRK_DSYM);
		if (x == NULL) {
			free(buf);
			return -1;
		}
	} else {
		fread(buf, 1, size, f);
	}

	for (i = 0; i < mod->len; i++) {	/* len == pat */
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		for (j = 0; j < mod->chn; j++) {
			int idx = 2 * (i * mod->chn + j);
			mod->xxp[i]->index[j] = readptr16l(&buf[idx]);

			if (mod->xxp[i]->index[j] == 0x1000) /* empty trk */
				mod->xxp[i]->index[j] = mod->trk - 1;

		}
		mod->xxo[i] = i;
	}
	free(buf);

	/* Read and convert patterns */

	a = read8(f);

	if (a != 0 && a != 1)
		return -1;

	D_(D_INFO "Packed tracks: %s", a ? "yes" : "no");
	D_(D_INFO "Stored tracks: %d", mod->trk - 1);

	size = 64 * (mod->trk - 1) * 4;
	if ((buf = malloc(size)) == NULL)
		return -1;

	if (a) {
		unsigned char *x = read_lzw_dynamic(f, buf, 13, 0, size,
						size, XMP_LZW_QUIRK_DSYM);
		if (x == NULL) {
			free(buf);
			return -1;
		}
	} else {
		fread(buf, 1, size, f);
	}

	for (i = 0; i < mod->trk - 1; i++) {
		mod->xxt[i] = calloc(sizeof(struct xmp_track) +
				sizeof(struct xmp_event) * 64 - 1, 1);
		mod->xxt[i]->rows = 64;

		for (j = 0; j < mod->xxt[i]->rows; j++) {
			int parm;

			event = &mod->xxt[i]->event[j];

			b = readptr32l(&buf[4 * (i * 64 + j)]);
			event->note = b & 0x0000003f;
			if (event->note)
				event->note += 48;
			event->ins = (b & 0x00001fc0) >> 6;
			event->fxt = (b & 0x000fc000) >> 14;
			parm = (b & 0xfff00000) >> 20;

			if (allowed_effects[event->fxt >> 3] & (1 << (event->fxt & 7))) {
				fix_effect(event, parm);
			} else {
				event->fxt = 0;
			}
		}
	}

	free(buf);

	/* Extra track */
	mod->xxt[i] = calloc(sizeof(struct xmp_track) +
				sizeof(struct xmp_event) * 64 - 1, 1);
	mod->xxt[i]->rows = 64;

	/* Load and convert instruments */

	D_(D_INFO "Instruments: %d", mod->ins);

	for (i = 0; i < mod->ins; i++) {
		uint8 buf[128];

		memset(buf, 0, 128);
		fread(buf, 1, sn[i] & 0x7f, f);
		copy_adjust(mod->xxi[i].name, buf, 32);

		if (~sn[i] & 0x80) {
			int looplen;

			mod->xxs[i].lps = read24l(f) << 1;
			looplen = read24l(f) << 1;
			if (looplen > 2)
				mod->xxs[i].flg |= XMP_SAMPLE_LOOP;
			mod->xxs[i].lpe = mod->xxs[i].lps + looplen;
			mod->xxi[i].nsm = 1;
			mod->xxi[i].sub[0].vol = read8(f);
			mod->xxi[i].sub[0].pan = 0x80;
			/* finetune adjusted comparing DSym and S3M versions
			 * of "inside out" */
			mod->xxi[i].sub[0].fin = (int8)(read8(f) << 4);
			mod->xxi[i].sub[0].sid = i;
		}

		D_(D_INFO "[%2X] %-22.22s %05x %05x %05x %c V%02x %+03d",
				i, mod->xxi[i].name, mod->xxs[i].len,
				mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin);

		if (sn[i] & 0x80 || mod->xxs[i].len == 0)
			continue;

		a = read8(f);

		if (a != 0 && a != 1) {
			fprintf(stderr, "libxmp: unsupported sample type\n");
			//return -1;
		}

		if (a == 1) {
			uint8 *b = malloc(mod->xxs[i].len);
			read_lzw_dynamic(f, b, 13, 0, mod->xxs[i].len,
					mod->xxs[i].len, XMP_LZW_QUIRK_DSYM);
			load_sample(NULL, SAMPLE_FLAG_NOLOAD | SAMPLE_FLAG_DIFF,
				&mod->xxs[mod->xxi[i].sub[0].sid], (char*)b);
			free(b);
		/*} else if (a == 4) {
			load_sample(f, SAMPLE_FLAG_VIDC,
				&mod->xxs[mod->xxi[i].sub[0].sid], NULL);*/
		} else {
			load_sample(f, SAMPLE_FLAG_VIDC,
				&mod->xxs[mod->xxi[i].sub[0].sid], NULL);
		}
	}

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = (((i + 3) / 2) % 2) * 0xff;

	return 0;
}
