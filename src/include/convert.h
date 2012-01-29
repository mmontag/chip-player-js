
#ifndef __XMP_CONVERT_H
#define __XMP_CONVERT_H

#include "driver.h"

void xmp_cvt_hsc2sbi (char *);
void xmp_cvt_diff2abs (int, int, uint8 *);
void xmp_cvt_stdownmix (int, int, uint8 *);
void xmp_cvt_sig2uns (int, int, char *);
void xmp_cvt_sex (int, uint8 *);
void xmp_cvt_2xsmp (int, uint8 *);
void xmp_cvt_vidc (int, uint8 *);

void xmp_cvt_anticlick (struct xxm_sample *);

#endif /* __XMP_CONVERT_H */
