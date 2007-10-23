/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: fnk_load.c,v 1.12 2007-10-23 20:32:43 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* INCOMPLETE LOADER!!! NO EFFECTS! */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_Funk	MAGIC4('F','u','n','k')


static int fnk_test (FILE *, char *);
static int fnk_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info fnk_loader = {
    "FNK",
    "Funktracker",
    fnk_test,
    fnk_load
};

static int fnk_test(FILE *f, char *t)
{
    uint8 a, b;

    if (read32b(f) != MAGIC_Funk)
	return -1;

    fseek(f, 8, SEEK_CUR);
    a = read8(f);    
    b = read8(f);    

    if (a != 'F' || b != '2')
	return -1;

    read_title(f, t, 0);

    return 0;
}


struct fnk_instrument {
    uint8 name[19];		/* ASCIIZ instrument name */
    uint32 loop_start;		/* Instrument loop start */
    uint32 length;		/* Instrument length */
    uint8 volume;		/* Volume (0-255) */
    uint8 pan;			/* Pan (0-255) */
    uint8 shifter;		/* Portamento and offset shift */
    uint8 waveform;		/* Vibrato and tremolo waveforms */
    uint8 retrig;		/* Retrig and arpeggio speed */
};

struct fnk_header {
    uint8 marker[4];		/* 'Funk' */
    uint8 info[4];		/* */
    uint32 filesize;		/* File size */
    uint8 format[4];		/* F2xx, Fkxx or Fvxx */
    uint8 loop;			/* Loop order number */
    uint8 order[256];		/* Order list */
    uint8 pbrk[128];		/* Break list for patterns */
    struct fnk_instrument fih[64];	/* Instruments */
};


static int fnk_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &p->m;
    int i, j;
    struct xxm_event *event;
    struct fnk_header ffh;
    uint8 ev[3];

    LOAD_INIT();

    fread(&ffh.marker, 4, 1, f);
    fread(&ffh.info, 4, 1, f);
    ffh.filesize = read32l(f);
    fread(&ffh.format, 4, 1, f);
    ffh.loop = read8(f);
    fread(&ffh.order, 256, 1, f);
    fread(&ffh.pbrk, 128, 1, f);

    for (i = 0; i < 64; i++) {
	fread(&ffh.fih[i].name, 19, 1, f);
	ffh.fih[i].loop_start = read32l(f);
	ffh.fih[i].length = read32l(f);
	ffh.fih[i].volume = read8(f);
	ffh.fih[i].pan = read8(f);
	ffh.fih[i].shifter = read8(f);
	ffh.fih[i].waveform = read8(f);
	ffh.fih[i].retrig = read8(f);
    }

    m->xxh->chn = (ffh.format[2] < '0') || (ffh.format[2] > '9') ||
	(ffh.format[3] < '0') || (ffh.format[3] > '9') ? 8 :
	(ffh.format[2] - '0') * 10 + ffh.format[3] - '0';

    m->xxh->smp = m->xxh->ins = 64;

    for (i = 0; i < 256 && ffh.order[i] != 0xff; i++) {
	if (ffh.order[i] > m->xxh->pat)
	    m->xxh->pat = ffh.order[i];
    }
    m->xxh->pat++;

    m->xxh->len = i;
    m->xxh->trk = m->xxh->chn * m->xxh->pat;
    memcpy (m->xxo, ffh.order, m->xxh->len);

    m->xxh->tpo = 6;
    m->xxh->bpm = 125;
    /*
     * If an R1 format (funktype = Fk** or Fv**), then ignore byte 3. It's
     * unreliable. It used to store the (GUS) sample memory requirement.
     */
    if (ffh.format[0] == 'F' && ffh.format[1] == '2') {
	if (((int8)ffh.info[0] >> 1) & 0x40)
	    m->xxh->bpm -= (ffh.info[0] >> 1) & 0x3f;
	else
	    m->xxh->bpm += (ffh.info[0] >> 1) & 0x3f;
    }

    strcpy(m->type, "Funk (Funktracker)");

    MODULE_INFO();

    INSTRUMENT_INIT();

    /* Convert instruments */
    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc(sizeof (struct xxm_instrument), 1);
	m->xxih[i].nsm = !!(m->xxs[i].len = ffh.fih[i].length);
	m->xxs[i].lps = ffh.fih[i].loop_start;
	if (m->xxs[i].lps == -1)
	    m->xxs[i].lps = 0;
	m->xxs[i].lpe = ffh.fih[i].length;
	m->xxs[i].flg = ffh.fih[i].loop_start != -1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].vol = ffh.fih[i].volume;
	m->xxi[i][0].pan = ffh.fih[i].pan;
	m->xxi[i][0].sid = i;

	copy_adjust(m->xxih[i].name, ffh.fih[i].name, 19);

	if ((V(1)) && (strlen((char *)m->xxih[i].name) || m->xxs[i].len > 2))
	    report ("[%2X] %-20.20s %04x %04x %04x %c V%02x P%02x\n", i,
		m->xxih[i].name,
		m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
		m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
		m->xxi[i][0].vol, m->xxi[i][0].pan);
    }

    PATTERN_INIT();

    /* Read and convert patterns */
    reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	EVENT(i, 1, ffh.pbrk[i]).f2t = FX_BREAK;

	for (j = 0; j < 64 * m->xxh->chn; j++) {
	    event = &EVENT(i, j % m->xxh->chn, j / m->xxh->chn);
	    fread(&ev, 1, 3, f);

	    switch (ev[0] >> 2) {
	    case 0x3f:
	    case 0x3e:
	    case 0x3d:
		break;
	    default:
		event->note = 25 + (ev[0] >> 2);
		event->ins = 1 + MSN(ev[1]) + ((ev[0] & 0x03) << 4);
		event->vol = ffh.fih[event->ins - 1].volume;
	    }

	    switch (LSN(ev[1])) {
	    case 0x00:
		    event->fxt = FX_PER_PORTA_UP;
		    event->fxp = ev[2];
		    break;
	    case 0x01:
		    event->fxt = FX_PER_PORTA_DN;
		    event->fxp = ev[2];
		    break;
	    case 0x02:
		    event->fxt = FX_PER_TPORTA;
		    event->fxp = ev[2];
		    break;
	    case 0x03:
		    event->fxt = FX_PER_VIBRATO;
		    event->fxp = ev[2];
		    break;
	    case 0x06:
		    event->fxt = FX_PER_VSLD_UP;
		    event->fxp = ev[2] << 1;
		    break;
	    case 0x07:
		    event->fxt = FX_PER_VSLD_DN;
		    event->fxp = ev[2] << 1;
		    break;
	    case 0x0b:
		    event->fxt = FX_ARPEGGIO;
		    event->fxp = ev[2];
		    break;
	    case 0x0d:
		    event->fxt = FX_VOLSET;
		    event->fxp = ev[2];
		    break;
	    }
	}
	reportv(ctx, 0, ".");
    }

    /* Read samples */
    reportv(ctx, 0, "\nStored samples : %d ", m->xxh->smp);

    for (i = 0; i < m->xxh->ins; i++) {
	if (m->xxs[i].len <= 2)
	    continue;

	xmp_drv_loadpatch(ctx, f, m->xxi[i][0].sid, m->c4rate, 0,
							&m->xxs[i], NULL);

	reportv(ctx, 0, ".");
    }

    reportv(ctx, 0, "\n");

    for (i = 0; i < m->xxh->chn; i++)
	m->xxc[i].pan = 0x80;

    m->volbase = 0xff;
    m->fetch = XMP_CTL_VSALL;

    return 0;
}
