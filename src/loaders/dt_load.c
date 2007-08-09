/* Digital Tracker DTM loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: dt_load.c,v 1.1 2007-08-09 22:00:05 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include "load.h"
#include "iff.h"
#include "period.h"


static void get_patt(int size, FILE *f)
{
}

static void get_inst(int size, FILE *f)
{
}

static void get_dapt(int size, FILE *f)
{
}

static void get_dait(int size, FILE *f)
{
}

int dt_load(FILE *f)
{
	char magic[4];

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp(magic, "D.T.", 4))
		return -1;

	strcpy(xmp_ctl->type, "Digital Tracker module");
	
	read32b(f);
	read32b(f);
	read32b(f);
	read16b(f);
	read32b(f);

	fread(xmp_ctl->name, 18, 1, f);

	MODULE_INFO();
	
	/* IFF chunk IDs */
	iff_register("PATT", get_patt);
	iff_register("INST", get_inst);
	iff_register("DAPT", get_dapt);
	iff_register("DAIT", get_dait);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(f);

	iff_release();

	return 0;
}
