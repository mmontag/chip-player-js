/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef ANDROID
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "common.h"


char *str_adj(char *s)
{
	int i;

	for (i = 0; i < strlen(s); i++)
		if (!isprint((int)s[i]) || ((uint8) s[i] > 127))
			s[i] = ' ';

	while (*s && (s[strlen(s) - 1] == ' '))
		s[strlen(s) - 1] = 0;

	return s;
}

int get_temp_dir(char *buf, int size)
{
#if defined WIN32
	const char def[] = "C:\\WINDOWS\\TEMP";
	char *tmp = getenv("TEMP");

	strncpy(buf, tmp ? tmp : def, size);
	strncat(buf, "\\", size);
#elif defined __AMIGA__
	strncpy(buf, "T:", size);
#elif defined ANDROID
#define APPDIR "/sdcard/Xmp for Android"
	struct stat st;
	if (stat(APPDIR, &st) < 0) {
		if (mkdir(APPDIR, 0777) < 0)
			return -1;
	}
	if (stat(APPDIR "/tmp", &st) < 0) {
		if (mkdir(APPDIR "/tmp", 0777) < 0)
			return -1;
	}
	strncpy(buf, APPDIR "/tmp/", size);
#else
	const char def[] = "/tmp";
	char *tmp = getenv("TMPDIR");

	strncpy(buf, tmp ? tmp : def, size);
	strncat(buf, "/", size);
#endif

	return 0;
}

