#ifndef __XMP_SYNTH_H
#define __XMP_SYNTH_H

#include "common.h"

struct synth_info {
	int (*init)(struct context_data *, int);
	int (*deinit)(struct context_data *);
	int (*reset)(struct context_data *);
	void (*setpatch)(struct context_data *, int, uint8 *);
	void (*setnote)(struct context_data *, int, int, int);
	void (*setvol)(struct context_data *, int, int);
	void (*seteffect)(struct context_data *, int, int, int);
	void (*mixer)(struct context_data *, int *, int, int, int, int);
};

extern struct synth_info synth_null;
extern struct synth_info synth_adlib;
extern struct synth_info synth_spectrum;

#define SYNTH_CHIP(x) ((x)->m.chip)

#endif
