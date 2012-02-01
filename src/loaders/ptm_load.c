/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "ptm.h"
#include "period.h"

#define MAGIC_PTMF	MAGIC4('P','T','M','F')


static int ptm_test (FILE *, char *, const int);
static int ptm_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info ptm_loader = {
    "PTM",
    "Poly Tracker",
    ptm_test,
    ptm_load
};

static int ptm_test(FILE *f, char *t, const int start)
{
    fseek(f, start + 44, SEEK_SET);
    if (read32b(f) != MAGIC_PTMF)
	return -1;

    fseek(f, start + 0, SEEK_SET);
    read_title(f, t, 28);

    return 0;
}


static int ptm_vol[] = {
     0,  5,  8, 10, 12, 14, 15, 17, 18, 20, 21, 22, 23, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40,
    41, 42, 42, 43, 44, 45, 46, 46, 47, 48, 49, 49, 50, 51, 51,
    52, 53, 54, 54, 55, 56, 56, 57, 58, 58, 59, 59, 60, 61, 61,
    62, 63, 63, 64, 64
};


static int ptm_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;
    int c, r, i, smp_ofs[256];
    struct xmp_event *event;
    struct ptm_file_header pfh;
    struct ptm_instrument_header pih;
    uint8 n, b;

    LOAD_INIT();

    /* Load and convert header */

    fread(&pfh.name, 28, 1, f);		/* Song name */
    pfh.doseof = read8(f);		/* 0x1a */
    pfh.vermin = read8(f);		/* Minor version */
    pfh.vermaj = read8(f);		/* Major type */
    pfh.rsvd1 = read8(f);		/* Reserved */
    pfh.ordnum = read16l(f);		/* Number of orders (must be even) */
    pfh.insnum = read16l(f);		/* Number of instruments */
    pfh.patnum = read16l(f);		/* Number of patterns */
    pfh.chnnum = read16l(f);		/* Number of channels */
    pfh.flags = read16l(f);		/* Flags (set to 0) */
    pfh.rsvd2 = read16l(f);		/* Reserved */
    pfh.magic = read32b(f); 		/* 'PTMF' */

#if 0
    if (pfh.magic != MAGIC_PTMF)
	return -1;
