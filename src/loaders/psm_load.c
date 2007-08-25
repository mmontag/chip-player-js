/* Epic Megagames MASI PSM loader for xmp
 * Copyright (C) 2005-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: psm_load.c,v 1.23 2007-08-25 11:45:17 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * Originally based on the PSM loader from Modplug by Olivier Lapicque and
 * fixed comparing the One Must Fall! PSMs with Kenny Chou's MTM files.
 */

/*
 * From EPICTEST Readme.1st:
 *
 * The Music And Sound Interface, MASI, is the basis behind all new Epic
 * games. MASI uses its own proprietary file format, PSM, for storing
 * its music.
 */

/*
 * kode54's comment on Sinaria PSMs in the foo_dumb hydrogenaudio forum:
 *
 * "The Sinaria variant uses eight character pattern and instrument IDs,
 * the sample headers are laid out slightly different, and the patterns
 * use a different format for the note values, and also different effect
 * scales for certain commands.
 *
 * [Epic] PSM uses high nibble for octave and low nibble for note, for
 * a valid range up to 0x7F, for a range of D-1 through D#9 compared to
 * IT. (...) Sinaria PSM uses plain note values, from 1 - 83, for a
 * range of C-3 through B-9.
 *
 * [Epic] PSM also uses an effect scale for portamento, volume slides,
 * and vibrato that is about four times as sensitive as the IT equivalents.
 * Sinaria does not. This seems to coincide with the MOD/S3M to PSM
 * converter that Joshua Jensen released in the EPICTEST.ZIP file which
 * can still be found on a few FTP sites. It converted effects literally,
 * even though the bundled players behaved as the libraries used with
 * Epic's games did and made the effects sound too strong."
 */

/*
 * Claudio's note: Sinaria seems to have a finetune byte just before
 * volume, slightly different sample size (subtract 2 bytes?) and some
 * kind of (stereo?) interleaved sample, with 16-byte frames (see Sinaria
 * songs 5 and 8). Sinaria song 10 sounds ugly, possibly caused by wrong
 * pitchbendings (see note above).
 */

/* FIXME: TODO: sinaria effects */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"
#include "period.h"

#define MAGIC_PSM_	MAGIC4('P','S','M',' ')
#define MAGIC_OPLH	MAGIC4('O','P','L','H')


static int sinaria;

static int cur_pat;
static int cur_ins;
uint8 *pnam;
uint8 *pord;


static void get_sdft(int size, FILE *f)
{
}

static void get_titl(int size, FILE *f)
{
	char buf[40];
	
	fread(buf, 1, 40, f);
	strncpy(xmp_ctl->name, buf, size > 32 ? 32 : size);
}

static void get_dsmp_cnt(int size, FILE *f)
{
	xxh->ins++;
	xxh->smp = xxh->ins;
}

static void get_pbod_cnt(int size, FILE *f)
{
	char buf[20];

	xxh->pat++;
	fread(buf, 1, 20, f);
	if (buf[9] != 0 && buf[13] == 0)
		sinaria = 1;
}


static void get_dsmp(int size, FILE *f)
{
	int i, srate;
	int finetune;

	read8(f);				/* flags */
	fseek(f, 8, SEEK_CUR);			/* songname */
	fseek(f, sinaria ? 8 : 4, SEEK_CUR);	/* smpid */

	if (V(1) && cur_ins == 0)
	    report("\n     Instrument name                  Len   LBeg  LEnd  L Vol Fine C2Spd");

	i = cur_ins;
	xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

	fread(&xxih[i].name, 1, 34, f);
	str_adj((char *)xxih[i].name);
	fseek(f, 5, SEEK_CUR);
	read8(f);		/* insno */
	read8(f);
	xxs[i].len = read32l(f);
	xxih[i].nsm = !!(xxs[i].len);
	xxs[i].lps = read32l(f);
	xxs[i].lpe = read32l(f);
	xxs[i].flg = xxs[i].lpe > 2 ? WAVE_LOOPING : 0;
	read16l(f);

	if ((int32)xxs[i].lpe < 0)
		xxs[i].lpe = 0;

	finetune = 0;
	if (sinaria) {
		if (xxs[i].len > 2)
			xxs[i].len -= 2;
		if (xxs[i].lpe > 2)
			xxs[i].lpe -= 2;

		finetune = (int8)(read8s(f) << 4);
	}

	xxi[i][0].vol = read8(f) / 2 + 1;
	read32l(f);
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	srate = read32l(f);

	if ((V(1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 1)))
	    report ("\n[%2X] %-32.32s %05x %05x %05x %c V%02x %+04d %5d", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, finetune, srate);

	srate = 8363 * srate / 8448;
	c2spd_to_note(srate, &xxi[i][0].xpo, &xxi[i][0].fin);
	xxi[i][0].fin += finetune;

	fseek(f, 16, SEEK_CUR);
	xmp_drv_loadpatch(f, i, xmp_ctl->c4rate, XMP_SMP_8BDIFF, &xxs[i], NULL);

	cur_ins++;
}


