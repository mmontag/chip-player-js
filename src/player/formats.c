/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: formats.c,v 1.57 2007-10-13 21:17:13 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "xmpi.h"
#include "list.h"
#include "formats.h"
#include "loader.h"

extern struct xmp_loader_info xm_loader;
extern struct xmp_loader_info mod_loader;
extern struct xmp_loader_info flt_loader;
extern struct xmp_loader_info st_loader;
extern struct xmp_loader_info it_loader;
extern struct xmp_loader_info s3m_loader;
extern struct xmp_loader_info stm_loader;
extern struct xmp_loader_info stx_loader;
extern struct xmp_loader_info mtm_loader;
extern struct xmp_loader_info ice_loader;
extern struct xmp_loader_info imf_loader;
extern struct xmp_loader_info ptm_loader;
extern struct xmp_loader_info mdl_loader;
extern struct xmp_loader_info ult_loader;
extern struct xmp_loader_info liq_loader;
extern struct xmp_loader_info no_loader;
extern struct xmp_loader_info psm_loader;
extern struct xmp_loader_info svb_loader;
extern struct xmp_loader_info amf_loader;
extern struct xmp_loader_info mmd1_loader;
extern struct xmp_loader_info mmd3_loader;


struct xmp_fmt_info *__fmt_head;

LIST_HEAD(loader_list);
LIST_HEAD(old_loader_list);


void register_format(char *suffix, char *tracker)
{
    struct xmp_fmt_info *f;

    f = malloc (sizeof (struct xmp_fmt_info));
    f->tracker = tracker;
    f->suffix = suffix;

    if (!__fmt_head)
        __fmt_head = f;
    else {
	struct xmp_fmt_info *i;
	for (i = __fmt_head; i->next; i = i->next);
	i->next = f;
    }

    f->next = NULL;
}


static void register_loader(struct xmp_loader_info *l)
{
    list_add_tail(&l->list, &loader_list);
    register_format(l->id, l->name);
}

static void old_register_loader(char *id, char *name, int (*loader)())
{
    struct xmp_loader_info *l;

    l = malloc(sizeof(struct xmp_loader_info));
    l->id = id;
    l->name = name;
    l->loader = loader;

    register_format(id, name);

    list_add_tail(&l->list, &old_loader_list);
}


void xmp_init_formats()
{
    register_loader(&xm_loader);
    register_loader(&mod_loader);
    register_loader(&flt_loader);
    register_loader(&st_loader);
    register_loader(&it_loader);
    register_loader(&s3m_loader);
    register_loader(&stm_loader);
    register_loader(&stx_loader);
    register_loader(&mtm_loader);
    register_loader(&ice_loader);
    register_loader(&imf_loader);
    register_loader(&ptm_loader);
    register_loader(&mdl_loader);
    register_loader(&ult_loader);
    register_loader(&liq_loader);
    register_loader(&no_loader);
    register_loader(&psm_loader);
    register_loader(&svb_loader);
    register_loader(&amf_loader);
    register_loader(&mmd1_loader);
    register_loader(&mmd3_loader);
    //old_register_loader("XM", "Fast Tracker II", xm_load);
    //old_register_loader("MOD", "Noise/Fast/Protracker", mod_load);
    //old_register_loader("MOD", "Startrekker/Audio Sculpure", flt_load);
    //old_register_loader("M15", "Soundtracker", st_load);
    //old_register_loader("IT", "Impulse Tracker", it_load);
    //old_register_loader("S3M", "Scream Tracker 3", s3m_load);
    //old_register_loader("STM", "Scream Tracker 2", stm_load);
    //old_register_loader("STX", "STMIK 0.2", stx_load);
    //old_register_loader("MTM", "Multitracker", mtm_load);
    //old_register_loader("MTN", "Soundtracker 2.6/Ice Tracker", ice_load);
    //old_register_loader("IMF", "Imago Orpheus", imf_load);
    //old_register_loader("PTM", "Poly Tracker", ptm_load);
    //old_register_loader("MDL", "Digitrakker", mdl_load);
    //old_register_loader("ULT", "Ultra Tracker", ult_load);
    //old_register_loader("LIQ", "Liquid Tracker", liq_load);
    //old_register_loader("LIQ", "Liquid Tracker (old)", no_load);
    //old_register_loader("PSM", "Epic Megagames MASI", psm_load);
    //old_register_loader("PSM", "Protracker Studio", svb_load);
    //old_register_loader("AMF", "DSMI (DMP)", amf_load);
    //old_register_loader("MMD0/1", "MED 3.00/OctaMED", mmd1_load);
    //old_register_loader("MMD2/3", "OctaMED Soundstudio", mmd3_load);
    //old_register_loader("MED2", "MED 1.12", med2_load);
    old_register_loader("MED3", "MED 2.00", med3_load);
    old_register_loader("MED4", "MED 2.10", med4_load);
    old_register_loader("DMF", "X-Tracker", dmf_load);
    old_register_loader("RTM", "Real Tracker", rtm_load);
    old_register_loader("PTM", "Protracker 3", pt3_load);
    old_register_loader("TCB", "TCB Tracker", tcb_load);
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

