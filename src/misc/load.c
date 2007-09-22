/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: load.c,v 1.22 2007-09-22 20:28:57 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMX__
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include "driver.h"
#include "convert.h"

extern struct xmp_fmt_info *__fmt_head;

#include "list.h"
#include "../prowizard/prowiz.h"

extern struct list_head *checked_format;

int decrunch_arc (FILE *, FILE *);
int decrunch_arcfs (FILE *, FILE *);
int decrunch_sqsh (FILE *, FILE *);
int decrunch_pp (FILE *, FILE *);
int decrunch_mmcmp (FILE *, FILE *);
int decrunch_umx (FILE *, FILE *);
int decrunch_pw (FILE *, FILE *);
int pw_check(unsigned char *, int);

#define BUILTIN_PP	0x01
#define BUILTIN_SQSH	0x02
#define BUILTIN_MMCMP	0x03
#define BUILTIN_PW	0x04
#define BUILTIN_ARC	0x05
#define BUILTIN_ARCFS	0x06
#define BUILTIN_S404	0x07
#define BUILTIN_UMX	0x08


static int decrunch(FILE **f, char **s)
{
    unsigned char *b;
    char *cmd;
    FILE *t;
    int fd, builtin, res;
    char *packer, *temp, *temp2;

    packer = cmd = NULL;
    builtin = res = 0;
    temp = strdup ("/tmp/xmp_XXXXXX");

    fseek (*f, 0, SEEK_SET);
    b = calloc(1, PW_TEST_CHUNK);
    fread (b, 1, PW_TEST_CHUNK, *f);

    if (b[0] == 'P' && b[1] == 'K') {
	packer = "Zip";
#ifdef __EMX__
	cmd = "unzip -pqqC \"%s\" -x readme '*.diz' '*.nfo' '*.txt' "
		"'*.exe' '*.com' 2>NUL";
#else
	cmd = "unzip -pqqC \"%s\" -x readme '*.diz' '*.nfo' '*.txt' "
		"'*.exe' '*.com' 2>/dev/null";
#endif
    } else if (b[2] == '-' && b[3] == 'l' && b[4] == 'h') {
	packer = "LHa";
#ifdef __EMX__
	fprintf( stderr, "LHA for OS/2 does NOT support output to stdout.\n" );
#endif
	cmd = "lha -pq \"%s\"";
    } else if (b[0] == 31 && b[1] == 139) {
	packer = "gzip";
	cmd = "gzip -dc \"%s\"";
    } else if (b[0] == 'B' && b[1] == 'Z' && b[2] == 'h') {
	packer = "bzip2";
	cmd = "bzip2 -dc \"%s\"";
    } else if (b[0] == 0x5d && b[1] == 0 && b[2] == 0 && b[3] == 0x80) {
	packer = "lzma";
	cmd = "lzma -dc \"%s\"";
    } else if (b[0] == 'Z' && b[1] == 'O' && b[2] == 'O' && b[3] == ' ') {
	packer = "zoo";
	cmd = "zoo xpq \"%s\"";
    } else if (b[0] == 'M' && b[1] == 'O' && b[2] == '3') {
	packer = "MO3";
	cmd = "unmo3 -s \"%s\" STDOUT";
    } else if (b[0] == 31 && b[1] == 157) {
	packer = "compress";
#ifdef __EMX__
	fprintf( stderr, "I was unable to find a OS/2 version of UnCompress...\n" );
	fprintf( stderr, "I hope that the default command will work if a OS/2 version is found/created!\n" );
#endif
	cmd = "uncompress -c \"%s\"";
    } else if (b[0] == 'P' && b[1] == 'P' && b[2] == '2' && b[3] == '0') {
	packer = "PowerPack";
	builtin = BUILTIN_PP;
    } else if (b[0] == 'X' && b[1] == 'P' && b[2] == 'K' && b[3] == 'F' &&
	b[8] == 'S' && b[9] == 'Q' && b[10] == 'S' && b[11] == 'H') {
	packer = "SQSH";
	builtin = BUILTIN_SQSH;
    } else if (!memcmp(b, "Archive\0", 8)) {
	packer = "ArcFS";
	builtin = BUILTIN_ARCFS;
    } else if (b[0] == 'z' && b[1] == 'i' && b[2] == 'R' && b[3] == 'C' &&
		b[4] == 'O' && b[5] == 'N' && b[6] == 'i' && b[7] == 'a') {
	packer = "MMCMP";
	builtin = BUILTIN_MMCMP;
    } else if (b[0] == 'R' && b[1] == 'a' && b[2] == 'r') {
	packer = "rar";
	cmd = "unrar p -inul -xreadme -x*.diz -x*.nfo -x*.txt "
	    "-x*.exe -x*.com \"%s\"";
    } else if (b[0] == 0xc1 && b[1] == 0x83 && b[2] == 0x2a && b[3] == 0x9e) {
	packer = "UMX";
	builtin = BUILTIN_UMX;
    } else {
	int extra;
	int s = PW_TEST_CHUNK;

	while ((extra = pw_check(b, s)) > 0) {
	    b = realloc(b, s + extra);
	    fread(b + s, extra, 1, *f);
	    s += extra;
	}

	if (extra == 0) {
	    struct pw_format *format;

	    format = list_entry(checked_format, struct pw_format, list);
	    packer = format->name;
	    builtin = BUILTIN_PW;
	}
    }

    /* Test Arc after prowizard to prevent misidentification */
    if (packer == 0 && b[0] == 0x1a) {
	int x = b[1] & 0x7f;
	if (x >= 1 && x <= 9 && x != 7) {
	    packer = "Arc";
	    builtin = BUILTIN_ARC;
	} else if (x == 0x7f) {
	    packer = "!Spark";
	    builtin = BUILTIN_ARC;
	}
    }

    free(b);

    fseek (*f, 0, SEEK_SET);

    if (!packer)
	return 0;

    if (xmp_ctl->verbose)
	report ("Depacking %s file... ", packer);

    if ((fd = mkstemp (temp)) < 0) {
	report ("failed\n");
	return -1;
    }

    if ((t = fdopen (fd, "w+b")) == NULL) {
	report ("failed\n");
	return -1;
    }

    if (cmd) {
	int n, lsize;
	char *line, *buf;
	FILE *p;

	lsize = strlen(cmd) + strlen(*s) + 16;
	line = malloc(lsize);
	snprintf (line, lsize, cmd, *s);

	if ((p = popen (line, "r")) == NULL) {
	    if (xmp_ctl->verbose)
		report ("failed\n");
	    fclose (t);
	    free (line);
            unlink (temp);
	    free (temp);
	    return -1;
	}
	free (line);
#define BSIZE 0x4000
	if ((buf = malloc (BSIZE)) == NULL) {
	    if (xmp_ctl->verbose)
		report ("failed\n");
            unlink (temp);
	    free (temp);
	    pclose (p);
	    fclose (t);
	    return -1;
	}
	while ((n = fread (buf, 1, BSIZE, p)) > 0) {
	    fwrite (buf, 1, n, t);
	} 
	free (buf);
	pclose (p);
    } else {
	switch (builtin) {
	case BUILTIN_PP:    
	    res = decrunch_pp(*f, t);
	    break;
	case BUILTIN_ARC:
	    res = decrunch_arc(*f, t);
	    break;
	case BUILTIN_ARCFS:
	    res = decrunch_arcfs(*f, t);
	    break;
	case BUILTIN_SQSH:    
	    res = decrunch_sqsh(*f, t);
	    break;
	case BUILTIN_MMCMP:    
	    res = decrunch_mmcmp(*f, t);
	    break;
	case BUILTIN_UMX:
	    res = decrunch_umx(*f, t);
	    break;
	case BUILTIN_PW:
	    res = decrunch_pw(*f, t);
	    break;
	}
    }

    if (res < 0) {
	if (xmp_ctl->verbose)
	    report ("failed\n");
	unlink (temp);
	free (temp);
	return -1;
    }

    if (xmp_ctl->verbose)
	report ("done\n");

    fclose (*f);
    *f = t;
 
    temp2 = strdup (temp);
    decrunch(f, &temp);
    unlink (temp2);
    free (temp2);
    free (temp);

    return 1;
}


