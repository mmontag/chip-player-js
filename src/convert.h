#ifndef __XMP_CONVERT_H
#define __XMP_CONVERT_H

#include "common.h"

void convert_hsc_to_sbi		(char *);
void convert_delta		(uint8*, int, int);
void convert_stereo_to_mono	(uint8*, int, int);
void convert_signal		(uint8*, int, int);
void convert_endian		(uint8*, int);
void convert_7bit_to_8bit	(uint8*, int);
void convert_vidc_to_linear	(uint8*, int);

#endif /* __XMP_CONVERT_H */
