/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include "load.h"

#define MAGIC_Funk	MAGIC4('F','u','n','k')


static int fnk_test (FILE *, char *, const int);
static int fnk_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info fnk_loader = {
    "FNK",
    "Funktracker",
    fnk_test,
    fnk_load
};

static int fnk_test(FILE *f, char *t, const int start)
{
    uint8 a, b;
    int size;
    struct stat st;

    if (read32b(f) != MAGIC_Funk)
	return -1;

    read8(f); 
    a = read8(f);
    b = read8(f); 
    read8(f); 

    if ((a >> 1) < 10)			/* creation year (-1980) */
	return -1;

    if (MSN(b) > 7 || LSN(b) > 9)	/* CPU and card */
	return -1;

    size = read32l(f);
    if (size < 1024)
	return -1;

    fstat(fileno(f), &st);
    if (size != st.st_size)
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
    uint8 fmt[4];		/* F2xx, Fkxx or Fvxx */
    uint8 loop;			/* Loop order number */
    uint8 order[256];		/* Order list */
    uint8 pbrk[128];		/* Break list for patterns */
    struct fnk_instrument fih[64];	/* Instruments */
};


static int fnk_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;
    int i, j;
    int day, month, year;
    struct xmp_event *event;
    struct fnk_header ffh;
    uint8 ev[3];

    LOAD_INIT();

    fread(&ffh.marker, 4, 1, f);
    fread(&ffh.info, 4, 1, f);
    ffh.filesize = read32l(f);
    fread(&ffh.fmt, 4, 1, f);
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

    day = ffh.info[0] & 0x1f;
    month = ((ffh.info[1] & 0x01) << 3) | ((ffh.info[0] & 0xe0) >> 5);
    year = 1980 + ((ffh.info[1] & 0xfe) >> 1);

    m->mod.smp = m->mod.ins = 64;

    for (i = 0; i < 256 && ffh.order[i] != 0xff; i++) {
	if (ffh.order[i] > m->mod.pat)
	    m->mod.pat = ffh.order[i];
    }
    m->mod.pat++;

    m->mod.len = i;
    memcpy (m->mod.xxo, ffh.order, m->mod.len);

    m->mod.tpo = 4;
    m->mod.bpm = 125;
    m->mod.chn = 0;

    /*
     * If an R1 fmt (funktype = Fk** or Fv**), then ignore byte 3. It's
     * unreliable. It used to store the (GUS) sample memory requirement.
     */
    if (ffh.fmt[0] == 'F' && ffh.fmt[1] == '2') {
	if (((int8)ffh.info[3] >> 1) & 0x40)
	    m->mod.bpm -= (ffh.info[3] >> 1) & 0x3f;
	else
	    m->mod.bpm += (ffh.info[3] >> 1) & 0x3f;

	strcpy(m->mod.type, "FNK R2 (FunktrackerGOLD)");
    } else if (ffh.fmt[0] == 'F' && (ffh.fmt[1] == 'v' || ffh.fmt[1] == 'k')) {
	strcpy(m->mod.type, "FNK R1 (Funktracker)");
    } else {
	m->mod.chn = 8;
	strcpy(m->mod.type, "FNK R0 (Funktracker DOS32)");
    }

    if (m->mod.chn == 0) {
	m->mod.chn = (ffh.fmt[2] < '0') || (ffh.fmt[2] > '9') ||
		(ffh.fmt[3] < '0') || (ffh.fmt[3] > '9') ? 8 :
		(ffh.fmt[2] - '0') * 10 + ffh.fmt[3] - '0';
    }

    m->mod.bpm = 4 * m->mod.bpm / 5;
    m->mod.trk = m->mod.chn * m->mod.pat;
    /* FNK allows mode per instrument but we don't, so use linear like 669 */
    m->mod.flg |= XXM_FLG_LINEAR;

    MODULE_INFO();
    _D(_D_INFO "Creation date: %02d/%02d/%04d", day, month, year);

    INSTRUMENT_INIT();

    /* Convert instruments */
    for (i = 0; i < m->mod.ins; i++) {
	m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len = ffh.fih[i].length);
	m->mod.xxs[i].lps = ffh.fih[i].loop_start;
	if (m->mod.xxs[i].lps == -1)
	    m->mod.xxs[i].lps = 0;
	m->mod.xxs[i].lpe = ffh.fih[i].length;
	m->mod.xxs[i].flg = ffh.fih[i].loop_start != -1 ? XMP_SAMPLE_LOOP : 0;
	m->mod.xxi[i].sub[0].vol = ffh.fih[i].volume;
	m->mod.xxi[i].sub[0].pan = ffh.fih[i].pan;
	m->mod.xxi[i].sub[0].sid = i;

	copy_adjust(m->mod.xxi[i].name, ffh.fih[i].name, 19);

	_D(_D_INFO "[%2X] %-20.20s %04x %04x %04x %c V%02x P%02x", i,
		m->mod.xxi[i].name,
		m->mod.xxs[i].len, m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
		m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		m->mod.xxi[i].sub[0].vol, m->mod.xxi[i].sub[0].pan);
    }

    PATTERN_INIT();

    /* Read and convert patterns */
    _D(_D_INFO "Stored patterns: %d", m->mod.pat);

    for (i = 0; i < m->mod.pat; i++) {
	PATTERN_ALLOC (i);
	m->mod.xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	EVENT(i, 1, ffh.pbrk[i]).f2t = FX_BREAK;

	for (j = 0; j < 64 * m->mod.chn; j++) {
	    event = &EVENT(i, j % m->mod.chn, j / m->mod.chn);
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
	    case 0x0e:
		if (ev[2] == 0x0a || ev[2] == 0x0b || ev[2] == 0x0c) {
		    event->fxt = FX_PER_CANCEL;
		    break;
		}

		switch (MSN(ev[2])) {
		case 0x1:
		    event->fxt = FX_EXTENDED;
		    event->fxp = (EX_CUT << 4) | LSN(ev[2]);
		    break;
		case 0x2:
		    event->fxt = FX_EXTENDED;
		    event->fxp = (EX_DELAY << 4) | LSN(ev[2]);
		    break;
		case 0xd:
		    event->fxt = FX_EXTENDED;
		    event->fxp = (EX_RETRIG << 4) | LSN(ev[2]);
		    break;
		case 0xe:
		    event->fxt = FX_SETPAN;
		    event->fxp = 8 + (LSN(ev[2]) << 4);	
		    break;
		case 0xf:
		    event->fxt = FX_TEMPO;
		    event->fxp = LSN(ev[2]);	
		    break;
		}
	    }
	}
    }

    /* Read samples */
    _D(_D_INFO "Stored samples: %d", m->mod.smp);

    for (i = 0; i < m->mod.ins; i++) {
	if (m->mod.xxs[i].len <= 2)
	    continue;

	load_patch(ctx, f, m->mod.xxi[i].sub[0].sid, 0,
							&m->mod.xxs[i], NULL);

    }

    for (i = 0; i < m->mod.chn; i++)
	m->mod.xxc[i].pan = 0x80;

    m->volbase = 0xff;
    m->quirk = QUIRK_VSALL;

    return 0;
}
