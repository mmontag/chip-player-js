/* Megatracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: mgt_load.c,v 1.1 2007-08-29 00:52:06 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_MGT	MAGIC4(0x00,'M','G','T')
#define MAGIC_MCS	MAGIC4(0xbd,'M','C','S')


int mgt_load(FILE *f)
{
	struct xxm_event *event;
	int i, j, k;
	int ver;
	int sng_ptr, seq_ptr, ins_ptr, pat_ptr, trk_ptr, smp_ptr;

	LOAD_INIT();

	if (read24b(f) != MAGIC_MGT)
		return -1;
	ver = read8(f);
	if (read32b(f) != MAGIC_MCS)
		return -1;

	sprintf(xmp_ctl->type, "MGT v%d.%d (Megatracker)", MSN(ver), LSN(ver));

	xxh->chn = read16b(f);
	read16b(f);			/* number of songs */
	xxh->len = read16b(f);
	xxh->pat = read16b(f);
	xxh->trk = read16b(f);
	xxh->ins = xxh->smp = read16b(f);
	read16b(f);			/* reserved */

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
	xxh->gvl = read16b(f);
	read8(f);			/* master L */
	read8(f);			/* master R */

	for (i = 0; i < xxh->chn; i++) {
		read16b(f);		/* pan */
	}
	
	MODULE_INFO();

	fseek(f, seq_ptr, SEEK_SET);

	for (i = 0; i < xxh->len; i++)
		xxo[i] = read16b(f);


	INSTRUMENT_INIT();

	/* Read instrument names */
	reportv(1, "     Name      Len  LBeg LEnd L Vol  ?? ?? ??\n");
	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-8.8s  %04x %04x %04x %c V%02x\n", i,
			       xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol);
		}

	}

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
				event = &EVENT (i, k, j);
			}
		}
		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Read samples */
	reportv(0, "Stored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->ins; i++) {
		/* xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_UNS, &xxs[xxi[i][0].sid], NULL);*/
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
