/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: stx_load.c,v 1.7 2007-10-13 18:25:05 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* From the STMIK 0.2 documentation:
 *
 * "The STMIK uses a special [Scream Tracker] beta-V3.0 module format.
 *  Due to the module formats beta nature, the current STMIK uses a .STX
 *  extension instead of the normal .STM. I'm not intending to do a
 *  STX->STM converter, so treat STX as the format to be used in finished
 *  programs, NOT as a format to be used in distributing modules. A program
 *  called STM2STX is included, and it'll convert STM modules to the STX
 *  format for usage in your own programs."
 *
 * Tested using "Future Brain" from Mental Surgery by Future Crew and
 * STMs converted with STM2STX.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "stx.h"
#include "s3m.h"
#include "period.h"


static int stx_test (FILE *, char *);
static int stx_load (FILE *);

struct xmp_loader_info stx_loader = {
    "STX",
    "STMIK 0.2",
    stx_test,
    stx_load
};

static int stx_test(FILE *f, char *t)
{
    char buf[8];

    fseek(f, 20, SEEK_SET);
    fread(buf, 8, 1, f);
    if (memcmp(buf, "!Scream!", 8) && memcmp(buf, "BMOD2STM", 8))
	return -1;

    fseek(f, 60, SEEK_SET);
    fread(buf, 4, 1, f);
    if (memcmp(buf, "SCRM", 4))
	return -1;

    fseek(f, 0, SEEK_SET);
    read_title(f, t, 20);

    return 0;
}

#define FX_NONE 0xff

static uint16 *pp_ins;		/* Parapointers to instruments */
static uint16 *pp_pat;		/* Parapointers to patterns */

static uint8 fx[] = {
    FX_NONE,		FX_TEMPO,
    FX_JUMP,		FX_BREAK,
    FX_VOLSLIDE,	FX_PORTA_DN,
    FX_PORTA_UP,	FX_TONEPORTA,
    FX_VIBRATO,		FX_TREMOR,
    FX_ARPEGGIO
};


