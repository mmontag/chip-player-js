/* Protracker 3 IFFMODL module loader for xmp
 * Copyright (C) 2000-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: pt3_load.c,v 1.7 2007-10-13 23:21:26 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "load.h"
#include "mod.h"
#include "iff.h"

#define MAGIC_FORM	MAGIC4('F','O','R','M')
#define MAGIC_MODL	MAGIC4('M','O','D','L')


static int pt3_test (FILE *, char *);
static int pt3_load (FILE *);

struct xmp_loader_info pt3_loader = {
    "PTM",
    "Protracker 3",
    pt3_test,
    pt3_load
};

static int pt3_test(FILE *f, char *t)
{
    uint32 form, id;

    form = read32b(f);
    read32b(f);
    id = read32b(f);
    
    if (form != MAGIC_FORM || id != MAGIC_MODL)
	return -1;

    read_title(f, t, 0);

    return 0;
}


#define PT3_FLAG_CIA	0x0001	/* VBlank if not set */
#define PT3_FLAG_FILTER	0x0002	/* Filter status */
#define PT3_FLAG_SONG	0x0004	/* Modules have this bit unset */
#define PT3_FLAG_IRQ	0x0008	/* Soft IRQ */
#define PT3_FLAG_VARPAT	0x0010	/* Variable pattern length */
#define PT3_FLAG_8VOICE	0x0020	/* 4 voices if not set */
#define PT3_FLAG_16BIT	0x0040	/* 8 bit samples if not set */
#define PT3_FLAG_RAWPAT	0x0080	/* Packed patterns if not set */


struct pt3_chunk_info {
    char name[32];
    uint16 nins;
    uint16 npos;
    uint16 npat;
    uint16 gvol;
    uint16 dbpm;
    uint16 flgs;
    uint16 cday;
    uint16 cmon;
    uint16 cyea;
    uint16 chrs;
    uint16 cmin;
    uint16 csec;
    uint16 dhrs;
    uint16 dmin;
    uint16 dsec;
    uint16 dmsc;
};

struct pt3_chunk_inst {
    char name[32];
    uint16 ilen;
    uint16 ilps;
    uint16 ilpl;
    uint16 ivol;
    int16 fine;
};


static int ptdt_load(FILE *);


static void get_vers(int size, FILE *f)
{
    char buf[20];

    fread(buf, 1, 10, f);
    sprintf (xmp_ctl->type, "%-6.6s (Protracker IFFMODL)", buf + 4);

    /* Workaround for PT3.61 bug (?) */
    fseek (f, 10 - size, SEEK_CUR);
}


static void get_info(int size, FILE *f)
{
    struct pt3_chunk_info i;

    fread(xmp_ctl->name, 1, 32, f);
    i.nins = read16b(f);
    i.npos = read16b(f);
    i.npat = read16b(f);
    i.gvol = read16b(f);
    i.dbpm = read16b(f);
    i.flgs = read16b(f);
    i.cday = read16b(f);
    i.cmon = read16b(f);
    i.cyea = read16b(f);
    i.chrs = read16b(f);
    i.cmin = read16b(f);
    i.csec = read16b(f);
    i.dhrs = read16b(f);
    i.dmin = read16b(f);
    i.dsec = read16b(f);
    i.dmsc = read16b(f);

    MODULE_INFO ();

    if (V(0)) {
	report("Creation date  : %02d/%02d/%02d %02d:%02d:%02d\n",
	    i.cmon, i.cday, i.cyea, i.chrs, i.cmin, i.csec);
	report("Playing time   : %02d:%02d:%02d\n", i.dhrs, i.dmin, i.dsec);
    }
}


static void get_cmnt(int size, FILE *f)
{
    reportv(0, "Comment size   : %d\n", size);
}


static void get_ptdt(int size, FILE *f)
{
    ptdt_load(f);
}


static int pt3_load(FILE *f)
{
    LOAD_INIT();

    read32b(f);		/* form */
    read32b(f);		/* size */
    read32b(f);		/* id */
    
    /* IFF chunk IDs */
    iff_register ("VERS", get_vers);
    iff_register ("INFO", get_info);
    iff_register ("CMNT", get_cmnt);
    iff_register ("PTDT", get_ptdt);

    iff_setflag(IFF_FULL_CHUNK_SIZE);

    /* Load IFF chunks */
    while (!feof(f))
	iff_chunk(f);

    iff_release();

    return 0;
}


static int ptdt_load(FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct mod_header mh;
    uint8 mod_event[4];

    fread(&mh.name, 20, 1, f);
    for (i = 0; i < 31; i++) {
	fread(&mh.ins[i].name, 22, 1, f);	/* Instrument name */
	mh.ins[i].size = read16b(f);		/* Length in 16-bit words */
	mh.ins[i].finetune = read8(f);		/* Finetune (signed nibble) */
	mh.ins[i].volume = read8(f);		/* Linear playback volume */
	mh.ins[i].loop_start = read16b(f);	/* Loop start in 16-bit words */
	mh.ins[i].loop_size = read16b(f);	/* Loop size in 16-bit words */
    }
    mh.len = read8(f);
    mh.restart = read8(f);
    fread(&mh.order, 128, 1, f);
    fread(&mh.magic, 4, 1, f);

    xxh->chn = 4;
    xxh->len = mh.len;
    xxh->rst = mh.restart;
    memcpy (xxo, mh.order, 128);

    for (i = 0; i < 128; i++) {
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    }

    xxh->pat++;
    xxh->trk = xxh->chn * xxh->pat;

    INSTRUMENT_INIT();

    reportv(1, "     Instrument name        Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc(sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * mh.ins[i].size;
	xxs[i].lps = 2 * mh.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * mh.ins[i].loop_size;
	xxs[i].flg = mh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8)(mh.ins[i].finetune << 4);
	xxi[i][0].vol = mh.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;

	copy_adjust(xxih[i].name, mh.ins[i].name, 22);

	if ((V(1)) && (strlen((char *)xxih[i].name) || xxs[i].len > 2)) {
	    report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d\n",
			i, xxih[i].name, xxs[i].len, xxs[i].lps,
			xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
			xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT();

    /* Load and convert patterns */
    reportv(0, "Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC(i);
	xxp[i]->rows = 64;
	TRACK_ALLOC(i);
	for (j = 0; j < (64 * 4); j++) {
	    event = &EVENT(i, j % 4, j / 4);
	    fread(mod_event, 1, 4, f);
	    cvt_pt_event(event, mod_event);
	}
	reportv(0, ".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */
    reportv(0, "\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->smp; i++) {
	if (!xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
					    &xxs[xxi[i][0].sid], NULL);
	reportv(0, ".");
    }
    reportv(0, "\n");

    return 0;
}
