/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: readrc.c,v 1.4 2006-02-13 16:51:56 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include "xmpi.h"

static char drive_id[32];


static void delete_spaces (char *l)
{
    char *s;

    for (s = l; *s; s++) {
	if ((*s == ' ') || (*s == '\t')) {
	    memmove (s, s + 1, strlen (s));
	    s--;
	}
    }
}


static int get_yesno (char *s)
{
    return !(strncmp (s, "y", 1) && strncmp (s, "o", 1));
}


int xmpi_read_rc (struct xmp_control *ctl)
{
    FILE *rc;
    char myrc[MAXPATHLEN], myrc2[MAXPATHLEN];
    char *home = getenv ("HOME");
    char *hash, *var, *val, line[256];
    char cparm[512];

#ifndef __EMX__
    snprintf(myrc, MAXPATHLEN, "%s/.xmp/xmp.conf", home);
    snprintf(myrc2, MAXPATHLEN, "%s/.xmprc", home);
#else
    snprintf(myrc, MAXPATHLEN, "%s\\.xmp\\xmp.conf", home);
    snprintf(myrc2, MAXPATHLEN, "%s\\.xmprc", home);    
#endif
    if ((rc = fopen (myrc2, "r")) == NULL) {
	if ((rc = fopen (myrc, "r")) == NULL) {
#ifndef __EMX__
	    if ((rc = fopen ("/etc/xmp/xmp.conf", "r")) == NULL) {
#else
	    if ((rc = fopen ("xmp.conf", "r")) == NULL) {
#endif
		return -1;
	    }
	}
    }

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
	if (!strcmp(var,x)) { if (get_yesno (val)) ctl->w |= (y); \
	    else ctl->w &= ~(y); continue; } }

#define getval_no(x,y) { \
	if (!strcmp(var,x)) { y = atoi (val); continue; } }

	getval_yn (flags, "8bit", XMP_CTL_8BIT);
	getval_yn (flags, "interpolate", XMP_CTL_ITPT);
	getval_yn (flags, "loop", XMP_CTL_LOOP);
	getval_yn (flags, "reverse", XMP_CTL_REVERSE);
	getval_yn (flags, "pan", XMP_CTL_DYNPAN);
	getval_yn (flags, "filter", XMP_CTL_FILTER);
#if 0
	getval_yn (flags, "fixloop", XMP_CTL_FIXLOOP);
	getval_yn (flags, "fx9bug", XMP_CTL_FX9BUG);
#endif
	getval_yn (outfmt, "mono", XMP_FMT_MONO);
	getval_no ("mix", ctl->mix);
	getval_no ("crunch", ctl->crunch);
	getval_no ("chorus", ctl->chorus);
	getval_no ("reverb", ctl->reverb);
	getval_no ("srate", ctl->freq);
	getval_no ("time", ctl->time);
	getval_no ("verbosity", ctl->verbose);

	if (!strcmp (var, "driver")) {
	    strncpy (drive_id, val, 31);
	    ctl->drv_id = drive_id;
	    continue;
	}

	if (!strcmp (var, "bits")) {
	    ctl->resol = atoi (val);
	    if (ctl->resol != 16 || ctl->resol != 8 || ctl->resol != 0)
		ctl->resol = 16;	/* #?# FIXME resol==0 -> U_LAW mode ? */
	    continue;
	}

	/* If the line does not match any of the previous parameter,
	 * send it to the device driver
	 */
	snprintf(cparm, 512, "%s=%s", var, val);
	xmp_set_driver_parameter (ctl, cparm);
    }

    fclose (rc);

    return XMP_OK;
}


static void parse_modconf (struct xmp_control *ctl, char *fn,
	unsigned crc, unsigned size)
{
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
		if (active && xmp_ctl->verbose > 2)
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

	getval_yn (fetch, "8bit", XMP_CTL_8BIT);
	getval_yn (fetch, "interpolate", XMP_CTL_ITPT);
	getval_yn (fetch, "loop", XMP_CTL_LOOP);
	getval_yn (fetch, "reverse", XMP_CTL_REVERSE);
	getval_yn (fetch, "pan", XMP_CTL_DYNPAN);
	getval_yn (fetch, "filter", XMP_CTL_FILTER);
	getval_yn (fetch, "fixloop", XMP_CTL_FIXLOOP);
	getval_yn (fetch, "fx9bug", XMP_CTL_FX9BUG);
	getval_yn (outfmt, "mono", XMP_FMT_MONO);
	getval_no ("mix", ctl->mix);
	getval_no ("crunch", ctl->crunch);
	getval_no ("chorus", ctl->chorus);
	getval_no ("reverb", ctl->reverb);
    }

    fclose (rc);
}


void xmpi_read_modconf (struct xmp_control *ctl, unsigned crc, unsigned size)
{
    char myrc[MAXPATHLEN];
    char *home = getenv ("HOME");

#ifndef __EMX__
    snprintf(myrc, MAXPATHLEN, "%s/.xmp/modules.conf", home);
    parse_modconf (ctl, "/etc/xmp/xmp-modules.conf", crc, size);
#else
    snprintf(myrc, MAXPATHLEN, "%s\\.xmp\\modules.conf", home);
    parse_modconf (ctl, "xmp-modules.conf", crc, size);
#endif
    parse_modconf (ctl, myrc, crc, size);
}

