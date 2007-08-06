/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: formats.c,v 1.16 2007-08-06 22:22:20 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "xmpi.h"
#include "formats.h"

struct xmp_fmt_info *__fmt_head;


static void register_fmt (char *suffix, char *tracker, int (*loader) ())
{
    struct xmp_fmt_info *f;

    f = malloc (sizeof (struct xmp_fmt_info));
    f->tracker = tracker;
    f->suffix = suffix;
    f->loader = loader;

    if (!__fmt_head)
	__fmt_head = f;
    else {
	struct xmp_fmt_info *i;
	for (i = __fmt_head; i->next; i = i->next);
	i->next = f;
    }

    f->next = NULL;
}


void xmp_init_formats ()
{
    register_fmt ("XM", "Fast Tracker II", xm_load);
    register_fmt ("MOD", "Noise/Fast/Protracker", mod_load);
    register_fmt ("MOD", "Startrekker/Audio Sculpure", flt_load);
    register_fmt ("M15", "Soundtracker", st_load);
    register_fmt ("IT", "Impulse Tracker", it_load);
    register_fmt ("S3M", "Scream Tracker 3", s3m_load);
    register_fmt ("STM", "Scream Tracker 2", stm_load);
    register_fmt ("STX", "STMIK 0.2", stx_load);
    register_fmt ("MTM", "Multitracker", mtm_load);
    register_fmt ("MTN", "Soundtracker 2.6/Ice Tracker", ice_load);
    register_fmt ("IMF", "Imago Orpheus", imf_load);
    register_fmt ("PTM", "Protracker 3", pt3_load);
    register_fmt ("MDL", "Digitrakker", mdl_load);
    register_fmt ("ULT", "Ultra Tracker", ult_load);
    register_fmt ("LIQ", "Liquid Tracker", liq_load);
    register_fmt ("PSM", "Epic Megagames MASI", psm_load);
    register_fmt ("PSM", "Silverball MASI", svb_load);
    register_fmt ("AMF", "DSMI (DMP)", amf_load);
    register_fmt ("MMD0/1", "OctaMED", mmd1_load);
    register_fmt ("MMD2/3", "OctaMED", mmd3_load);
#if 0
    register_fmt ("MED2/3", "MED 1.1/2.0", med2_load);
    register_fmt ("MED4", "MED 3.22", med4_load);
#endif
    register_fmt ("RTM", "Real Tracker", rtm_load);
    register_fmt ("PTM", "Poly Tracker", ptm_load);
    register_fmt ("DIGI", "DIGI Booster", digi_load);
#if 0
    register_fmt ("DBM", "DigiBooster Pro", dbm_load);
#endif
    register_fmt ("EMOD", "Quadra Composer", emod_load);
    register_fmt ("OKT", "Oktalyzer", okt_load);
    register_fmt ("SFX", "SoundFX 1.3", sfx_load);
    register_fmt ("FAR", "Farandole Composer", far_load);
    register_fmt ("STIM", "Slamtilt", stim_load);
#if 0
    register_fmt ("FC-M", "FC-M Packer", fcm_load);
    register_fmt ("KSM", "Kefrens Sound Machine", ksm_load);
#endif
    register_fmt ("IMS", "Images Music System", ims_load);
#if 0
    register_fmt ("WN", "Wanton Packer", wn_load);
    register_fmt ("PM", "Power Music", pm_load);
    register_fmt ("KRIS", "ChipTracker", kris_load);
    register_fmt ("UNIC", "Unic Tracker", unic_load);
    register_fmt ("P60A", "The Player 6.0a", p60a_load);
    register_fmt ("PRU1", "ProRunner 1.0", pru1_load);
    register_fmt ("PRU2", "ProRunner 2.0", pru2_load);
    register_fmt ("PM01", "Promizer 0.1", pm01_load);
    register_fmt ("PM10", "Promizer 1.0c", pm10_load);
    register_fmt ("PM18", "Promizer 1.8a", pm18_load);
    register_fmt ("PM20", "Promizer 2.0", pm20_load);
    register_fmt ("PM40", "Promizer 4.0", pm40_load);
    register_fmt ("AC1D", "AC1D Packer", ac1d_load);
    register_fmt ("PP10", "Pha Packer", pha_load);
#endif
    register_fmt ("XANN", "XANN Packer", xann_load);
#if 0
    register_fmt ("ZEN", "Zen Packer", zen_load);
    register_fmt ("NP", "NoisePacker", np_load);
    register_fmt ("DI", "Digital Illusions", di_load);
    register_fmt ("MP", "Module Protector", mp_load);
#endif
    register_fmt ("669", "Composer 669", ssn_load);
    register_fmt ("FNK", "Funktracker", fnk_load);
    register_fmt ("AMD", "Amusic Adlib Tracker", amd_load);
    register_fmt ("RAD", "Reality Adlib Tracker", rad_load);
    register_fmt ("HSC", "HSC-Tracker", hsc_load);
#if 0
    register_fmt ("CRB", "Heatseeker", crb_load);
#endif
    register_fmt ("ALM", "Aley Keptr", alm_load);
}

