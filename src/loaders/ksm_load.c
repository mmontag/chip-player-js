/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Kefrens Sound Machine loader based on the desciption written by
 * Sylvain Chipaux. Tested with the modules sent by BuZz:
 *
 * KSM.dragonjive
 * KSM.music
 * KSM.termination
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct ksm_ins {
    uint8 unk1[16];		/* ?!? */
    uint32 addr;		/* Sample address in file */
    uint16 len;			/* Sample length */
    uint8 volume;		/* Volume (0-63) */
    uint8 unk2;			/* ?!? */
    uint16 loop_start;		/* Sample loop start */
    uint16 unk3[3];		/* ?!? */
} PACKED;

struct ksm_header {
    uint8 id1[2];		/* "M." */
    uint8 title[13];		/* Title */
    uint8 id2;			/* "a" */
    uint8 unk1[16];		/* ?!? (16 empty bytes) */
    struct ksm_ins ins[15];	/* Instruments */
    uint8 trkidx[1020];		/* Track indexes */
    uint32 id3;			/* 0xff 0xff 0xff 0xff */
} PACKED;


int ksm_load (FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct ksm_header kh;
    uint8 ev[3];

    LOAD_INIT ();

    fread (&kh, 1, sizeof (kh), f);

    B_ENDIAN32 (kh.id3);

    if (kh.id1[0] != 'M' || kh.id1[1] != '.' || kh.id2 != 'a' ||
	(kh.id3 != 0xffffffff && kh.id3 !=0x00000000))
	return -1;

    strncpy (xmp_ctl->name, (char *)kh.title, 13);
    strcpy (xmp_ctl->type, "Kefrens Sound Machine");
    MODULE_INFO ();

    xxh->ins = 15;
    xxh->smp = xxh->ins;
    xxh->flg |= XXM_FLG_MODRNG;

    for (xxh->trk = j = i = 0; kh.trkidx[i] != 0xff; i++)
	if (kh.trkidx[i] > xxh->trk)
	    xxh->trk = kh.trkidx[i];
    xxh->trk++;
    xxh->pat = xxh->len = i >> 2;

    for (i = 0; i < xxh->len; i++)
	xxo[i] = i;

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN32 (kh.ins[i].addr);
	B_ENDIAN16 (kh.ins[i].len);
	B_ENDIAN16 (kh.ins[i].loop_start);
    }

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Len  LBeg LEnd L Vl Ft\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = kh.ins[i].len);
	xxs[i].lps = kh.ins[i].loop_start;
	xxs[i].lpe = xxs[i].len;
#if 0
	xxs[i].flg = xxs[i].lpe - xxs[i].lps > 4 ? WAVE_LOOPING : 0;
#endif
	xxi[i][0].vol = kh.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	if (V (1) && xxs[i].len > 2)
	    report ("[%2X] %04x %04x %04x %c %02x %+01x\n",
		i, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol,
		xxi[i][0].fin >> 4);
    }

    PATTERN_INIT ();

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;

	for (j = 0; j < xxh->chn; j++)
	    xxp[i]->info[j].index = kh.trkidx[i * xxh->chn + j];

	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\nStored tracks  : %d ", xxh->trk);

    for (i = 0; i < xxh->trk; i++) {
	xxt[i] = calloc (sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * 64, 1);
	xxt[i]->rows = 64;

	for (j = 0; j < 64; j++) {
	    event = &xxt[i]->event[j];
	    fread (ev, 1, 3, f);
	    if ((event->note = ev[0]))
		event->note += 36;
	    event->ins = MSN (ev[1]);
	    if ((event->fxt = LSN (ev[1])) == 0x0d)
		event->fxt = 0x0a;
	    event->fxp = ev[2];
	}

	if (V (0) && !(i % xxh->chn))
	    report (".");
    }

    /* Read samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	if (xxs[i].len <= 4)
	    continue;
	fseek (f, kh.ins[i].addr, SEEK_SET);
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
