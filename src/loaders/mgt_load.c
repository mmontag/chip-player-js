/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "period.h"

#define MAGIC_MGT	MAGIC4(0x00,'M','G','T')
#define MAGIC_MCS	MAGIC4(0xbd,'M','C','S')


static int mgt_test (HANDLE *, char *, const int);
static int mgt_load (struct module_data *, HANDLE *, const int);

const struct format_loader mgt_loader = {
	"Megatracker (MGT)",
	mgt_test,
	mgt_load
};

static int mgt_test(HANDLE *f, char *t, const int start)
{
	int sng_ptr;

	if (hread_24b(f) != MAGIC_MGT)
		return -1;
	hread_8(f);
	if (hread_32b(f) != MAGIC_MCS)
		return -1;

	hseek(f, 18, SEEK_CUR);
	sng_ptr = hread_32b(f);
	hseek(f, start + sng_ptr, SEEK_SET);

	read_title(f, t, 32);
	
	return 0;
}

static int mgt_load(struct module_data *m, HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j;
	int ver;
	int sng_ptr, seq_ptr, ins_ptr, pat_ptr, trk_ptr, smp_ptr;
	int sdata[64];

	LOAD_INIT();

	hread_24b(f);		/* MGT */
	ver = hread_8(f);
	hread_32b(f);		/* MCS */

	set_type(m, "Megatracker MGT v%d.%d", MSN(ver), LSN(ver));

	mod->chn = hread_16b(f);
	hread_16b(f);			/* number of songs */
	mod->len = hread_16b(f);
	mod->pat = hread_16b(f);
	mod->trk = hread_16b(f);
	mod->ins = mod->smp = hread_16b(f);
	hread_16b(f);			/* reserved */
	hread_32b(f);			/* reserved */

	sng_ptr = hread_32b(f);
	seq_ptr = hread_32b(f);
	ins_ptr = hread_32b(f);
	pat_ptr = hread_32b(f);
	trk_ptr = hread_32b(f);
	smp_ptr = hread_32b(f);
	hread_32b(f);			/* total smp len */
	hread_32b(f);			/* unpacked trk size */

	hseek(f, start + sng_ptr, SEEK_SET);

	hread(mod->name, 1, 32, f);
	seq_ptr = hread_32b(f);
	mod->len = hread_16b(f);
	mod->rst = hread_16b(f);
	mod->bpm = hread_8(f);
	mod->spd = hread_8(f);
	hread_16b(f);			/* global volume */
	hread_8(f);			/* master L */
	hread_8(f);			/* master R */

	for (i = 0; i < mod->chn; i++) {
		hread_16b(f);		/* pan */
	}
	
	MODULE_INFO();

	/* Sequence */

	hseek(f, start + seq_ptr, SEEK_SET);
	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = hread_16b(f);

	/* Instruments */

	INSTRUMENT_INIT();

	hseek(f, start + ins_ptr, SEEK_SET);

	for (i = 0; i < mod->ins; i++) {
		int c2spd, flags;

		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		hread(mod->xxi[i].name, 1, 32, f);
		sdata[i] = hread_32b(f);
		mod->xxs[i].len = hread_32b(f);
		mod->xxs[i].lps = hread_32b(f);
		mod->xxs[i].lpe = mod->xxs[i].lps + hread_32b(f);
		hread_32b(f);
		hread_32b(f);
		c2spd = hread_32b(f);
		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
		mod->xxi[i].sub[0].vol = hread_16b(f) >> 4;
		hread_8(f);		/* vol L */
		hread_8(f);		/* vol R */
		mod->xxi[i].sub[0].pan = 0x80;
		flags = hread_8(f);
		mod->xxs[i].flg = flags & 0x03 ? XMP_SAMPLE_LOOP : 0;
		mod->xxs[i].flg |= flags & 0x02 ? XMP_SAMPLE_LOOP_BIDIR : 0;
		mod->xxi[i].sub[0].fin += 0 * hread_8(f);	// FIXME
		hread_8(f);		/* unused */
		hread_8(f);
		hread_8(f);
		hread_8(f);
		hread_16b(f);
		hread_32b(f);
		hread_32b(f);

		mod->xxi[i].nsm = !!mod->xxs[i].len;
		mod->xxi[i].sub[0].sid = i;
		
		D_(D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x %5d\n",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, c2spd);
	}

	/* PATTERN_INIT - alloc extra track*/
	PATTERN_INIT();

	D_(D_INFO "Stored tracks: %d", mod->trk);

	/* Tracks */

	for (i = 1; i < mod->trk; i++) {
		int offset, rows;
		uint8 b;

		hseek(f, start + trk_ptr + i * 4, SEEK_SET);
		offset = hread_32b(f);
		hseek(f, start + offset, SEEK_SET);

		rows = hread_16b(f);
		mod->xxt[i] = calloc(sizeof(struct xmp_track) +
				sizeof(struct xmp_event) * rows, 1);
		mod->xxt[i]->rows = rows;

		//printf("\n=== Track %d ===\n\n", i);
		for (j = 0; j < rows; j++) {
			uint8 note, f2p;

			b = hread_8(f);
			j += b & 0x03;

			note = 0;
			event = &mod->xxt[i]->event[j];
			if (b & 0x04)
				note = hread_8(f);
			if (b & 0x08)
				event->ins = hread_8(f);
			if (b & 0x10)
				event->vol = hread_8(f);
			if (b & 0x20)
				event->fxt = hread_8(f);
			if (b & 0x40)
				event->fxp = hread_8(f);
			if (b & 0x80)
				f2p = hread_8(f);

			if (note == 1)
				event->note = XMP_KEY_OFF;
			else if (note > 11) /* adjusted to play codeine.mgt */
				event->note = note + 1;

			/* effects */
			if (event->fxt < 0x10)
				/* like amiga */ ;
			else switch (event->fxt) {
			case 0x13: 
			case 0x14: 
			case 0x15: 
			case 0x17: 
			case 0x1c: 
			case 0x1d: 
			case 0x1e: 
				event->fxt = FX_EXTENDED;
				event->fxp = ((event->fxt & 0x0f) << 4) |
							(event->fxp & 0x0f);
				break;
			default:
				event->fxt = event->fxp = 0;
			}

			/* volume and volume column effects */
			if ((event->vol >= 0x10) && (event->vol <= 0x50)) {
				event->vol -= 0x0f;
				continue;
			}

			switch (event->vol >> 4) {
			case 0x06:	/* Volume slide down */
				event->f2t = FX_VOLSLIDE_2;
				event->f2p = event->vol - 0x60;
				break;
			case 0x07:	/* Volume slide up */
				event->f2t = FX_VOLSLIDE_2;
				event->f2p = (event->vol - 0x70) << 4;
				break;
			case 0x08:	/* Fine volume slide down */
				event->f2t = FX_EXTENDED;
				event->f2p = (EX_F_VSLIDE_DN << 4) |
							(event->vol - 0x80);
				break;
			case 0x09:	/* Fine volume slide up */
				event->f2t = FX_EXTENDED;
				event->f2p = (EX_F_VSLIDE_UP << 4) |
							(event->vol - 0x90);
				break;
			case 0x0a:	/* Set vibrato speed */
				event->f2t = FX_VIBRATO;
				event->f2p = (event->vol - 0xa0) << 4;
				break;
			case 0x0b:	/* Vibrato */
				event->f2t = FX_VIBRATO;
				event->f2p = event->vol - 0xb0;
				break;
			case 0x0c:	/* Set panning */
				event->f2t = FX_SETPAN;
				event->f2p = ((event->vol - 0xc0) << 4) + 8;
				break;
			case 0x0d:	/* Pan slide left */
				event->f2t = FX_PANSLIDE;
				event->f2p = (event->vol - 0xd0) << 4;
				break;
			case 0x0e:	/* Pan slide right */
				event->f2t = FX_PANSLIDE;
				event->f2p = event->vol - 0xe0;
				break;
			case 0x0f:	/* Tone portamento */
				event->f2t = FX_TONEPORTA;
				event->f2p = (event->vol - 0xf0) << 4;
				break;
			}
	
			event->vol = 0;

			/*printf("%02x  %02x %02x %02x %02x %02x\n",
				j, event->note, event->ins, event->vol,
				event->fxt, event->fxp);*/
		}
	}

	/* Extra track */
	mod->xxt[0] = calloc(sizeof(struct xmp_track) +
			sizeof(struct xmp_event) * 64 - 1, 1);
	mod->xxt[0]->rows = 64;

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	hseek(f, start + pat_ptr, SEEK_SET);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);

		mod->xxp[i]->rows = hread_16b(f);
		for (j = 0; j < mod->chn; j++) {
			mod->xxp[i]->index[j] = hread_16b(f) - 1;
		}
	}

	/* Read samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxi[i].nsm == 0)
			continue;

		hseek(f, start + sdata[i], SEEK_SET);
		load_sample(m, f, 0, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
