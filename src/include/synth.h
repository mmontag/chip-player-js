#ifndef __XMP_SYNTH_H
#define __XMP_SYNTH_H

#include "common.h"

struct xmp_synth_info {
	int (*init)(struct xmp_context *, int);
	int (*deinit)(struct xmp_context *);
	int (*reset)(struct xmp_context *);
	void (*setpatch)(struct xmp_context *, int, uint8 *);
	void (*setnote)(struct xmp_context *, int, int, int);
	void (*setvol)(struct xmp_context *, int, int);
	void (*seteffect)(struct xmp_context *, int, int, int);
	void (*mixer)(struct xmp_context *, int *, int, int, int, int);
};

extern struct xmp_synth_info synth_null;
extern struct xmp_synth_info synth_adlib;
extern struct xmp_synth_info synth_spectrum;

#define SYNTH_CHIP(x) ((x)->p.m.chip)

#endif
