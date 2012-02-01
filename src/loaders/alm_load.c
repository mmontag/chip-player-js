/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* ALM (Aley's Module) is a module format used on 8bit computers. It was
 * designed to be usable on Sam Coupe (CPU Z80 6MHz) and PC XT. The ALM file
 * format is very simple and it have no special effects, so every computer
 * can play the ALMs.
 *
 * Note: xmp's module loading mechanism was not designed to load samples
 * from different files. Using *module into a global variable is a hack.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


static int alm_test (FILE *, char *, const int);
static int alm_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info alm_loader = {
    "ALM",
    "Aley Keptr",
    alm_test,
    alm_load
};

static int alm_test(FILE *f, char *t, const int start)
{
    char buf[7];

    if (fread(buf, 1, 7, f) < 7)
	return -1;

    if (memcmp(buf, "ALEYMOD", 7) && memcmp(buf, "ALEY MO", 7))
	return -1;

    read_title(f, t, 0);

    return 0;
}



struct alm_file_header {
    uint8 id[7];		/* "ALEY MO" or "ALEYMOD" */
    uint8 speed;		/* Only in versions 1.1 and 1.2 */
    uint8 length;		/* Length of module */
    uint8 restart;		/* Restart position */
    uint8 order[128];		/* Pattern sequence */
};

#define NAME_SIZE 255

static int alm_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;
    int i, j;
    struct alm_file_header afh;
    struct xmp_event *event;
    struct stat stat;
    uint8 b;
    char *basename;
    char filename[NAME_SIZE];
    char modulename[NAME_SIZE];
    FILE *s;

    LOAD_INIT();

    fread(&afh.id, 7, 1, f);

    if (!strncmp((char *)afh.id, "ALEYMOD", 7))		/* Version 1.0 */
	m->mod.xxh->tpo = afh.speed / 2;

    strncpy(modulename, m->filename, NAME_SIZE);
    basename = strtok (modulename, ".");

    afh.speed = read8(f);
    afh.length = read8(f);
    afh.restart = read8(f);
    fread(&afh.order, 128, 1, f);

    m->mod.xxh->len = afh.length;
    m->mod.xxh->rst = afh.restart;
    memcpy (m->mod.xxo, afh.order, m->mod.xxh->len);

    for (m->mod.xxh->pat = i = 0; i < m->mod.xxh->len; i++)
	if (m->mod.xxh->pat < afh.order[i])
	    m->mod.xxh->pat = afh.order[i];
    m->mod.xxh->pat++;

    m->mod.xxh->ins = 31;
    m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;
    m->mod.xxh->smp = m->mod.xxh->ins;
    m->c4rate = C4_NTSC_RATE;

    set_type(m, "Aley's Module");

    MODULE_INFO();

    PATTERN_INIT();

    /* Read and convert patterns */
    _D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

    for (i = 0; i < m->mod.xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->mod.xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64 * m->mod.xxh->chn; j++) {
	    event = &EVENT (i, j % m->mod.xxh->chn, j / m->mod.xxh->chn);
	    b = read8(f);
	    if (b)
		event->note = (b == 37) ? 0x61 : b + 36;
	    event->ins = read8(f);
	}
    }

    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    _D(_D_INFO "Loading samples: %d", m->mod.xxh->ins);

    for (i = 0; i < m->mod.xxh->ins; i++) {
	m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	snprintf(filename, NAME_SIZE, "%s.%d", basename, i + 1);
	s = fopen (filename, "rb");

	if (!(m->mod.xxi[i].nsm = (s != NULL)))
	    continue;

	fstat (fileno (s), &stat);
	b = read8(s);		/* Get first octet */
	m->mod.xxs[i].len = stat.st_size - 5 * !b;

	if (!b) {		/* Instrument with header */
	    m->mod.xxs[i].lps = read16l(f);
	    m->mod.xxs[i].lpe = read16l(f);
	    m->mod.xxs[i].flg = m->mod.xxs[i].lpe > m->mod.xxs[i].lps ? XMP_SAMPLE_LOOP : 0;
	} else {
	    fseek(s, 0, SEEK_SET);
	}

	m->mod.xxi[i].sub[0].pan = 0x80;
	m->mod.xxi[i].sub[0].vol = 0x40;
	m->mod.xxi[i].sub[0].sid = i;

	_D(_D_INFO "[%2X] %-14.14s %04x %04x %04x %c V%02x", i,
		filename, m->mod.xxs[i].len, m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
		m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ', m->mod.xxi[i].sub[0].vol);

	xmp_drv_loadpatch(ctx, s, m->mod.xxi[i].sub[0].sid,
	    XMP_SMP_UNS, &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);

	fclose(s);
    }

    /* ALM is LRLR, not LRRL */
    for (i = 0; i < m->mod.xxh->chn; i++)
	m->mod.xxc[i].pan = (i % 2) * 0xff;

    return 0;
}
