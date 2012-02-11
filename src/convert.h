#ifndef __XMP_CONVERT_H
#define __XMP_CONVERT_H

#include "common.h"

void convert_hsc_to_sbi		(char *);
void convert_delta		(int, int, uint8 *);
void convert_stereo_to_mono	(int, int, uint8 *);
void convert_signal		(int, int, uint8 *);
void convert_endian		(int, uint8 *);
void convert_7bit_to_8bit	(int, uint8 *);
void convert_vidc_to_linear	(int, uint8 *);

#endif /* __XMP_CONVERT_H */
