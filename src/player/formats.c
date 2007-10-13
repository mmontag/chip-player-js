/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: formats.c,v 1.59 2007-10-13 23:21:26 cmatsuoka Exp $
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

#define LOADER(x) do { \
	extern struct xmp_loader_info x##_loader; \
	register_loader(&x##_loader); \
} while (0)

void xmp_init_formats()
{
	LOADER(xm);
	LOADER(mod);
	LOADER(flt);
	LOADER(st);
	LOADER(it);
	LOADER(s3m);
	LOADER(stm);
	LOADER(stx);
	LOADER(mtm);
	LOADER(ice);
	LOADER(imf);
	LOADER(ptm);
	LOADER(mdl);
	LOADER(ult);
	LOADER(liq);
	LOADER(no);
	LOADER(psm);
	LOADER(svb);
	LOADER(amf);
	LOADER(mmd1);
	LOADER(mmd3);
	LOADER(med3);
	LOADER(med4);
	LOADER(dmf);
	LOADER(rtm);
	LOADER(pt3);
	LOADER(tcb);

	//old_register_loader("RTM", "Real Tracker", rtm_load);
	//old_register_loader("PTM", "Protracker 3", pt3_load);
	//old_register_loader("TCB", "TCB Tracker", tcb_load);
	old_register_loader("DTM", "Digital Tracker", dt_load);
	old_register_loader("GTK", "Graoumf Tracker", gtk_load);
	old_register_loader("DTT", "Desktop Tracker", dtt_load);
	old_register_loader("MGT", "Megatracker", mgt_load);
	old_register_loader("MUSX", "Archimedes Tracker", arch_load);
	old_register_loader("DSYM", "Digital Symphony", sym_load);
	old_register_loader("DIGI", "DIGI Booster", digi_load);
	old_register_loader("DBM", "DigiBooster Pro", dbm_load);
	old_register_loader("EMOD", "Quadra Composer", emod_load);
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
