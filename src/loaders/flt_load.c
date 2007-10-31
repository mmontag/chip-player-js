/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "mod.h"
#include "period.h"

static int flt_test (FILE *, char *);
static int flt_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info flt_loader = {
    "MOD",
    "Startrekker/Audio Sculpture",
    flt_test,
    flt_load
};

static int flt_test(FILE *f, char *t)
{
    char buf[4];

    fseek(f, 1080, SEEK_SET);
    fread(buf, 4, 1, f);

    if (memcmp(buf, "FLT", 3) && memcmp(buf, "EXO", 3))
	return -1;

    if (buf[3] != '4' && buf[3] != '8' && buf[3] != 'M')
	return -1;

    fseek(f, 0, SEEK_SET);
    read_title(f, t, 28);

    return 0;
}



/* Waveforms from the Startrekker 1.2 AM synth replayer code */

int8 am_waveform[4][32] = {
	{    0,   25,   49,   71,   90,  106,  117,  125,	/* Sine */
	   127,  125,  117,  106,   90,   71,   49,   25,
	     0,  -25,  -49,  -71,  -90, -106, -117, -125,
	  -127, -125, -117, -106,  -90,  -71,  -49,  -25
	},

	{ -128, -120, -112, -104,  -96,  -88,  -80,  -72,	/* Ramp */
	   -64,  -56,  -48,  -40,  -32,  -24,  -16,   -8,
	     0,    8,   16,   24,   32,   40,   48,   56,
	    64,   72,   80,   88,   96,  104,  112,  120
	},

	{ -128, -128, -128, -128, -128, -128, -128, -128,	/* Square */
	  -128, -128, -128, -128, -128, -128, -128, -128,
	   127,  127,  127,  127,  127,  127,  127,  127,
	   127,  127,  127,  127,  127,  127,  127,  127
	},

	{							/* Random */
	}
};

/*
 * AM synth parameters based on the Startrekker 1.2 documentation
 *
 * FQ    This waves base frequency. $1 is very low, $4 is average and $20 is
 *       quite high.
 * L0    Start amplitude for the envelope
 * A1L   Attack level
 * A1S   The speed that the amplitude changes to the attack level, $1 is slow
 *       and $40 is fast.
 * A2L   Secondary attack level, for those who likes envelopes...
 * A2S   Secondary attack speed.
 * DS    The speed that the amplitude decays down to the:
 * SL    Sustain level. There is remains for the time set by the
 * ST    Sustain time.
 * RS    Release speed. The speed that the amplitude falls from ST to 0.
 */

struct am_instrument {
	int16 l0;		/* start amplitude */
	int16 a1l;		/* attack level */
	int16 a1s;		/* attack speed */
	int16 a2l;		/* secondary attack level */
	int16 a2s;		/* secondary attack speed */
	int16 sl;		/* sustain level */
	int16 ds;		/* decay speed */
	int16 st;		/* sustain time */
	int16 rs;		/* release speed */
	int16 wf;		/* waveform */
	int16 p_fall;		/* ? */
	int16 v_amp;		/* vibrato amplitude */
	int16 v_spd;		/* vibrato speed */
	int16 fq;		/* base frequency */
};



static int is_am_instrument(FILE *nt, int i)
{
    char buf[2];

    fseek(nt, 144 + i * 120, SEEK_SET);
    fread(buf, 1, 2, nt);

    return !memcmp(buf, "AM", 2);
}

static void read_am_instrument(struct xmp_context *ctx, FILE *nt, int i)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &p->m;
    struct am_instrument am;

    fseek(nt, 144 + i * 120 + 2 + 4, SEEK_SET);
    am.l0 = read16b(nt);
    am.a1l = read16b(nt);
    am.a1s = read16b(nt);
    am.a2l = read16b(nt);
    am.a2s = read16b(nt);
    am.sl = read16b(nt);
    am.ds = read16b(nt);
    am.st = read16b(nt);
    read16b(nt);
    am.rs = read16b(nt);
    am.wf = read16b(nt);
    am.p_fall = -(int16)read16b(nt);
    am.v_amp = read16b(nt);
    am.v_spd = read16b(nt);
    am.fq = read16b(nt);

    m->xxs[i].len = 32;
    m->xxs[i].lps = 0;
    m->xxs[i].lpe = 32;
    m->xxs[i].flg = WAVE_LOOPING;
    //m->xxi[i][0].vol = mh.ins[i].volume;
    m->xxih[i].nsm = 1;

    xmp_drv_loadpatch(ctx, NULL, m->xxi[i][0].sid, m->c4rate, XMP_SMP_NOLOAD,
			&m->xxs[m->xxi[i][0].sid], (char *)am_waveform[am.wf]);

    reportv(ctx, 0, "A");
}


