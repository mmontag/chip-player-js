#ifndef __XMP_SYNTH_H
#define __XMP_SYNTH_H

#include "common.h"
#include "list.h"

struct xmp_synth_info {
	int (*init)(int);
	int (*deinit)(void);
	int (*reset)(void);
	void (*setpatch)(int, uint8 *);
	void (*setnote)(int, int, int);
	void (*setvol)(int, int);
	void (*mixer)(int *, int, int, int, int);
        struct list_head list;
};

extern struct xmp_synth_info synth_null;
extern struct xmp_synth_info synth_adlib;
extern struct xmp_synth_info synth_spectrum;

#endif
