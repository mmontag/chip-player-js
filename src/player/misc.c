/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: misc.c,v 1.9 2007-10-19 12:49:01 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "xmpi.h"


int report(char *fmt, ...)
{
	va_list a;
	int n;

	va_start(a, fmt);
	n = vfprintf(stderr, fmt, a);
	va_end(a);

	return n;
}

int reportv(int v, char *fmt, ...)
{
	va_list a;
	int n;

#if 0
	if (xmp_ctl == NULL || xmp_ctl->verbose <= v)
		return 0;
#endif

	va_start(a, fmt);
	n = vfprintf(stderr, fmt, a);
	va_end(a);

	return n;
}

char *str_adj(char *s)
{
	int i;

	for (i = 0; i < strlen(s); i++)
		if (!isprint(s[i]) || ((uint8) s[i] > 127))
			s[i] = ' ';

	while (*s && (s[strlen(s) - 1] == ' '))
		s[strlen(s) - 1] = 0;

	return s;
}