#endif

    fread(&pfh.rsvd3, 16, 1, f);	/* Reserved */
    fread(&pfh.chset, 32, 1, f);	/* Channel settings */
    fread(&pfh.order, 256, 1, f);	/* Orders */
    for (i = 0; i < 128; i++)
	pfh.patseg[i] = read16l(f);

    m->mod.len = pfh.ordnum;
    m->mod.ins = pfh.insnum;
    m->mod.pat = pfh.patnum;
    m->mod.chn = pfh.chnnum;
    m->mod.trk = m->mod.pat * m->mod.chn;
    m->mod.smp = m->mod.ins;
    m->mod.tpo = 6;
    m->mod.bpm = 125;
    memcpy (m->mod.xxo, pfh.order, 256);

    m->c4rate = C4_NTSC_RATE;

    copy_adjust(m->mod.name, pfh.name, 28);
    set_type(m, "PTMF %d.%02x (Poly Tracker)",
	pfh.vermaj, pfh.vermin);

    MODULE_INFO();

    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    for (i = 0; i < m->mod.ins; i++) {
	m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	pih.type = read8(f);			/* Sample type */
	fread(&pih.dosname, 12, 1, f);		/* DOS file name */
	pih.vol = read8(f);			/* Volume */
	pih.c4spd = read16l(f);			/* C4 speed */
	pih.smpseg = read16l(f);		/* Sample segment (not used) */
	pih.smpofs = read32l(f);		/* Sample offset */
	pih.length = read32l(f);		/* Length */
	pih.loopbeg = read32l(f);		/* Loop begin */
	pih.loopend = read32l(f);		/* Loop end */
	pih.gusbeg = read32l(f);		/* GUS begin address */
	pih.guslps = read32l(f);		/* GUS loop start address */
	pih.guslpe = read32l(f);		/* GUS loop end address */
	pih.gusflg = read8(f);			/* GUS loop flags */
	pih.rsvd1 = read8(f);			/* Reserved */
	fread(&pih.name, 28, 1, f);		/* Instrument name */
	pih.magic = read32b(f);			/* 'PTMS' */

	if ((pih.type & 3) != 1)
	    continue;

	smp_ofs[i] = pih.smpofs;
	m->mod.xxs[i].len = pih.length;
	m->mod.xxi[i].nsm = pih.length > 0 ? 1 : 0;
	m->mod.xxs[i].lps = pih.loopbeg;
	m->mod.xxs[i].lpe = pih.loopend;
	if (m->mod.xxs[i].lpe)
		m->mod.xxs[i].lpe--;
	m->mod.xxs[i].flg = pih.type & 0x04 ? XMP_SAMPLE_LOOP : 0;
	m->mod.xxs[i].flg |= pih.type & 0x08 ? XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR : 0;

	if (pih.type & 0x10) {
	    m->mod.xxs[i].flg |= XMP_SAMPLE_16BIT;
	    m->mod.xxs[i].len >>= 1;
	    m->mod.xxs[i].lps >>= 1;
	    m->mod.xxs[i].lpe >>= 1;
	}

	m->mod.xxi[i].sub[0].vol = pih.vol;
	m->mod.xxi[i].sub[0].pan = 0x80;
	m->mod.xxi[i].sub[0].sid = i;
	pih.magic = 0;

	copy_adjust(m->mod.xxi[i].name, pih.name, 28);

	_D(_D_INFO "[%2X] %-28.28s %05x%c%05x %05x %c V%02x %5d",
		i, m->mod.xxi[i].name, m->mod.xxs[i].len,
		pih.type & 0x10 ? '+' : ' ',
		m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
		m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		m->mod.xxi[i].sub[0].vol, pih.c4spd);

	/* Convert C4SPD to relnote/finetune */
	c2spd_to_note (pih.c4spd, &m->mod.xxi[i].sub[0].xpo, &m->mod.xxi[i].sub[0].fin);
    }

    PATTERN_INIT();

    /* Read patterns */
    _D(_D_INFO "Stored patterns: %d", m->mod.pat);

    for (i = 0; i < m->mod.pat; i++) {
	if (!pfh.patseg[i])
	    continue;
	PATTERN_ALLOC (i);
	m->mod.xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	fseek(f, start + 16L * pfh.patseg[i], SEEK_SET);
	r = 0;
	while (r < 64) {
	    b = read8(f);
	    if (!b) {
		r++;
		continue;
	    }

	    c = b & PTM_CH_MASK;
	    if (c >= m->mod.chn)
		continue;

	    event = &EVENT (i, c, r);
	    if (b & PTM_NI_FOLLOW) {
		n = read8(f);
		switch (n) {
		case 255:
		    n = 0;
		    break;	/* Empty note */
		case 254:
		    n = XMP_KEY_OFF;
		    break;	/* Key off */
		}
		event->note = n;
		event->ins = read8(f);
	    }
	    if (b & PTM_FX_FOLLOWS) {
		event->fxt = read8(f);
		event->fxp = read8(f);

		if (event->fxt > 0x17)
			event->fxt = event->fxp = 0;

		switch (event->fxt) {
		case 0x0e:	/* Extended effect */
		    if (MSN(event->fxp) == 0x8) {	/* Pan set */
			event->fxt = FX_SETPAN;
			event->fxp = LSN (event->fxp) << 4;
		    }
		    break;
		case 0x10:	/* Set global volume */
		    event->fxt = FX_GLOBALVOL;
		    break;
		case 0x11:	/* Multi retrig */
		    event->fxt = FX_MULTI_RETRIG;
		    break;
		case 0x12:	/* Fine vibrato */
		    event->fxt = FX_FINE4_VIBRA;
		    break;
		case 0x13:	/* Note slide down */
		    event->fxt = FX_NSLIDE_DN;
		    break;
		case 0x14:	/* Note slide up */
		    event->fxt = FX_NSLIDE_UP;
		    break;
		case 0x15:	/* Note slide down + retrig */
		    event->fxt = FX_NSLIDE_R_DN;
		    break;
		case 0x16:	/* Note slide up + retrig */
		    event->fxt = FX_NSLIDE_R_UP;
		    break;
		case 0x17:	/* Reverse sample */
		    event->fxt = event->fxp = 0;
		    break;
		}
	    }
	    if (b & PTM_VOL_FOLLOWS) {
		event->vol = read8(f) + 1;
	    }
	}
    }

    _D(_D_INFO "Stored samples: %d", m->mod.smp);

    for (i = 0; i < m->mod.smp; i++) {
	if (!m->mod.xxs[i].len)
	    continue;
	fseek(f, start + smp_ofs[m->mod.xxi[i].sub[0].sid], SEEK_SET);
	load_patch(ctx, f, m->mod.xxi[i].sub[0].sid,
			XMP_SMP_8BDIFF, &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
    }

    m->vol_table = ptm_vol;

    for (i = 0; i < m->mod.chn; i++)
	m->mod.xxc[i].pan = pfh.chset[i] << 4;

    return 0;
}
