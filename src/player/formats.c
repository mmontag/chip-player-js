/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: formats.c,v 1.64 2007-10-17 13:08:53 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "xmpi.h"
#include "list.h"
#include "formats.h"
#include "loader.h"

struct xmp_fmt_info *__fmt_head;

LIST_HEAD(loader_list);

void register_format(char *suffix, char *tracker)
{
	struct xmp_fmt_info *f;

	f = malloc(sizeof(struct xmp_fmt_info));
	f->tracker = tracker;
	f->suffix = suffix;

	if (!__fmt_head)
		__fmt_head = f;
	else {
		struct xmp_fmt_info *i;
		for (i = __fmt_head; i->next; i = i->next) ;
		i->next = f;
	}

	f->next = NULL;
}

static void register_loader(struct xmp_loader_info *l)
{
	list_add_tail(&l->list, &loader_list);
	register_format(l->id, l->name);
}

#define REG_LOADER(x) do { \
	extern struct xmp_loader_info x##_loader; \
	register_loader(&x##_loader); \
} while (0)

void xmp_init_formats()
{
	REG_LOADER(xm);
	REG_LOADER(mod);
	REG_LOADER(flt);
	REG_LOADER(st);
	REG_LOADER(it);
	REG_LOADER(s3m);
	REG_LOADER(stm);
	REG_LOADER(stx);
	REG_LOADER(mtm);
	REG_LOADER(ice);
	REG_LOADER(imf);
	REG_LOADER(ptm);
	REG_LOADER(mdl);
	REG_LOADER(ult);
	REG_LOADER(liq);
	REG_LOADER(no);
	REG_LOADER(psm);
	REG_LOADER(svb);
	REG_LOADER(amf);
	REG_LOADER(mmd1);
	REG_LOADER(mmd3);
	REG_LOADER(med3);
	REG_LOADER(med4);
	REG_LOADER(dmf);
	REG_LOADER(rtm);
	REG_LOADER(pt3);
	REG_LOADER(tcb);
	REG_LOADER(dt);
	REG_LOADER(gtk);
	REG_LOADER(dtt);
	REG_LOADER(mgt);
	REG_LOADER(arch);
	REG_LOADER(sym);
	REG_LOADER(digi);
	REG_LOADER(dbm);
	REG_LOADER(emod);
	REG_LOADER(okt);
	REG_LOADER(sfx);
	REG_LOADER(far);
	REG_LOADER(umx);
	REG_LOADER(stim);
	REG_LOADER(mtp);
	REG_LOADER(ims);
	REG_LOADER(ssn);
	REG_LOADER(fnk);
	REG_LOADER(amd);
	REG_LOADER(rad);
	REG_LOADER(hsc);
	REG_LOADER(alm);
}
