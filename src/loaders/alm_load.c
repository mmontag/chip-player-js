/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
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

#include "loader.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


static int alm_test (FILE *, char *, const int);
static int alm_load (struct module_data *, FILE *, const int);

const struct format_loader alm_loader = {
    "Aley Keptr (ALM)",
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

static int alm_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
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
	mod->spd = afh.speed / 2;

    strncpy(modulename, m->filename, NAME_SIZE);
    basename = strtok (modulename, ".");

    afh.speed = read8(f);
    afh.length = read8(f);
    afh.restart = read8(f);
    fread(&afh.order, 128, 1, f);

    mod->len = afh.length;
    mod->rst = afh.restart;
    memcpy (mod->xxo, afh.order, mod->len);

    for (mod->pat = i = 0; i < mod->len; i++)
	if (mod->pat < afh.order[i])
	    mod->pat = afh.order[i];
    mod->pat++;

    mod->ins = 31;
    mod->trk = mod->pat * mod->chn;
    mod->smp = mod->ins;
    m->c4rate = C4_NTSC_RATE;

    set_type(m, "Aley's Module");

    MODULE_INFO();

    PATTERN_INIT();

    /* Read and convert patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64 * mod->chn; j++) {
	    event = &EVENT (i, j % mod->chn, j / mod->chn);
	    b = read8(f);
	    if (b)
		event->note = (b == 37) ? 0x61 : b + 48;
	    event->ins = read8(f);
	}
    }

    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    D_(D_INFO "Loading samples: %d", mod->ins);

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	snprintf(filename, NAME_SIZE, "%s.%d", basename, i + 1);
	s = fopen (filename, "rb");

	if (!(mod->xxi[i].nsm = (s != NULL)))
	    continue;

	fstat (fileno (s), &stat);
	b = read8(s);		/* Get first octet */
	mod->xxs[i].len = stat.st_size - 5 * !b;

	if (!b) {		/* Instrument with header */
	    mod->xxs[i].lps = read16l(f);
	    mod->xxs[i].lpe = read16l(f);
	    mod->xxs[i].flg = mod->xxs[i].lpe > mod->xxs[i].lps ? XMP_SAMPLE_LOOP : 0;
	} else {
	    fseek(s, 0, SEEK_SET);
	}

	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].vol = 0x40;
	mod->xxi[i].sub[0].sid = i;

	D_(D_INFO "[%2X] %-14.14s %04x %04x %04x %c V%02x", i,
		filename, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ', mod->xxi[i].sub[0].vol);

	load_sample(s, SAMPLE_FLAG_UNS, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);

	fclose(s);
    }

    /* ALM is LRLR, not LRRL */
    for (i = 0; i < mod->chn; i++)
	mod->xxc[i].pan = (i % 2) * 0xff;

    return 0;
}
