/* X-Tracker DMF loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: dmf_load.c,v 1.2 2007-08-08 00:46:51 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"
#include "period.h"

static int ver;

static void get_sequ(int size, FILE *f)
{
	int i;

	read16l(f);	/* sequencer loop start */
	read16l(f);	/* sequencer loop end */

	xxh->len = (size - 4) / 2;
	if (xxh->len > 255)
		xxh->len = 255;

	for (i = 0; i < xxh->len; i++)
		xxo[i] = read16l(f);
}

static void get_patt(int size, FILE *f)
{
	int i, j, r, chn;
	int patsize;
	int info, counter, data;
	int track_counter[32];
	struct xxm_event *event;

	xxh->pat = read16l(f);
	xxh->chn = read8(f);
	xxh->trk = xxh->chn * xxh->pat;

	PATTERN_INIT();

	if (V(0))
		report("Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		chn = read8(f);
		read8(f);		/* beat */
		xxp[i]->rows = read16l(f);
		TRACK_ALLOC(i);

		patsize = read32l(f);

		for (j = 0; j < chn; j++)
			track_counter[j] = 0;

		for (counter = r = 0; r < xxp[i]->rows; r++) {
			if (counter == 0) {
				/* global track */
				info = read8(f);
				counter = info & 0x80 ? read8(f) : 0;
				data = read8(f);
			} else {
				counter--;
			}

			for (j = 0; j < chn; j++) {
				int b;

				event = &EVENT(i, j, r);

				if (track_counter[j] == 0) {
					b = read8(f);
		
					if (b & 0x80)
						track_counter[j] = read8(f);
					if (b & 0x40)
						event->ins = read8(f);
					if (b & 0x20)
						event->note = read8(f);
					if (b & 0x10)
						event->vol = read8(f);
					if (b & 0x08) {	/* instrument effect */
						read8(f);
						read8(f);
					}
					if (b & 0x04) {	/* note effect */
						read8(f);
						read8(f);
					}
					if (b & 0x02) {	/* volume effect */
						read8(f);
						read8(f);
					}
				} else {
					track_counter[j]--;
				}
			}
		}
		if (V(0)) report(".");
	}
	if (V(0)) report("\n");
}

static void get_smpi(int size, FILE *f)
{
	int i, namelen, c3spd, flag;
	uint8 name[30];

	xxh->ins = read8(f);
	xxh->smp = xxh->ins;

	INSTRUMENT_INIT();

	if (V(1))
		report ("     Instrument name        Len  LBeg LEnd L Vol Fin\n");

	for (i = 0; i < xxh->ins; i++) {
		int x;

		xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
		
		namelen = read8(f);
		x = namelen - fread(name, 1, namelen > 30 ? 30 : namelen, f);
		copy_adjust(xxih[i].name, name, namelen);
		name[namelen] = 0;
		while (x--)
			read8(f);


		xxs[i].len = read32l(f);
		xxs[i].lps = read32l(f);
		xxs[i].lpe = read32l(f);
		c3spd = read16l(f);
		xxi[i][0].vol = read8(f);
		flag = read8(f);
		if (ver >= 8)
			fseek(f, 8, SEEK_CUR);	/* library name */
		read16l(f);	/* reserved -- specs say 1 byte only*/
		read32l(f);	/* sampledata crc32 */

		if (V(1) & (strlen((char *)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-30.30s %05x %05x %05x %c %5d V%02x\n",
				i, name, xxs[i].len, xxs[i].lps & 0xfffff,
				xxs[i].lpe & 0xfffff,
				xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				c3spd, xxi[i][0].vol);
		}
	}
}

static void get_smpd(int size, FILE *f)
{
}

int dmf_load(FILE *f)
{
	char magic[4];
	char composer[XMP_DEF_NAMESIZE];
	uint8 date[3];

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp(magic, "DDMF", 4))
		return -1;

	ver = read8(f);
	snprintf(xmp_ctl->type, XMP_DEF_NAMESIZE,
				"D-Lusion Digital Music File v%d", ver);
	fread(tracker_name, 8, 1, f);
	tracker_name[8] = 0;
	fread(xmp_ctl->name, 30, 1, f);
	fread(composer, 20, 1, f);
	fread(date, 3, 1, f);
	
	xxh->smp = xxh->ins = 0;

	MODULE_INFO();
	if (V(0)) {
		report("Composer name  : %s\n", composer);
		report("Creation date  : %02d/%02d/%04d\n", date[0], date[1],
							1900 + date[2]);
	}
	
	/* IFF chunk IDs */
	iff_register("SEQU", get_sequ);
	iff_register("PATT", get_patt);
	iff_register("SMPI", get_smpi);
	iff_register("SMPD", get_smpd);
	iff_setflag(IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(f);

	iff_release();

	return 0;
}

