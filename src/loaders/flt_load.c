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
static int flt_load (struct xmp_mod_context *, FILE *);

struct xmp_loader_info flt_loader = {
    "MOD",
    "Startrekker/Audio Sculpture",
    flt_test,
    flt_load
};

static int flt_test(FILE *f, char *t)
{
    int buf[4];

    fseek(f, 1080, SEEK_SET);
    fread(buf, 4, 1, f);

    if (memcmp(buf, "FLT", 3) && memcmp(buf, "EXO", 3))
	return -1;

    fseek(f, 0, SEEK_SET);
    read_title(f, t, 28);

    return 0;
}


static int flt_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct mod_header mh;
    uint8 mod_event[4];
    char *tracker;

    LOAD_INIT();

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
    if (mh.magic[0] == 'F' && mh.magic[1] == 'L' && mh.magic[2] == 'T')
	tracker = "Startrekker";
    else if (mh.magic[0] == 'E' && mh.magic[1] == 'X' && mh.magic[2] == 'O')
	tracker = "Startrekker/Audio Sculpture";
    else
	return -1;

    if (mh.magic[3] == '4')
	m->xxh->chn = 4;
    else if (mh.magic[3] == '8') 
	m->xxh->chn = 8;
    else return -1;

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

    reportv(1, "     Instrument name        Len  LBeg LEnd L Vol Fin\n");

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

	copy_adjust(m->xxih[i].name, mh.ins[i].name, 22);

	if ((V (1)) && (strlen((char *)m->xxih[i].name) || m->xxs[i].len > 2)) {
	    report("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d\n",
			i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
			m->xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
			m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT();

    /* Load and convert patterns */
    reportv(0, "Stored patterns: %d ", m->xxh->pat);

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
	reportv(0, ".");
    }

    m->xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    reportv(0, "\nStored samples : %d ", m->xxh->smp);
    for (i = 0; i < m->xxh->smp; i++) {
	if (!m->xxs[i].len)
	    continue;
	xmp_drv_loadpatch(f, m->xxi[i][0].sid, xmp_ctl->c4rate, 0,
					&m->xxs[m->xxi[i][0].sid], NULL);
	reportv(0, ".");
    }
    reportv(0, "\n");

    return 0;
}