static int stx_load(FILE *f)
{
    int c, r, i, broken = 0;
    struct xxm_event *event = 0, dummy;
    struct stx_file_header sfh;
    struct stx_instrument_header sih;
    uint8 n, b;
    uint16 x16;
    int bmod2stm = 0;

    LOAD_INIT ();

    fread(&sfh.name, 20, 1, f);
    fread(&sfh.magic, 8, 1, f);
    sfh.psize = read16l(f);
    sfh.unknown1 = read16l(f);
    sfh.pp_pat = read16l(f);
    sfh.pp_ins = read16l(f);
    sfh.pp_chn = read16l(f);
    sfh.unknown2 = read16l(f);
    sfh.unknown3 = read16l(f);
    sfh.gvol = read8(f);
    sfh.tempo = read8(f);
    sfh.unknown4 = read16l(f);
    sfh.unknown5 = read16l(f);
    sfh.patnum = read16l(f);
    sfh.insnum = read16l(f);
    sfh.ordnum = read16l(f);
    sfh.unknown6 = read16l(f);
    sfh.unknown7 = read16l(f);
    sfh.unknown8 = read16l(f);
    fread(&sfh.magic2, 4, 1, f);

    /* BMOD2STM does not convert pitch */
    if (!strncmp ((char *) sfh.magic, "BMOD2STM", 8))
	bmod2stm = 1;

#if 0
    if ((strncmp ((char *) sfh.magic, "!Scream!", 8) &&
	!bmod2stm) || strncmp ((char *) sfh.magic2, "SCRM", 4))
	return -1;
#endif

    xxh->ins = sfh.insnum;
    xxh->pat = sfh.patnum;
    xxh->trk = xxh->pat * xxh->chn;
    xxh->len = sfh.ordnum;
    xxh->tpo = MSN (sfh.tempo);
    xxh->smp = xxh->ins;
    xmp_ctl->c4rate = C4_NTSC_RATE;

    /* STM2STX 1.0 released with STMIK 0.2 converts STMs with the pattern
     * length encoded in the first two bytes of the pattern (like S3M).
     */
    fseek (f, sfh.pp_pat << 4, SEEK_SET);
    x16 = read16l(f);
    fseek (f, x16 << 4, SEEK_SET);
    x16 = read16l(f);
    if (x16 == sfh.psize)
	broken = 1;

    strncpy(xmp_ctl->name, (char *)sfh.name, 20);
    if (bmod2stm)
	sprintf(xmp_ctl->type, "STMIK 0.2 (BMOD2STM)");
    else
	snprintf(xmp_ctl->type, XMP_DEF_NAMESIZE, "STMIK 0.2 (STM2STX 1.%d)",
							broken ? 0 : 1);

    MODULE_INFO ();
 
    pp_pat = calloc (2, xxh->pat);
    pp_ins = calloc (2, xxh->ins);

    /* Read pattern pointers */
    fseek (f, sfh.pp_pat << 4, SEEK_SET);
    for (i = 0; i < xxh->pat; i++)
	pp_pat[i] = read16l(f);

    /* Read instrument pointers */
    fseek (f, sfh.pp_ins << 4, SEEK_SET);
    for (i = 0; i < xxh->ins; i++)
	pp_ins[i] = read16l(f);

    /* Skip channel table (?) */
    fseek (f, (sfh.pp_chn << 4) + 32, SEEK_SET);

    /* Read orders */
    for (i = 0; i < xxh->len; i++) {
	fread (&xxo[i], 1, 1, f);
	fseek (f, 4, SEEK_CUR);
    }
 
    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (1))
	report ("     Sample name    Len  LBeg LEnd L Vol C2Spd\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	fseek (f, pp_ins[i] << 4, SEEK_SET);

	sih.type = read8(f);
	fread(&sih.dosname, 13, 1, f);
	sih.memseg = read16l(f);
	sih.length = read32l(f);
	sih.loopbeg = read32l(f);
	sih.loopend = read32l(f);
	sih.vol = read8(f);
	sih.rsvd1 = read8(f);
	sih.pack = read8(f);
	sih.flags = read8(f);
	sih.c2spd = read16l(f);
	sih.rsvd2 = read16l(f);
	fread(&sih.rsvd3, 4, 1, f);
	sih.int_gp = read16l(f);
	sih.int_512 = read16l(f);
	sih.int_last = read32l(f);
	fread(&sih.name, 28, 1, f);
	fread(&sih.magic, 4, 1, f);

	xxih[i].nsm = !!(xxs[i].len = sih.length);
	xxs[i].lps = sih.loopbeg;
	xxs[i].lpe = sih.loopend;
	if (xxs[i].lpe == 0xffff)
	    xxs[i].lpe = 0;
	xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = sih.vol;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;

	copy_adjust(xxih[i].name, sih.name, 12);

	if (V (1) &&
	    (strlen ((char *) xxih[i].name) || (xxs[i].len > 1))) {
	    report ("[%2X] %-14.14s %04x %04x %04x %c V%02x %5d\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, sih.c2spd);
	}

	sih.c2spd = 8363 * sih.c2spd / 8448;
	c2spd_to_note (sih.c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
    }

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	if (!pp_pat[i])
	    continue;

	fseek (f, pp_pat[i] * 16, SEEK_SET);
	if (broken)
	    fseek (f, 2, SEEK_CUR);

	for (r = 0; r < 64; ) {
	    fread (&b, 1, 1, f);

	    if (b == S3M_EOR) {
		r++;
		continue;
	    }

	    c = b & S3M_CH_MASK;
	    event = c >= xxh->chn ? &dummy : &EVENT (i, c, r);

	    if (b & S3M_NI_FOLLOW) {
		fread (&n, 1, 1, f);

		switch (n) {
		case 255:
		    n = 0;
		    break;	/* Empty note */
		case 254:
		    n = 0x61;
		    break;	/* Key off */
		default:
		    n = 25 + 12 * MSN (n) + LSN (n);
		}

		event->note = n;
		fread (&n, 1, 1, f);
		event->ins = n;
	    }

	    if (b & S3M_VOL_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->vol = n + 1;
	    }

	    if (b & S3M_FX_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->fxt = fx[n];
		fread (&n, 1, 1, f);
		event->fxp = n;
		switch (event->fxt) {
		case FX_TEMPO:
		    event->fxp = MSN (event->fxp);
		    break;
		case FX_NONE:
		    event->fxp = event->fxt = 0;
		    break;
		}
	    }
	}

	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\n");

    /* Read samples */
    if (V (0))
	report ("Stored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->ins; i++) {
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    free (pp_pat);
    free (pp_ins);

    xmp_ctl->fetch |= XMP_CTL_VSALL | XMP_MODE_ST3;

    return 0;
}