static void get_smp_size (int awe, int *a, int *b)
{
    int i, len, smp_size, smp_4kb;

    for (smp_4kb = smp_size = i = 0; i < xxh->smp; i++) {
	len = xxs[i].len;

	/* AWE cards work with 16 bit samples only and expand bidirectional
	 * loops.
	 */
	if (awe) {
	    if (xxs[i].flg & WAVE_BIDIR_LOOP)
		len += xxs[i].lpe - xxs[i].lps;
	    if (awe && (~xxs[i].flg & WAVE_16_BITS))
		len <<= 1;
	}

	smp_size += (len += sizeof (int)); 
	if (len < 0x1000)
	    smp_4kb += len;
    }

    *a = smp_size;
    *b = smp_4kb;
}


static int crunch_ratio (int awe)
{
    int memavl, smp_size, ratio, smp_4kb;

    ratio = 0x10000;
    if (!(memavl = xmp_ctl->memavl))
	return ratio;

    memavl = memavl * 100 / (100 + xmp_ctl->crunch);

    get_smp_size (awe, &smp_size, &smp_4kb);

    if (smp_size > memavl) {
	if (!awe)
	    xmp_cvt_to8bit ();
	get_smp_size (awe, &smp_size, &smp_4kb);
    }

    if (smp_size > memavl) {
	ratio = (int)
	    (((long long)(memavl - smp_4kb) << 16) / (smp_size - smp_4kb));
	if (xmp_ctl->verbose)
	    report ("Crunch ratio   : %d%% [Mem:%.3fMb Smp:%.3fMb]\n",
		100 - 100 * ratio / 0x10000, .000001 * xmp_ctl->memavl,
		.000001 * smp_size);
    }
	
    return ratio;
}


