/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: readrc.c,v 1.15 2007-12-01 17:27:09 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include "xmpi.h"

#ifdef __AMIGA__
#include <sys/unistd.h>
#endif

/* TODO: use .ini file in Windows */

static char drive_id[32];


static void delete_spaces(char *l)
{
    char *s;

    for (s = l; *s; s++) {
	if ((*s == ' ') || (*s == '\t')) {
	    memmove (s, s + 1, strlen (s));
	    s--;
	}
    }
}


static int get_yesno(char *s)
{
    return !(strncmp (s, "y", 1) && strncmp (s, "o", 1));
}


int xmpi_read_rc(struct xmp_context *ctx)
{
    struct xmp_options *o = &ctx->o;
    FILE *rc;
    char myrc[MAXPATHLEN], myrc2[MAXPATHLEN];
    char *hash, *var, *val, line[256];
    char cparm[512];

#if defined __EMX__
    char *home = getenv("HOME");

    snprintf(myrc, MAXPATHLEN, "%s\\.xmp\\xmp.conf", home);
    snprintf(myrc2, MAXPATHLEN, "%s\\.xmprc", home);    

    if ((rc = fopen(myrc2, "r")) == NULL) {
	if ((rc = fopen(myrc, "r")) == NULL) {
	    if ((rc = fopen("xmp.conf", "r")) == NULL) {
		return -1;
	    }
	}
    }
#elif defined __AMIGA__
    strncpy(myrc, "PROGDIR:xmp.conf", MAXPATHLEN);

    if ((rc = fopen(myrc, "r")) == NULL)
	return -1;
#else
    char *home = getenv("HOME");

    snprintf(myrc, MAXPATHLEN, "%s/.xmp/xmp.conf", home);
    snprintf(myrc2, MAXPATHLEN, "%s/.xmprc", home);

    if ((rc = fopen(myrc2, "r")) == NULL) {
	if ((rc = fopen(myrc, "r")) == NULL) {
	    if ((rc = fopen(SYSCONFDIR "/xmp.conf", "r")) == NULL) {
		return -1;
	    }
	}
    }
#endif

    while (!feof (rc)) {
	memset (line, 0, 256);
	fscanf (rc, "%255[^\n]", line);
	fgetc (rc);

	/* Delete comments */
	if ((hash = strchr (line, '#')))
	    *hash = 0;

	delete_spaces (line);

	if (!(var = strtok (line, "=\n")))
	    continue;

	val = strtok (NULL, " \t\n");

#define getval_yn(w,x,y) { \
	if (!strcmp(var,x)) { if (get_yesno (val)) w |= (y); \
	    else w &= ~(y); continue; } }

#define getval_no(x,y) { \
	if (!strcmp(var,x)) { y = atoi (val); continue; } }

	getval_yn(o->flags, "8bit", XMP_CTL_8BIT);
	getval_yn(o->flags, "interpolate", XMP_CTL_ITPT);
	getval_yn(o->flags, "loop", XMP_CTL_LOOP);
	getval_yn(o->flags, "reverse", XMP_CTL_REVERSE);
	getval_yn(o->flags, "pan", XMP_CTL_DYNPAN);
	getval_yn(o->flags, "filter", XMP_CTL_FILTER);
	getval_yn(o->outfmt, "mono", XMP_FMT_MONO);
	getval_no("mix", o->mix);
	getval_no("crunch", o->crunch);
	getval_no("chorus", o->chorus);
	getval_no("reverb", o->reverb);
	getval_no("srate", o->freq);
	getval_no("time", o->time);
	getval_no("verbosity", o->verbosity);

	if (!strcmp (var, "driver")) {
	    strncpy (drive_id, val, 31);
	    o->drv_id = drive_id;
	    continue;
	}

	if (!strcmp (var, "bits")) {
	    o->resol = atoi (val);
	    if (o->resol != 16 || o->resol != 8 || o->resol != 0)
		o->resol = 16;	/* #?# FIXME resol==0 -> U_LAW mode ? */
	    continue;
	}

	/* If the line does not match any of the previous parameter,
	 * send it to the device driver
	 */
	snprintf(cparm, 512, "%s=%s", var, val);
	xmp_set_driver_parameter(&ctx->o, cparm);
    }

    fclose (rc);

    return XMP_OK;
}


static void parse_modconf(struct xmp_context *ctx, char *fn, unsigned crc, unsigned size)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &p->m;
    struct xmp_options *o = &ctx->o;
    FILE *rc;
    char *hash, *var, *val, line[256];
    int active = 0;

    if ((rc = fopen (fn, "r")) == NULL)
	return;

    while (!feof (rc)) {
	memset (line, 0, 256);
	fscanf (rc, "%255[^\n]", line);
	fgetc (rc);

	/* Delete comments */
	if ((hash = strchr (line, '#')))
	    *hash = 0;

	if (line[0] == ':') {
	    strtok (&line[1], " "); 
	    val = strtok (NULL, " \t\n");
	    if (strtoul (&line[1], NULL, 0) && strtoul (val, NULL, 0)) {
		active = (strtoul (&line[1], NULL, 0) == crc &&
	 	    strtoul (val, NULL, 0) == size);
		if (active && o->verbosity > 2)
		    report ("Matching CRC in %s (%u)\n", fn, crc);
	    }
	    continue;
 	}
	
	if (!active)
	    continue;

	delete_spaces (line);

	if (!(var = strtok (line, "=\n")))
	    continue;

	val = strtok (NULL, " \t\n");

	getval_yn(m->fetch, "8bit", XMP_CTL_8BIT);
	getval_yn(m->fetch, "interpolate", XMP_CTL_ITPT);
	getval_yn(m->fetch, "loop", XMP_CTL_LOOP);
	getval_yn(m->fetch, "reverse", XMP_CTL_REVERSE);
	getval_yn(m->fetch, "pan", XMP_CTL_DYNPAN);
	getval_yn(m->fetch, "filter", XMP_CTL_FILTER);
	getval_yn(m->fetch, "fixloop", XMP_CTL_FIXLOOP);
	getval_yn(m->fetch, "fx9bug", XMP_CTL_FX9BUG);
	getval_yn(o->outfmt, "mono", XMP_FMT_MONO);
	getval_no("mix", o->mix);
	getval_no("crunch", o->crunch);
	getval_no("chorus", o->chorus);
	getval_no("reverb", o->reverb);
    }

    fclose (rc);
}


void xmpi_read_modconf(struct xmp_context *ctx, unsigned crc, unsigned size)
{
#if defined __EMX__
    char myrc[MAXPATHLEN];
    char *home = getenv ("HOME");

    snprintf(myrc, MAXPATHLEN, "%s\\.xmp\\modules.conf", home);
    parse_modconf(ctx, "xmp-modules.conf", crc, size);
    parse_modconf(ctx, myrc, crc, size);
#elif defined __AMIGA__
    parse_modconf(ctx, "PROGDIR:xmp-modules.conf", crc, size);
#else
    char myrc[MAXPATHLEN];
    char *home = getenv ("HOME");

    snprintf(myrc, MAXPATHLEN, "%s/.xmp/modules.conf", home);
    parse_modconf(ctx, SYSCONFDIR "/xmp-modules.conf", crc, size);
    parse_modconf(ctx, myrc, crc, size);
#endif
}

