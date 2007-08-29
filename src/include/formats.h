/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: formats.h,v 1.16 2007-08-29 00:52:06 cmatsuoka Exp $
 */

#ifndef __FORMATS_H
#define __FORMATS_H

void xmp_formats_list (const int);
void xmp_init_formats (void);

int ac1d_load (FILE *);
int alm_load (FILE *);
int amd_load (FILE *);
int amf_load (FILE *);
int amf_load (FILE *);
int aon_load (FILE *);
int arch_load (FILE *);
int crb_load (FILE *);
int dbm_load (FILE *);
int di_load (FILE *);
int digi_load (FILE *);
int dmf_load (FILE *);
int dpp_load (FILE *);
int dss_load (FILE *);
int dt_load (FILE *);
int dtt_load (FILE *);
int emod_load (FILE *);
int far_load (FILE *);
int fcm_load (FILE *);
int flt_load (FILE *);
int fnk_load (FILE *);
int gmc_load (FILE *);
int gtk_load (FILE *);
int hsc_load (FILE *);
int ice_load (FILE *);
int imf_load (FILE *);
int ims_load (FILE *);
int it_load (FILE *);
int kris_load (FILE *);
int ksm_load (FILE *);
int liq_load (FILE *);
int mdl_load (FILE *);
int mmd1_load (FILE *);
int mmd3_load (FILE *);
int med2_load (FILE *);
int med4_load (FILE *);
int mgt_load (FILE *);
int mod_load (FILE *);
int mp_load (FILE *);
int mtm_load (FILE *);
int no_load (FILE *);
int np_load (FILE *);
int okt_load (FILE *);
int pha_load (FILE *);
int pm_load (FILE *);
int pm01_load (FILE *);
int pm10_load (FILE *);
int pm18_load (FILE *);
int pm20_load (FILE *);
int pm40_load (FILE *);
int pru1_load (FILE *);
int pru2_load (FILE *);
int psm_load (FILE *);
int pt3_load (FILE *);
int ptm_load (FILE *);
int rad_load (FILE *);
int rtm_load (FILE *);
int s3m_load (FILE *);
int sfx_load (FILE *);
int ssn_load (FILE *);
int st_load (FILE *);
int stim_load (FILE *);
int stm_load (FILE *);
int stx_load (FILE *);
int svb_load (FILE *);
int sym_load (FILE *);
int tcb_load (FILE *);
int p60a_load (FILE *);
int ult_load (FILE *);
int unic_load (FILE *);
int wn_load (FILE *);
int xann_load (FILE *);
int xm_load (FILE *);
int zen_load (FILE *);

#endif /* __FORMATS_H */