static void get_pbod(int size, FILE *f)
{
	int i, r;
	struct xxm_event *event, dummy;
	uint8 flag, chan;
	uint32 len;
	int rows, rowlen;

	i = cur_pat;

	len = read32l(f);
	fread(pnam + i * 8, 1, sinaria ? 8 : 4, f);

	rows = read16l(f);

	PATTERN_ALLOC(i);
	xxp[i]->rows = rows;
	TRACK_ALLOC(i);

	r = 0;

	do {
		rowlen = read16l(f) - 2;
		while (rowlen > 0) {
			flag = read8(f);
	
			if (rowlen == 1)
				break;
	
			chan = read8(f);
			rowlen -= 2;
	
			event = chan < xxh->chn ? &EVENT(i, chan, r) : &dummy;
	
			if (flag & 0x80) {
				uint8 note = read8(f);
				rowlen--;
				if (sinaria)
					note += 25;
				else
					note = (note >> 4) * 12 + (note & 0x0f) + 2;
				event->note = note;
			}

			if (flag & 0x40) {
				event->ins = read8(f) + 1;
				rowlen--;
			}
	
			if (flag & 0x20) {
				event->vol = read8(f) / 2;
				rowlen--;
			}
	
			if (flag & 0x10) {
				uint8 fxt = read8(f);
				uint8 fxp = read8(f);
				rowlen -= 2;
	
				/* compressed events */
				if (fxt >= 0x40) {
					switch (fxp >> 4) {
					case 0x0: {
						uint8 note;
						note = (fxt>>4)*12 +
							(fxt & 0x0f) + 2;
						event->note = note;
						fxt = FX_TONEPORTA;
						fxp = (fxp + 1) * 2;
						break; }
					default:
printf("p%d r%d c%d: compressed event %02x %02x\n", i, r, chan, fxt, fxp);
					}
				} else
				switch (fxt) {
				case 0x01:		/* fine volslide up */
					fxt = FX_EXTENDED;
					fxp = (EX_F_VSLIDE_UP << 4) |
						((fxp / 2) & 0x0f);
					break;
				case 0x02:		/* volslide up */
					fxt = FX_VOLSLIDE;
					fxp = (fxp / 2) << 4;
					break;
				case 0x03:		/* fine volslide down */
					fxt = FX_EXTENDED;
					fxp = (EX_F_VSLIDE_DN << 4) |
						((fxp / 2) & 0x0f);
					break;
				case 0x04: 		/* volslide down */
					fxt = FX_VOLSLIDE;
					fxp /= 2;
					break;
			    	case 0x0C:		/* portamento up */
					fxt = FX_PORTA_UP;
					fxp = (fxp - 1) / 2;
					break;
				case 0x0E:		/* portamento down */
					fxt = FX_PORTA_DN;
					fxp = (fxp - 1) / 2;
					break;
				case 0x0f:		/* tone portamento */
					fxt = FX_TONEPORTA;
					fxp /= 4;
					break;
				case 0x15:		/* vibrato */
					fxt = sinaria ?
						FX_VIBRATO : FX_FINE4_VIBRA;
					/* fxp remains the same */
					break;
				case 0x2a:		/* retrig note */
					fxt = FX_EXTENDED;
					fxp = (EX_RETRIG << 4) | (fxp & 0x0f); 
					break;
				case 0x29:		/* unknown */
					read16l(f);
					rowlen -= 2;
					break;
				case 0x33:		/* position Jump */
					fxt = FX_JUMP;
					break;
			    	case 0x34:		/* pattern break */
					fxt = FX_BREAK;
					break;
				case 0x3D:		/* speed */
					fxt = FX_TEMPO;
					break;
				case 0x3E:		/* tempo */
					fxt = FX_TEMPO;
					break;
				default:
printf("p%d r%d c%d: unknown effect %02x %02x\n", i, r, chan, fxt, fxp);
					fxt = fxp = 0;
				}
	
				event->fxt = fxt;
				event->fxp = fxp;
			}
		}
		r++;
	} while (r < rows);

	cur_pat++;
}

