
#ifndef __XMP_SYNTH_H
#define __XMP_SYNTH_H

#include "common.h"
#include "list.h"

struct xmp_synth_info {
	char *id;
	int (*init)(int);
	int (*deinit)(void);
	int (*reset)(void);
	void (*setpatch)(int, uint8 *);
	void (*setnote)(int, int, int);
	void (*setvol)(int, int);
	void (*mixer)(int, int, int, int, int);
        struct list_head list;
};

extern struct xmp_synth_info synth_ym3812;

int	synth_init	(int);
int	synth_deinit	(void);
int	synth_reset	(void);
void	synth_setpatch	(int, uint8 *);
void	synth_setnote	(int, int, int);
void	synth_setvol	(int, int);
void	synth_mixer	(int*, int, int, int, int);


#endif
