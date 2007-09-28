/* DigiBoosterPRO module loader for xmp
 * Copyright (C) 1999-2007 Claudio Matsuoka
 *
 * $Id: dbm_load.c,v 1.2 2007-09-28 01:25:01 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on DigiBooster_E.guide from the DigiBoosterPro 2.20 package.
 * DigiBooster Pro written by Tomasz & Waldemar Piasta
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"

#define MAGIC_DBM0	MAGIC4('D','B','M','0')


static void get_info(int size, uint8 *buffer)
{
	xxh->ins = readmem16b(buffer);
	xxh->smp = readmem16b(buffer + 2);
	xxh->pat = readmem16b(buffer + 6);
	xxh->chn = readmem16b(buffer + 8);

	xxh->trk = xxh->pat * xxh->chn;

	INSTRUMENT_INIT();
}

static void get_song(int size, uint8 *buffer)
{
	if (V(0) && *buffer)
		report("Song name      : %-44.44s\n", buffer);

	buffer += 44;

	xxh->len = readmem16b(buffer);

	reportv(0, "Song length    : %d patterns\n", xxh->len);

	memcpy(xxo, buffer + 2, xxh->len);
}

static void get_inst(int size, uint8 *buffer)
{
	int i;
	int c2spd;

	reportv(0, "Instruments    : %d", xxh->ins);

	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		xxih[i].nsm = 1;
		copy_adjust(xxih[i].name, buffer, 30);
		xxi[i][0].sid = readmem16b(buffer + 30);
		xxi[i][0].vol = readmem16b(buffer + 32);
		c2spd = readmem32b(buffer + 34);
		xxs[i].lps = readmem32b(buffer + 38);
		xxs[i].lpe = xxs[i].lps + readmem32b(buffer + 42);
		/*xxs[i].flg = inst[i].looplen > 2 ? WAVE_LOOPING : 0; */
		xxi[i][0].pan = readmem16b(buffer + 46);

		if (V(1) && (*xxih[i].name || (xxs[i].len > 1))) {
			report("\n[%2X] %-30.30s %05x %05x %c %02x %02x", i,
				xxih[i].name, xxs[i].lps, xxs[i].lpe,
				xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				xxi[i][0].vol, xxi[i][0].pan);
		}

		buffer += 48;
	}
}

static void get_patt(int size, uint8 *buffer)
{
	int i, c, r, n, rows, sz;
	struct xxm_event *event, dummy;
	uint8 *p = buffer;

	PATTERN_INIT();

	reportv(0, "\nStored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		rows = readmem16b(p);
		p += 2;

		PATTERN_ALLOC(i);
		xxp[i]->rows = rows;
		TRACK_ALLOC(i);

		sz = readmem32b(p);
		p += 4;

		r = 0;
		c = -1;

		while (sz >= 0) {
			n = *p++;
			sz--;
			if (n == 0) {
				r++;
				c = -1;
				continue;
			}
			if (c >= *p)
				r++;
			c = *p++;
			sz--;
			event = c >= xxh->chn ? &dummy : &EVENT(i, c, r);
			if (n & 0x01) {
				event->note = MSN(*p) * 12 + LSN(*p);
				p++;
				sz--;
			}
			if (n & 0x02)
				sz--, event->ins = *p++ + 1;
			if (n & 0x04)
				sz--, event->fxt = *p++;
			if (n & 0x08)
				sz--, event->fxp = *p++;
			if (n & 0x10)
				sz--, event->f2t = *p++;
			if (n & 0x20)
				sz--, event->f2p = *p++;
		}
		reportv(0, ".");
	}
}

#if 0
static void get_sbod(int size, char *buffer)
{
	int flags;

	if (sample >= xxh->ins)
		return;

	if (!sample && V(0))
		report("\nStored samples : %d ", xxh->smp);

	flags = XMP_SMP_NOLOAD;
	if (mode[idx[sample]] == OKT_MODE8)
		flags |= XMP_SMP_7BIT;
	if (mode[idx[sample]] == OKT_MODEB)
		flags |= XMP_SMP_7BIT;
	xmp_drv_loadpatch(NULL, sample, xmp_ctl->c4rate, flags,
			  &xxs[idx[sample]], buffer);
	if (V(0))
		report(".");
	sample++;
}
#endif

int dbm_load(FILE *f)
{
	char name[44];
	uint16 version;

	LOAD_INIT();

	/* Check magic */
	if (read32b(f) != MAGIC_DBM0)
		return -1;

	version = read16b(f);

	fseek(f, 10, SEEK_CUR);
	fread(name, 1, 44, f);

	/* IFF chunk IDs */
	iff_register("INFO", get_info);
	iff_register("SONG", get_song);
	iff_register("INST", get_inst);
	iff_register("PATT", get_patt);
#if 0
	iff_register("SMPL", get_smpl);
	iff_register("VENV", get_venv);
#endif

	strncpy(xmp_ctl->name, name, XMP_DEF_NAMESIZE);
	strcpy(xmp_ctl->type, "DBM0");
	sprintf(tracker_name, "DigiBooster Pro %d.%0x",
		version >> 8, version & 0xff);

	MODULE_INFO();

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(f);

	iff_release();

	reportv(0, "\n");

	return 0;
}