static void get_song(int size, FILE *f)
{
	fseek(f, 10, SEEK_CUR);
	xxh->chn = read8(f);
	if (*xmp_ctl->name == 0)
		fread(&xmp_ctl->name, 1, 8, f);
}

static void get_song_2(int size, FILE *f)
{
	uint32 magic;
	char c;
	int i;

	xxh->len = 0;

	fseek(f, 11, SEEK_CUR);		/* Point to first sub-chunk */

	magic = read32b(f);
	while (magic != MAGIC_OPLH) {
		int skip;
		skip = read32l(f);;
		fseek(f, skip, SEEK_CUR);
		magic = read32b(f);
	}

	read32l(f);	/* chunk size */

	fseek(f, 9, SEEK_CUR);		/* unknown data */
	
	c = read8(f);
	for (i = 0; c != 0x01; c = read8(f)) {
		switch (c) {
		case 0x07:
			xxh->tpo = read8(f);
			read8(f);		/* 08 */
			xxh->bpm = read8(f);
			break;
		case 0x0d:
			read8(f);		/* channel number? */
			xxc[i].pan = read8(f);
			read8(f);		/* flags? */
			i++;
			break;
		case 0x0e:
			read8(f);		/* channel number? */
			read8(f);		/* ? */
			break;
		default:
			printf("channel %d: %02x %02x\n", i, c, read8(f));
		}
	}

	for (; c == 0x01; c = read8(f)) {
		fread(pord + xxh->len * 8, 1, sinaria ? 8 : 4, f);
		xxh->len++;
	}
}

int psm_load(FILE *f)
{
	int offset;
	int i, j;

	LOAD_INIT ();

	/* Check magic */
	if (read32b(f) != MAGIC_PSM_)
		return -1;

	sinaria = 0;

	fseek(f, 8, SEEK_CUR);		/* skip file size and FILE */
	xxh->smp = xxh->ins = 0;
	cur_pat = 0;
	cur_ins = 0;
	offset = ftell(f);

	/* IFF chunk IDs */
	iff_register("TITL", get_titl);
	iff_register("SDFT", get_sdft);
	iff_register("SONG", get_song);
	iff_register("DSMP", get_dsmp_cnt);
	iff_register("PBOD", get_pbod_cnt);
	iff_setflag(IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof (f))
		iff_chunk(f);

	iff_release();

	xxh->trk = xxh->pat * xxh->chn;
	pnam = malloc(xxh->pat * 8);		/* pattern names */
	pord = malloc(255 * 8);			/* pattern orders */

	strcpy (xmp_ctl->type, sinaria ?
		"Sinaria MASI (PSM)" : "Epic Megagames MASI (PSM)");

	MODULE_INFO();
	INSTRUMENT_INIT();
	PATTERN_INIT();

	if (V(0)) {
	    report("Stored patterns: %d\n", xxh->pat);
	    report("Stored samples : %d", xxh->smp);
	}

	fseek(f, offset, SEEK_SET);

	iff_register("SONG", get_song_2);
	iff_register("DSMP", get_dsmp);
	iff_register("PBOD", get_pbod);
	iff_setflag(IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof (f))
		iff_chunk(f);

	iff_release ();

	for (i = 0; i < xxh->len; i++) {
		for (j = 0; j < xxh->pat; j++) {
			if (!memcmp(pord + i * 8, pnam + j * 8, sinaria ? 8 : 4)) {
				xxo[i] = j;
				break;
			}
		}

		if (j == xxh->pat)
			break;
	}

	free(pnam);
	free(pord);

	reportv(0, "\n");

	return 0;
}

