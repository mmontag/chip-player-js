/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <stdlib.h>
#include "common.h"
#include "loader.h"

int pw_init(void);

extern struct format_loader xm_loader;
extern struct format_loader mod_loader;
extern struct format_loader flt_loader;
extern struct format_loader st_loader;
extern struct format_loader it_loader;
extern struct format_loader s3m_loader;
extern struct format_loader stm_loader;
extern struct format_loader stx_loader;
extern struct format_loader mtm_loader;
extern struct format_loader ice_loader;
extern struct format_loader imf_loader;
extern struct format_loader ptm_loader;
extern struct format_loader mdl_loader;
extern struct format_loader ult_loader;
extern struct format_loader liq_loader;
extern struct format_loader no_loader;
extern struct format_loader masi_loader;
extern struct format_loader gal5_loader;
extern struct format_loader gal4_loader;
extern struct format_loader psm_loader;
extern struct format_loader amf_loader;
extern struct format_loader gdm_loader;
extern struct format_loader mmd1_loader;
extern struct format_loader mmd3_loader;
extern struct format_loader med2_loader;
extern struct format_loader med3_loader;
extern struct format_loader med4_loader;
extern struct format_loader dmf_loader;
extern struct format_loader rtm_loader;
extern struct format_loader pt3_loader;
extern struct format_loader tcb_loader;
extern struct format_loader dt_loader;
extern struct format_loader gtk_loader;
extern struct format_loader dtt_loader;
extern struct format_loader mgt_loader;
extern struct format_loader arch_loader;
extern struct format_loader sym_loader;
extern struct format_loader digi_loader;
extern struct format_loader dbm_loader;
extern struct format_loader emod_loader;
extern struct format_loader okt_loader;
extern struct format_loader sfx_loader;
extern struct format_loader far_loader;
extern struct format_loader umx_loader;
extern struct format_loader stim_loader;
extern struct format_loader coco_loader;
extern struct format_loader mtp_loader;
extern struct format_loader ims_loader;
extern struct format_loader ssn_loader;
extern struct format_loader fnk_loader;
extern struct format_loader amd_loader;
extern struct format_loader rad_loader;
extern struct format_loader hsc_loader;
extern struct format_loader mfp_loader;
extern struct format_loader alm_loader;
extern struct format_loader polly_loader;
extern struct format_loader stc_loader;
extern struct format_loader pw_loader;


struct format_loader *format_loader[] = {
	&xm_loader,
	&mod_loader,
	&flt_loader,
	&st_loader,
	&it_loader,
	&s3m_loader,
	&stm_loader,
	&stx_loader,
	&mtm_loader,
	&ice_loader,
	&imf_loader,
	&ptm_loader,
	&mdl_loader,
	&ult_loader,
	&liq_loader,
	&no_loader,
	&masi_loader,
	&gal5_loader,
	&gal4_loader,
	&psm_loader,
	&amf_loader,
	&gdm_loader,
	&mmd1_loader,
	&mmd3_loader,
	&med2_loader,
	&med3_loader,
	&med4_loader,
	&dmf_loader,
	&rtm_loader,
	&pt3_loader,
	&tcb_loader,
	&dt_loader,
	&gtk_loader,
	&dtt_loader,
	&mgt_loader,
	&arch_loader,
	&sym_loader,
	&digi_loader,
	&dbm_loader,
	&emod_loader,
	&okt_loader,
	&sfx_loader,
	&far_loader,
	&umx_loader,
	&stim_loader,
	&coco_loader,
	&mtp_loader,
	&ims_loader,
	&ssn_loader,
	&fnk_loader,
	&amd_loader,
	&rad_loader,
	&hsc_loader,
	&mfp_loader,
	&alm_loader,
	&polly_loader,
	&stc_loader,
	&pw_loader,
	NULL
};

void xmp_init_formats()
{
	pw_init();
}
