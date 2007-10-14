/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: formats.c,v 1.61 2007-10-14 03:17:17 cmatsuoka Exp $
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
LIST_HEAD(old_loader_list);

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

static void old_register_loader(char *id, char *name, int (*loader) ())
{
	struct xmp_loader_info *l;

	l = malloc(sizeof(struct xmp_loader_info));
	l->id = id;
	l->name = name;
	l->loader = loader;

	register_format(id, name);

	list_add_tail(&l->list, &old_loader_list);
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

	old_register_loader("OKT", "Oktalyzer", okt_load);
	old_register_loader("SFX", "SoundFX 1.3", sfx_load);
	old_register_loader("FAR", "Farandole Composer", far_load);
	register_format("UMX", "Epic Games Unreal/UT");
	old_register_loader("STIM", "Slamtilt", stim_load);
	old_register_loader("MTP", "SoundSmith/MegaTracker", mtp_load);
	old_register_loader("IMS", "Images Music System", ims_load);
	old_register_loader("XANN", "XANN Packer", xann_load);
	old_register_loader("669", "Composer 669", ssn_load);
	old_register_loader("FNK", "Funktracker", fnk_load);
	old_register_loader("AMD", "Amusic Adlib Tracker", amd_load);
	old_register_loader("RAD", "Reality Adlib Tracker", rad_load);
	old_register_loader("HSC", "HSC-Tracker", hsc_load);
	old_register_loader("ALM", "Aley Keptr", alm_load);
}