static int flt_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &p->m;
    struct xmp_options *o = &ctx->o;
    int i, j;
    struct xxm_event *event;
    struct mod_header mh;
    uint8 mod_event[4];
    char *tracker;
    char filename[1024];
    char buf[16];
    FILE *nt;
    int am_synth;

    LOAD_INIT();

    /* See if we have the synth parameters file */
    am_synth = 0;
    snprintf(filename, 1024, "%s/%s.NT", m->dirname, m->basename);
    if ((nt = fopen(filename, "rb")) == NULL) {
	snprintf(filename, 1024, "%s/%s.nt", m->dirname, m->basename);
	nt = fopen(filename, "rb");
    }

    if (nt) {
	fread(buf, 1, 16, nt);
	if (memcmp(buf, "ST1.2 ModuleINFO", 16) == 0)
	    am_synth = 1;
	if (memcmp(buf, "ST1.3 ModuleINFO", 16) == 0)
	    am_synth = 1;
    }

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

    /* Also RASP? */
    if (mh.magic[0] == 'F' && mh.magic[1] == 'L' && mh.magic[2] == 'T') {
	tracker = "Startrekker";
    } else {
	tracker = "Startrekker/Audio Sculpture";
    }

    if (am_synth) {
	if (buf[4] == '2')
	    tracker = "Startrekker 1.2";
	else if (buf[4] == '3')
	    tracker = "Startrekker 1.3";
    }

    if (mh.magic[3] == '4')
	m->xxh->chn = 4;
    else
	m->xxh->chn = 8;

    m->xxh->len = mh.len;
    m->xxh->rst = mh.restart;
    memcpy(m->xxo, mh.order, 128);

    for (i = 0; i < 128; i++) {
	if (m->xxh->chn > 4)
	    m->xxo[i] >>= 1;
	if (m->xxo[i] > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];
    }

    m->xxh->pat++;

    m->xxh->trk = m->xxh->chn * m->xxh->pat;

    strncpy(m->name, (char *) mh.name, 20);
    sprintf(m->type, "%4.4s (%s)", mh.magic, tracker);
    MODULE_INFO();

    INSTRUMENT_INIT();

    reportv(ctx, 1, "     Instrument name        Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxs[i].len = 2 * mh.ins[i].size;
	m->xxs[i].lps = 2 * mh.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * mh.ins[i].loop_size;
	m->xxs[i].flg = mh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].fin = (int8)(mh.ins[i].finetune << 4);
	m->xxi[i][0].vol = mh.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;

	if (m->xxs[i].flg & WAVE_LOOPING && m->xxs[i].len > m->xxs[i].lpe)
            m->xxs[i].flg |= WAVE_PTKLOOP;

	copy_adjust(m->xxih[i].name, mh.ins[i].name, 22);

	if (V(1) && (strlen((char *)m->xxih[i].name) || m->xxs[i].len > 2
						|| is_am_instrument(nt, i))) {
	    report("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d %c\n",
			i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
			m->xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
			m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4,
			m->xxs[i].flg & WAVE_PTKLOOP ? '!' : ' ');
	}
    }

    PATTERN_INIT();

    /* Load and convert patterns */
    reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

    /* The format you are looking for is FLT8, and the ONLY two differences
     * are: It says FLT8 instead of FLT4 or M.K., AND, the patterns are PAIRED.
     * I thought this was the easiest 8 track format possible, since it can be
     * loaded in a normal 4 channel tracker if you should want to rip sounds or
     * patterns. So, in a 8 track FLT8 module, patterns 00 and 01 is "really"
     * pattern 00. Patterns 02 and 03 together is "really" pattern 01. Thats
     * it. Oh well, I didnt have the time to implement all effect commands
     * either, so some FLT8 modules would play back badly (I think especially
     * the portamento command uses a different "scale" than the normal
     * portamento command, that would be hard to patch).
     */
    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC(i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC(i);
	for (j = 0; j < (64 * 4); j++) {
	    event = &EVENT(i, j % 4, j / 4);
	    fread(mod_event, 1, 4, f);
	    cvt_pt_event(event, mod_event);
	}
	if (m->xxh->chn > 4) {
	    for (j = 0; j < (64 * 4); j++) {
		event = &EVENT(i, (j % 4) + 4, j / 4);
		fread(mod_event, 1, 4, f);
		cvt_pt_event(event, mod_event);
	    }
	}
	reportv(ctx, 0, ".");
    }

    m->xxh->flg |= XXM_FLG_MODRNG;

    if (o->skipsmp)
	return 0;

    /* Load samples */

    reportv(ctx, 0, "\nStored samples : %d ", m->xxh->smp);
    for (i = 0; i < m->xxh->smp; i++) {
	if (m->xxs[i].len == 0) {
	    if (am_synth && is_am_instrument(nt, i)) {
		read_am_instrument(ctx, nt, i);
	    }
	    continue;
	}
	xmp_drv_loadpatch(ctx, f, m->xxi[i][0].sid, m->c4rate, 0,
					&m->xxs[m->xxi[i][0].sid], NULL);
	reportv(ctx, 0, ".");
    }
    reportv(ctx, 0, "\n");

    return 0;
}
