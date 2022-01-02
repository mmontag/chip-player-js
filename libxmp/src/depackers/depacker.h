#ifndef LIBXMP_DEPACKER_H
#define LIBXMP_DEPACKER_H

#include "../common.h"
#include "../hio.h"

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

struct depacker {
	int (*test)(unsigned char *);
	int (*depack)(HIO_HANDLE *, FILE *, long);
	int (*depack_mem)(HIO_HANDLE *, void **, long, long *);
};

int	libxmp_decrunch		(HIO_HANDLE **h, const char *filename, char **temp);
int	libxmp_exclude_match	(const char *);

#endif /* LIBXMP_DEPACKER_H */
