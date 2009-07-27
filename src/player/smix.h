#ifndef __XMP_SMIX_H
#define __XMP_SMIX_H

struct smix_info {
	char** buffer;		/* array of output buffers */
	int* buf32b;		/* temp buffer for 32 bit samples */
	int numvoc;		/* default softmixer voices number */
	int numbuf;		/* number of buffer to output */
	int mode;		/* mode = 0:OFF, 1:MONO, 2:STEREO */
	int resol;		/* resolution output 1:8bit, 2:16bit */
	int ticksize;
};

void smix_resetvar(struct xmp_context *);
void smix_setpatch(struct xmp_context *, int, int);
void smix_voicepos(struct xmp_context *, int, int, int);
void smix_setnote(struct xmp_context *, int, int);
void smix_setbend(struct xmp_context *, int, int);

#endif
