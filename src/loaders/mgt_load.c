/* Megatracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: mgt_load.c,v 1.7 2007-10-14 03:17:17 cmatsuoka Exp $
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

#define MAGIC_MGT	MAGIC4(0x00,'M','G','T')
#define MAGIC_MCS	MAGIC4(0xbd,'M','C','S')


static int mgt_test(FILE *, char *);
static int mgt_load(FILE *);

struct xmp_loader_info mgt_loader = {
	"MGT",
	"Megatracker",
	mgt_test,
	mgt_load
};

static int mgt_test(FILE *f, char *t)
{
	int sng_ptr;

	if (read24b(f) != MAGIC_MGT)
		return -1;
	read8(f);
	if (read32b(f) != MAGIC_MCS)
		return -1;

	fseek(f, 18, SEEK_CUR);
	sng_ptr = read32b(f);
	fseek(f, sng_ptr, SEEK_SET);

	read_title(f, t, 32);
	
	return 0;
}

static int mgt_load(FILE *f)
{
	struct xxm_event *event;
	int i, j;
	int ver;
	int sng_ptr, seq_ptr, ins_ptr, pat_ptr, trk_ptr, smp_ptr;
	int sdata[64];

	LOAD_INIT();

	read24b(f);		/* MGT */
	ver = read8(f);
	read32b(f);		/* MCS */

	sprintf(xmp_ctl->type, "MGT v%d.%d (Megatracker)", MSN(ver), LSN(ver));

	xxh->chn = read16b(f);
	read16b(f);			/* number of songs */
	xxh->len = read16b(f);
	xxh->pat = read16b(f);
	xxh->trk = read16b(f);
	xxh->ins = xxh->smp = read16b(f);
	read16b(f);			/* reserved */
	read32b(f);			/* reserved */

	sng_ptr = read32b(f);
	seq_ptr = read32b(f);
	ins_ptr = read32b(f);
	pat_ptr = read32b(f);
	trk_ptr = read32b(f);
	smp_ptr = read32b(f);
	read32b(f);			/* total smp len */
	read32b(f);			/* unpacked trk size */

	fseek(f, sng_ptr, SEEK_SET);

	fread(xmp_ctl->name, 1, 32, f);
	seq_ptr = read32b(f);
	xxh->len = read16b(f);
	xxh->rst = read16b(f);
	xxh->bpm = read8(f);
	xxh->tpo = read8(f);
	read16b(f);			/* global volume */
	read8(f);			/* master L */
	read8(f);			/* master R */

	for (i = 0; i < xxh->chn; i++) {
		read16b(f);		/* pan */
	}
	
	MODULE_INFO();

	/* Sequence */

	fseek(f, seq_ptr, SEEK_SET);
	for (i = 0; i < xxh->len; i++)
		xxo[i] = read16b(f);

	/* Instruments */

	INSTRUMENT_INIT();

	fseek(f, ins_ptr, SEEK_SET);
	reportv(1, "     Name                             Len  LBeg LEnd L Vol C2Spd\n");

	for (i = 0; i < xxh->ins; i++) {
		int c2spd, flags;

		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		fread(xxih[i].name, 1, 32, f);
		sdata[i] = read32b(f);
		xxs[i].len = read32b(f);
		xxs[i].lps = read32b(f);
		xxs[i].lpe = xxs[i].lps + read32b(f);
		read32b(f);
		read32b(f);
		c2spd = read32b(f);
		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
		xxi[i][0].vol = read16b(f) >> 4;
		read8(f);		/* vol L */
		read8(f);		/* vol R */
		xxi[i][0].pan = 0x80;
		flags = read8(f);
		xxs[i].flg = flags & 0x03 ? WAVE_LOOPING : 0;
		xxs[i].flg |= flags & 0x02 ? WAVE_BIDIR_LOOP : 0;
		xxi[i][0].fin += 0 * read8(f);	// FIXME
		read8(f);		/* unused */
		read8(f);
		read8(f);
		read8(f);
		read16b(f);
		read32b(f);
		read32b(f);

		xxih[i].nsm = !!xxs[i].len;
		xxi[i][0].sid = i;
		
		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-32.32s %04x %04x %04x %c V%02x %5d\n",
				i, xxih[i].name,
				xxs[i].len, xxs[i].lps, xxs[i].lpe,
				xxs[i].flg & WAVE_BIDIR_LOOP ? 'B' :
					xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				xxi[i][0].vol, c2spd);
		}
	}

	/* PATTERN_INIT - alloc extra track*/
	PATTERN_INIT();

	reportv(0, "Stored tracks  : %d ", xxh->trk);

	/* Tracks */

	for (i = 1; i < xxh->trk; i++) {
		int offset, rows;
		uint8 b;

		fseek(f, trk_ptr + i * 4, SEEK_SET);
		offset = read32b(f);
		fseek(f, offset, SEEK_SET);

		rows = read16b(f);
		xxt[i] = calloc(sizeof(struct xxm_track) +
				sizeof(struct xxm_event) * rows, 1);
		xxt[i]->rows = rows;

		//printf("\n=== Track %d ===\n\n", i);
		for (j = 0; j < rows; j++) {
			uint8 note, f2p;

			b = read8(f);
			j += b & 0x03;

			note = 0;
			event = &xxt[i]->event[j];
			if (b & 0x04)
				note = read8(f);
			if (b & 0x08)
				event->ins = read8(f);
			if (b & 0x10)
				event->vol = read8(f);
			if (b & 0x20)
				event->fxt = read8(f);
			if (b & 0x40)
				event->fxp = read8(f);
			if (b & 0x80)
				f2p = read8(f);

			if (note == 1)
				event->note = XMP_KEY_OFF;
			else if (note > 11) /* adjusted to play codeine.mgt */
				event->note = note - 11;

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

		if (V(0) && i % xxh->chn == 0)
			report(".");
	}
	reportv(0, "\n");

	/* Extra track */
	xxt[0] = calloc(sizeof(struct xxm_track) +
			sizeof(struct xxm_event) * 64 - 1, 1);
	xxt[0]->rows = 64;

	/* Read and convert patterns */

	reportv(0, "Stored patterns: %d ", xxh->pat);
	fseek(f, pat_ptr, SEEK_SET);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);

		xxp[i]->rows = read16b(f);
		for (j = 0; j < xxh->chn; j++) {
			xxp[i]->info[j].index = read16b(f) - 1;
			//printf("%3d ", xxp[i]->info[j].index);
		}

		reportv(0, ".");
		//printf("\n");
	}
	reportv(0, "\n");

	/* Read samples */

	reportv(0, "Stored samples : %d ", xxh->smp);

	for (i = 0; i < xxh->ins; i++) {
		if (xxih[i].nsm == 0)
			continue;

		fseek(f, sdata[i], SEEK_SET);
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
						&xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
