/* Extended Module Player
 * Copyright (C) 1997 Claudio Matsuoka and Hipolito Carraro Jr
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
#include "xmpi.h"
#include "iff.h"

static struct iff_info *iff_head = NULL;
static int __id_size;
static int __flags;

void iff_chunk (FILE *f)
{
    long size;
    char id[17] = "";

    if (fread (id, 1, __id_size, f) != __id_size)
	return;
    if (fread (&size, 1, 4, f) != 4)
	return;
    if (__flags & IFF_LITTLE_ENDIAN)
	L_ENDIAN32 (size);
    else
	B_ENDIAN32 (size);
    if (__flags & IFF_FULL_CHUNK_SIZE)
	size -= __id_size + 4;
    iff_process (id, size, f);
}


void iff_register (char *id, void (*loader) ())
{
    struct iff_info *f;

    __id_size = 4;
    __flags = 0;
    f = malloc (sizeof (struct iff_info));
    strcpy (f->id, id);
    f->loader = loader;
    if (!iff_head) {
	iff_head = f;
	f->prev = NULL;
    } else {
	struct iff_info *i;
	for (i = iff_head; i->next; i = i->next);
	i->next = f;
	f->prev = i;
    }
    f->next = NULL;
}


void iff_release ()
{
    struct iff_info *i;

    for (i = iff_head; i->next; i = i->next);
    while (i->prev) {
	i = i->prev;
	free (i->next);
	i->next = NULL;
    }
    free (iff_head);
    iff_head = NULL;
}


int iff_process (char *id, long size, FILE *f)
{
    char *buffer;
    struct iff_info *i;

    if ((buffer = malloc (size + 2)) == NULL)
	return -1;
    fread (buffer, 1, size, f);
    for (i = iff_head; i; i = i->next) {
	if (id && !strncmp (id, i->id, __id_size)) {
	    i->loader (size, buffer);
	    break;
	}
    }
    free (buffer);
    return 0;
}


/* Functions to tune IFF mutations */

void iff_idsize (int n)
{
    __id_size = n;
}

void iff_setflag (int i)
{
    __flags |= i;
}

