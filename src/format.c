/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <stdlib.h>
#include <string.h>
#include "loaders/prowizard/prowiz.h"
#include "format.h"

extern const struct format_loader xm_loader;
extern const struct format_loader mod_loader;
extern const struct format_loader flt_loader;
extern const struct format_loader st_loader;
extern const struct format_loader it_loader;
extern const struct format_loader s3m_loader;
extern const struct format_loader stm_loader;
extern const struct format_loader stx_loader;
extern const struct format_loader mtm_loader;
extern const struct format_loader ice_loader;
extern const struct format_loader imf_loader;
extern const struct format_loader ptm_loader;
extern const struct format_loader mdl_loader;
extern const struct format_loader ult_loader;
extern const struct format_loader liq_loader;
extern const struct format_loader no_loader;
extern const struct format_loader masi_loader;
extern const struct format_loader gal5_loader;
extern const struct format_loader gal4_loader;
extern const struct format_loader psm_loader;
extern const struct format_loader amf_loader;
extern const struct format_loader gdm_loader;
extern const struct format_loader mmd1_loader;
extern const struct format_loader mmd3_loader;
extern const struct format_loader med2_loader;
extern const struct format_loader med3_loader;
extern const struct format_loader med4_loader;
extern const struct format_loader dmf_loader;
extern const struct format_loader rtm_loader;
extern const struct format_loader pt3_loader;
extern const struct format_loader tcb_loader;
extern const struct format_loader dt_loader;
extern const struct format_loader gtk_loader;
extern const struct format_loader dtt_loader;
extern const struct format_loader mgt_loader;
extern const struct format_loader arch_loader;
extern const struct format_loader sym_loader;
extern const struct format_loader digi_loader;
extern const struct format_loader dbm_loader;
extern const struct format_loader emod_loader;
extern const struct format_loader okt_loader;
extern const struct format_loader sfx_loader;
extern const struct format_loader far_loader;
extern const struct format_loader umx_loader;
extern const struct format_loader stim_loader;
extern const struct format_loader coco_loader;
extern const struct format_loader mtp_loader;
extern const struct format_loader ims_loader;
extern const struct format_loader ssn_loader;
extern const struct format_loader fnk_loader;
extern const struct format_loader amd_loader;
extern const struct format_loader rad_loader;
extern const struct format_loader hsc_loader;
extern const struct format_loader mfp_loader;
extern const struct format_loader alm_loader;
extern const struct format_loader polly_loader;
extern const struct format_loader stc_loader;
extern const struct format_loader pw_loader;

extern const struct pw_format *const pw_format[];

const struct format_loader *const format_loader[] = {
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

static const char **farray = NULL;

int format_init()
{
	int i;

	if (farray != NULL)
		return 0;

	for (i = 0; format_loader[i] != NULL; i++);
	for (; pw_format[i] != NULL; i++);

	farray = malloc(i * sizeof(char *));
	if (farray == NULL)
		return -1;

	for (i = 0; format_loader[i] != NULL; i++) {
		if (strcmp(format_loader[i]->name, "prowizard") == 0) {
			for (; pw_format[i] != NULL; i++) {
				farray[i] = pw_format[i]->name;
			}
		} else {
			farray[i] = format_loader[i]->name;
		}
	}
	farray[i] = NULL;
	
	return 0;
}

void format_deinit()
{
	free(farray);
	farray = NULL;
}

void format_list(char ***array)
{
	*array = (char **)farray;
}
