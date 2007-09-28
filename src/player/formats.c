/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: formats.c,v 1.47 2007-09-28 13:07:40 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "xmpi.h"
#include "list.h"
#include "formats.h"

struct xmp_fmt_info *__fmt_head;

LIST_HEAD(loader_list);


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


static void register_loader(char *suffix, char *tracker, int (*loader)())
{
    struct xmp_loader_info *l;

    l = malloc(sizeof(struct xmp_loader_info));
    l->tracker = tracker;
    l->suffix = suffix;
    l->loader = loader;

    register_format(suffix, tracker);

    list_add_tail(&l->list, &loader_list);
}


void xmp_init_formats()
{
    register_loader("XM", "Fast Tracker II", xm_load);
    register_loader("MOD", "Noise/Fast/Protracker", mod_load);
    register_loader("MOD", "Startrekker/Audio Sculpure", flt_load);
    register_loader("M15", "Soundtracker", st_load);
    register_loader("IT", "Impulse Tracker", it_load);
    register_loader("S3M", "Scream Tracker 3", s3m_load);
    register_loader("STM", "Scream Tracker 2", stm_load);
    register_loader("STX", "STMIK 0.2", stx_load);
    register_loader("MTM", "Multitracker", mtm_load);
    register_loader("MTN", "Soundtracker 2.6/Ice Tracker", ice_load);
    register_loader("IMF", "Imago Orpheus", imf_load);
    register_loader("PTM", "Protracker 3", pt3_load);
    register_loader("MDL", "Digitrakker", mdl_load);
    register_loader("ULT", "Ultra Tracker", ult_load);
    register_loader("LIQ", "Liquid Tracker", liq_load);
    register_loader("LIQ", "Liquid Tracker (old)", no_load);
    register_loader("PSM", "Epic Megagames MASI", psm_load);
    register_loader("PSM", "Protracker Studio", svb_load);
    register_loader("AMF", "DSMI (DMP)", amf_load);
    register_loader("MMD0/1", "MED 3.00/OctaMED", mmd1_load);
    register_loader("MMD2/3", "OctaMED Soundstudio", mmd3_load);
    //register_loader("MED2", "MED 1.12", med2_load);
    register_loader("MED3", "MED 2.00", med3_load);
    register_loader("MED4", "MED 2.10", med4_load);
    register_loader("DMF", "X-Tracker", dmf_load);
    register_loader("RTM", "Real Tracker", rtm_load);
    register_loader("PTM", "Poly Tracker", ptm_load);
    register_loader("TCB", "TCB Tracker", tcb_load);
    register_loader("DTM", "Digital Tracker", dt_load);
    register_loader("GTK", "Graoumf Tracker", gtk_load);
    register_loader("DTT", "Desktop Tracker", dtt_load);
    register_loader("MGT", "Megatracker", mgt_load);
    register_loader("MUSX", "Archimedes Tracker", arch_load);
    register_loader("DSYM", "Digital Symphony", sym_load);
    register_loader("DIGI", "DIGI Booster", digi_load);
    register_loader("DBM", "DigiBooster Pro", dbm_load);
    register_loader("EMOD", "Quadra Composer", emod_load);
    register_loader("OKT", "Oktalyzer", okt_load);
    register_loader("SFX", "SoundFX 1.3", sfx_load);
    register_loader("FAR", "Farandole Composer", far_load);
    register_format("UMX", "Epic Games Unreal/UT");
    register_loader("STIM", "Slamtilt", stim_load);
    register_loader("MTP", "SoundSmith/MegaTracker", mtp_load);
    register_loader("IMS", "Images Music System", ims_load);
    register_loader("XANN", "XANN Packer", xann_load);
    register_loader("669", "Composer 669", ssn_load);
    register_loader("FNK", "Funktracker", fnk_load);
    register_loader("AMD", "Amusic Adlib Tracker", amd_load);
    register_loader("RAD", "Reality Adlib Tracker", rad_load);
    register_loader("HSC", "HSC-Tracker", hsc_load);
    register_loader("ALM", "Aley Keptr", alm_load);
}