int xmp_load_module (char *s)
{
    FILE *f;
    int i, t;
    struct xmp_fmt_info *fmt;
    struct stat st;
    unsigned int crc;

    if ((f = fopen (s, "rb")) == NULL)
	return -3;

    if (fstat (fileno (f), &st) < 0)
	return -3;

    if (S_ISDIR (st.st_mode))
	return -1;

    if ((t = decrunch (&f, &s)) < 0) {
	fclose (f);
	return -1;
    }

    if (fstat (fileno (f), &st) < 0)	/* get size after decrunch */
	return -3;

    crc = cksum (f);

    xmp_drv_clearmem ();

    /* Reset variables */
    memset (xmp_ctl->name, 0, XMP_DEF_NAMESIZE);
    memset (xmp_ctl->type, 0, XMP_DEF_NAMESIZE);
    xmp_ctl->filename = s;		/* For ALM */
    xmp_ctl->size = st.st_size;
    xmp_ctl->rrate = PAL_RATE;
    xmp_ctl->c4rate = C4_PAL_RATE;
    xmp_ctl->volbase = 0x40;
    xmp_ctl->volume = 0x40;
    xmp_ctl->vol_xlat = NULL;
    /* Reset control for next module */
    xmp_ctl->fetch = xmp_ctl->flags & ~XMP_CTL_FILTER;

    xmpi_read_modconf (xmp_ctl, crc, st.st_size);

    xxh = calloc (sizeof (struct xxm_header), 1);
    /* Set defaults */
    xxh->tpo = 6;
    xxh->bpm = 125;
    xxh->chn = 4;

    for (i = 0; i < 64; i++) {
	xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
	xxc[i].cho = xmp_ctl->chorus;
	xxc[i].rvb = xmp_ctl->reverb;
	xxc[i].vol = 0x40;
    }

    for (i = 0, fmt = __fmt_head; fmt; fmt = fmt->next) {
	if (fmt->loader && ((i = fmt->loader (f)) != -1))
	    break;
    }

    fclose (f);

    if (i < 0)
	return i;

    if (xmp_ctl->description && (i = (strstr (xmp_ctl->description, " [AWE") != NULL))) {
	xmp_cvt_to16bit ();
	xmp_cvt_bid2und ();
    }

    xmp_drv_flushpatch (crunch_ratio (i));

    str_adj (xmp_ctl->name);
    if (!*xmp_ctl->name)
	strcpy (xmp_ctl->name, "(untitled)");

    if (xmp_ctl->verbose > 1) {
	report ("Module looping : %s\n",
	    xmp_ctl->fetch & XMP_CTL_LOOP ? "yes" : "no");
	report ("Period mode    : %s\n",
	    xxh->flg & XXM_FLG_LINEAR ? "linear" : "Amiga");
    }

    if (xmp_ctl->verbose > 2) {
	report ("Amiga range    : %s\n", xxh->flg & XXM_FLG_MODRNG ?
		"yes" : "no");
	report ("Restart pos    : %d\n", xxh->rst);
	report ("Base volume    : %d\n", xmp_ctl->volbase);
	report ("C4 replay rate : %d\n", xmp_ctl->c4rate);
	report ("Channel mixing : %d%% (dynamic pan %s)\n",
		xmp_ctl->fetch & XMP_CTL_REVERSE ? -xmp_ctl->mix : xmp_ctl->mix,
		xmp_ctl->fetch & XMP_CTL_DYNPAN ? "enabled" : "disabled");
    }

    if (xmp_ctl->verbose) {
	report ("Channels       : %d [ ", xxh->chn);
	for (i = 0; i < xxh->chn; i++) {
	    if (xxc[i].flg & XXM_CHANNEL_FM)
		report ("F ");
	    else
	        report ("%x ", xxc[i].pan >> 4);
	}
	report ("]\n");
    }

    t = xmpi_scan_module ();

    if (xmp_ctl->verbose) {
	if (xmp_ctl->fetch & XMP_CTL_LOOP)
	    report ("One loop time  : %dmin%02ds\n",
		(t + 500) / 60000, ((t + 500) / 1000) % 60);
	else
	    report ("Estimated time : %dmin%02ds\n",
		(t + 500) / 60000, ((t + 500) / 1000) % 60);
    }

    return t;
}
