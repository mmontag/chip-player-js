#ifndef __XMP_VIRTUAL_H
#define __XMP_VIRTUAL_H

#include "common.h"

#define VIRTCH_ACTION_CUT	XMP_INST_NNA_CUT
#define VIRTCH_ACTION_CONT	XMP_INST_NNA_CONT
#define VIRTCH_ACTION_OFF	XMP_INST_NNA_OFF
#define VIRTCH_ACTION_FADE	XMP_INST_NNA_FADE

#define VIRTCH_ACTIVE		0x100
#define VIRTCH_INVALID		-1

int	virtch_on		(struct context_data *, int);
void	virtch_off		(struct context_data *);
void	virtch_mute		(struct context_data *, int, int);
int	virtch_setpatch		(struct context_data *, int, int, int, int,
				 int, int, int, int, int);
int	virtch_cvt8bit		(void);
void	virtch_setsmp		(struct context_data *, int, int);
void	virtch_setnna		(struct context_data *, int, int);
void	virtch_pastnote		(struct context_data *, int, int);
void	virtch_setvol		(struct context_data *, int, int);
void	virtch_voicepos		(struct context_data *, int, int);
void	virtch_setbend		(struct context_data *, int, int);
void	virtch_setpan		(struct context_data *, int, int);
void	virtch_seteffect	(struct context_data *, int, int, int);
int	virtch_cstat		(struct context_data *, int);
void	virtch_resetchannel	(struct context_data *, int);
void	virtch_resetvoice	(struct context_data *, int, int);
void	virtch_reset		(struct context_data *);

int map_virt_channel(struct player_data *, int);

#endif /* __XMP_VIRTUAL_H */
