/* Epic Megagames MASI PSM loader for xmp
 * Copyright (C) 2005 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: psm_load.c,v 1.18 2005-02-21 02:29:20 cmatsuoka Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"
#include "period.h"

static int sinaria;


static int cur_pat;
static int cur_ins;
uint8 *pnam;
uint8 *pord;


static void get_sdft(int size, uint16 *buffer)
{
}

static void get_titl(int size, uint16 *buffer)
{
	strncpy(xmp_ctl->name, (char *)buffer, size > 32 ? 32 : size);
}

static void get_dsmp_cnt(int size, uint16 *buffer)
{
	xxh->ins++;
	xxh->smp = xxh->ins;
}

static void get_pbod_cnt(int size, uint8 *buffer)
{
	xxh->pat++;
	if (buffer[9] != 0 && buffer[13] == 0)
		sinaria = 1;
}


static void get_dsmp(int size, void *buffer)
{
	int i, srate;
	uint8 *p;

	p = buffer;

	p++;			/* flags */
	p += 8;			/* songname */
	p += sinaria ? 8 : 4;	/* smpid */

#if 0
	pi = (struct psm_ins *)p;

	L_ENDIAN32(pi->length);
	L_ENDIAN32(pi->loopstart);
	L_ENDIAN32(pi->loopend);
	L_ENDIAN32(pi->samplerate);

	/* for jjxmas95 xm3 */
	if ((int)pi->loopend == -1)
		pi->loopend = 0;
#endif

	if (V(1) && cur_ins == 0)
	    report("\n     Instrument name                   Len  LBeg  LEnd  L Vol C2Spd");

	i = cur_ins;
	xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

	strncpy((char *)xxih[i].name, p, 32); p += 34;
	str_adj ((char *)xxih[i].name);
	p += 5;
	p++;		/* insno */
	p++;
	xxs[i].len = readbuf32l(p); p += 4;
	xxih[i].nsm = !!(xxs[i].len);
	xxs[i].lps = readbuf32l(p); p += 4;
	xxs[i].lpe = readbuf32l(p); p += 4;
	xxs[i].flg = xxs[i].lpe > 2 ? WAVE_LOOPING : 0;
	p += 2;

	if (sinaria) p++;

	xxi[i][0].vol = *p++ / 2 + 1;
	p += 4;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	srate = readbuf32l(p);

	c2spd_to_note(srate, &xxi[i][0].xpo, &xxi[i][0].fin);

	if ((V(1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 1)))
	    report ("\n[%2X] %-32.32s %05x %05x %05x %c V%02x %5d", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, srate);

	xmp_drv_loadpatch (NULL, i, xmp_ctl->c4rate,
		XMP_SMP_NOLOAD | XMP_SMP_8BDIFF, &xxs[i],
		(uint8 *)buffer + 96);

	cur_ins++;
}


static void get_pbod (int size, void *buffer)
{
	int i, r;
	struct xxm_event *event, dummy;
	char *p;
	uint8 f, c;
	uint32 len;
	int rows, pos, rowlen;

	i = cur_pat;
	p = buffer;

	len = *(uint32 *)p;
	p += 4;
	L_ENDIAN32(len);
	if (sinaria) {
		memcpy(pnam + i * 8, p, 8);
		p += 8;
	} else {
		memcpy(pnam + i * 8, p, 4);
		p += 4;
	}

	rows = 256 * p[1] + p[0];
	p += 2;

	PATTERN_ALLOC(i);
	xxp[i]->rows = rows;
	TRACK_ALLOC(i);

	pos = 0;
	r = 0;

	do {
		rowlen = 256 * p[pos + 1] + p[pos] - 2;
		pos += 2;
		while (rowlen > 0) {
	
			f = p[pos++];
	
			if (rowlen == 1)
				break;
	
			c = p[pos++];
			rowlen -= 2;
	
if (f & 0x0f) printf("p%d r%d c%d: unknown flag %02x\n", i, r, c, f);

			event = c < xxh->chn ? &EVENT(i, c, r) : &dummy;
	
			if (f & 0x80) {
				uint8 note = p[pos++];
				rowlen--;
				if (sinaria)
					note += 25;
				else
					note = (note >> 4) * 12 + (note & 0x0f) + 2;
				event->note = note;
			}

			if (f & 0x40) {
				event->ins = p[pos++] + 1;
				rowlen--;
			}
	
			if (f & 0x20) {
				event->vol = p[pos++] / 2;
				rowlen--;
			}
	
			if (f & 0x10) {
				uint8 fxt = p[pos++];
				uint8 fxp = p[pos++];
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
printf("p%d r%d c%d: compressed event %02x %02x\n", i, r, c, fxt, fxp);
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
					fxt = FX_VIBRATO;
					/* fxp remains the same */
					break;
				case 0x2a:		/* retrig note */
					fxt = FX_EXTENDED;
					fxp = (EX_RETRIG << 4) | (fxp & 0x0f); 
					break;
				case 0x29:		/* unknown */
/*printf("\np%d r%d c%d: effect 0x29: %02x %02x %02x %02x", i, r, c, fxt, fxp, p[pos], p[pos + 1]);*/
					pos += 2;
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
printf("p%d r%d c%d: unknown effect %02x %02x\n", i, r, c, fxt, fxp);
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

static void get_song(int size, char *buffer)
{
	xxh->chn = *(buffer + 8 + 2);
	if (*xmp_ctl->name == 0)
		strncpy(xmp_ctl->name, buffer, 8);
}

static void get_song_2(int size, char *buffer)
{
	char *p;
	uint32 oplh_size;
	int i;

	xxh->len = 0;

	p = buffer + 11;		/* Point to first sub-chunk */

	while (strncmp(p, "OPLH", 4)) {
		int skip;
		p += 4;
		skip = *(uint32 *)p;
		L_ENDIAN32(skip);
		p += 4 + skip;
	}

	p += 4;
	oplh_size = *(uint32 *)p;
	L_ENDIAN32(oplh_size);
	p += 4;

	p += 9;		/* unknown data */
	
	for (i = 0; *p != 0x01; ) {
		switch (*p++) {
		case 0x07:
			xxh->tpo = *(uint8 *)p++;
			p++;		/* 08 */
			xxh->bpm = *(uint8 *)p++;
			break;
		case 0x0d:
			p++;		/* channel number? */
			xxc[i].pan = *(uint8 *)p++;
			p++;		/* flags? */
			i++;
			break;
		case 0x0e:
			p++;		/* channel number? */
			p++;		/* ? */
			break;
		default:
printf("channel %d: %02x %02x\n", i, *(p - 1), *p);

		}
	}

	for (; *p == 0x01; p += 1 + (sinaria ? 8 : 4)) {
		memcpy(pord + xxh->len * 8, p + 1, sinaria ? 8 : 4);
		xxh->len++;
	}
}

int psm_load(FILE *f)
{
	char magic[4];
	int offset;
	int i, j;

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp (magic, "PSM ", 4))
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

	if (V(0))
		report("\n");

	return 0;
}

