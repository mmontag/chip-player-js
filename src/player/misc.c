/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: misc.c,v 1.4 2007-08-04 20:08:15 cmatsuoka Exp $
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
#include "xmpi.h"

#ifndef HAVE_BROKEN_STDARG
#include <stdarg.h>
#endif


/* This conditional looks pretty weird, but the Sun machine I was using 
 * for the tests (barracuda.inf.ufpr.br) actually has a "broken stdarg".
 */
#ifndef HAVE_BROKEN_STDARG

int report (char *fmt,...)
{
    va_list a;
    int n;

    va_start (a, fmt);
    n = vfprintf (stderr, fmt, a);
    va_end (a);

    return n;
}

#endif


char *copy_adjust(uint8 *s, uint8 *r, int n)
{
    int i;

    if (n > strlen((char *)r))
	n = strlen((char *)r);

    memset(s, 0, n);
    strncpy((char *)s, (char *)r, n);

    for (i = 0; i < n; i++)
	if (!isprint(s[i]) || ((uint8) s[i] > 127))
	    s[i] = ' ';

    while (*s && (s[strlen((char *)s) - 1] == ' '))
	s[strlen((char *)s) - 1] = 0;

    return (char *)s;
}

char *str_adj (char *s)
{
    int i;

    for (i = 0; i < strlen (s); i++)
	if (!isprint (s[i]) || ((uint8) s[i] > 127))
	    s[i] = ' ';

    while (*s && (s[strlen (s) - 1] == ' '))
	s[strlen (s) - 1] = 0;

    return s;
}
