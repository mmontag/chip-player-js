/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: common.c,v 1.12 2007-10-13 13:56:01 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define __XMP_LOADERS_COMMON

#include "xmp.h"
#include "xmpi.h"
#include "period.h"
#include "load.h"

void read_title(FILE *f, char *t, int s)
{
	char buf[XMP_DEF_NAMESIZE];

	if (t == NULL)
		return;

	if (s >= XMP_DEF_NAMESIZE)
		s = XMP_DEF_NAMESIZE -1;

	fread(buf, 1, s, f);
	buf[s] = 0;
	copy_adjust((uint8 *)t, (uint8 *)buf, s);
}

void set_xxh_defaults(struct xxm_header *xxh)
{
	memset(xxh, 0, sizeof(struct xxm_header));
	xxh->gvl = 0x40;
	xxh->tpo = 6;
	xxh->bpm = 125;
	xxh->chn = 4;
	xxh->ins = 31;
	xxh->smp = xxh->ins;
}

void cvt_pt_event(struct xxm_event *event, uint8 *mod_event)
{
	event->note = period_to_note((LSN(mod_event[0]) << 8) + mod_event[1]);
	event->ins = ((MSN(mod_event[0]) << 4) | MSN(mod_event[2]));
	event->fxt = LSN(mod_event[2]);
	event->fxp = mod_event[3];

	disable_continue_fx(event);
}

void disable_continue_fx(struct xxm_event *event)
{
	if (!event->fxp) {
		switch (event->fxt) {
		case 0x05:
			event->fxt = 0x03;
			break;
		case 0x06:
			event->fxt = 0x04;
			break;
		case 0x01:
		case 0x02:
		case 0x0a:
			event->fxt = 0x00;
		}
	}
}
