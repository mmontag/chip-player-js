/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on the format description written by Sylvain Chipaux. Tested
 * with modules converted by NoiseConverter.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct di_ins {
    uint16 len;			/* Sample length in words */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in bytes */
    uint16 loop_length;		/* Sample loop length in words */
} PACKED;

struct di_header {
    uint32 tptr;		/* Offset of pattern table */
    uint32 pptr;		/* Offset of pattern data */
    uint32 sptr;		/* Offset of sample data */
    struct di_ins ins[31];	/* Instruments */
} PACKED;


int di_load (FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct di_header di;
    uint8 x, y;
    uint16 ins;
    int smp_size;

    LOAD_INIT ();

    fread (&ins, 2, 1, f);
    B_ENDIAN16 (ins);
    if (ins == 0 || ins > 31)
	return -1;

    fread (&di, 1, 12 + ins * sizeof (struct di_ins), f);

    B_ENDIAN32 (di.tptr);
    B_ENDIAN32 (di.pptr);
    B_ENDIAN32 (di.sptr);

    xxh->ins = ins;
    xxh->smp = xxh->ins;
    xxh->pat = (di.tptr - ftell (f)) / 2;
    xxh->trk = xxh->chn * xxh->pat;

    for (smp_size = i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (di.ins[i].len);
	B_ENDIAN16 (di.ins[i].loop_start);
	B_ENDIAN16 (di.ins[i].loop_length);
	smp_size += 2 * di.ins[i].len;
    }

    if (xmp_ctl->size != di.sptr + smp_size)
	return -1;

    fseek (f, 2 * xxh->pat, SEEK_CUR);

    for (xxh->len = i = 0; i < 256; i++) {
	fread (&xxo[i], 1, 1, f);
	if (xxo[i] == 0xff)
	    break;
	xxh->len++;
    }

    strcpy (xmp_ctl->type, "Digital Illusions");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Len  LBeg LEnd L Vol\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = 2 * di.ins[i].len);
	xxs[i].lps = di.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * di.ins[i].loop_length;
	xxs[i].flg = di.ins[i].loop_length > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = di.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	if (V (1) && xxs[i].len > 2)
	    report ("[%2X] %04x %04x %04x %c %02x\n",
		i, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol);
    }

    PATTERN_INIT ();

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	for (j = 0; j < 64 * xxh->chn; j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (&x, 1, 1, f);

	    if (x == 0xff)
		continue;
	    
	    fread (&y, 1, 1, f);
	    event->note = ((x & 0x3) << 4) | MSN (y);
	    if (event->note)
		event->note += 36;
	    event->ins = (x & 0x7c) >> 2;
	    event->fxt = LSN (y);

	    if (x & 0x80) {
		fread (&y, 1, 1, f);
		event->fxp = y;
	    }

	    disable_continue_fx (event);
	}
	if (V (0))
	    report (".");
    }

    /* Read samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	if (xxs[i].len <= 2)
	    continue;
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
