/* Quartet 4v/set loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: quartet_load.c,v 1.1 2007-08-10 01:29:46 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define NAME_SIZE 255

int quartet_load(FILE *f)
{
	int i, j;
	struct xxm_event *event;
	struct stat stat;
	uint8 b;
	char *basename;
	char filename[NAME_SIZE];
	char modulename[NAME_SIZE];
	FILE *s;

	LOAD_INIT();

	/* We shouldn't rely on file names, but this seems to be the
	 * best way to detect Quartet files.
	 */
	i = strlen(xmp_ctl->filename);
	if (strcmp(xmp_ctl->filename + i - 3, ".4v") &&
			strcmp(xmp_ctl->filename + i - 3, ".4V"))
		return -1;

	strncpy(modulename, xmp_ctl->filename, NAME_SIZE);
	basename = strtok(modulename, ".");

	snprintf(filename, NAME_SIZE, "%s.set", basename);
	s = fopen(filename, "rb");
	if (s == NULL) {
		snprintf(filename, NAME_SIZE, "%s.SET", basename);
		s = fopen(filename, "rb");
		if (s == NULL)
			return -1;
	}

	strcpy(xmp_ctl->type, "Quartet 4V/Set");

	MODULE_INFO();

	return 0;
}
