#ifndef XMP_DEPACKER_H
#define XMP_DEPACKER_H

#include <stdio.h>

extern struct depacker libxmp_depacker_zip;
extern struct depacker libxmp_depacker_lha;
extern struct depacker libxmp_depacker_gzip;
extern struct depacker libxmp_depacker_bzip2;
extern struct depacker libxmp_depacker_xz;
extern struct depacker libxmp_depacker_compress;
extern struct depacker libxmp_depacker_pp;
extern struct depacker libxmp_depacker_sqsh;
extern struct depacker libxmp_depacker_arc;
extern struct depacker libxmp_depacker_arcfs;
extern struct depacker libxmp_depacker_mmcmp;
extern struct depacker libxmp_depacker_muse;
extern struct depacker libxmp_depacker_lzx;
extern struct depacker libxmp_depacker_s404;
extern struct depacker libxmp_depacker_xfd;
extern struct depacker libxmp_depacker_oxm;

struct depacker {
	int (*const test)(unsigned char *);
	int (*const depack)(FILE *, FILE *);
};

#endif
