#ifndef __XMP_VIRTUAL_H
#define __XMP_VIRTUAL_H

#include "common.h"

#define VIRTCH_ACTION_CUT	XMP_INST_NNA_CUT
#define VIRTCH_ACTION_CONT	XMP_INST_NNA_CONT
#define VIRTCH_ACTION_OFF	XMP_INST_NNA_OFF
#define VIRTCH_ACTION_FADE	XMP_INST_NNA_FADE

#define VIRTCH_ACTIVE		0x100
#define VIRTCH_INVALID		-1

int	virtch_on		(struct xmp_context *, int);
void	virtch_off		(struct xmp_context *);
void	virtch_mute		(struct xmp_context *, int, int);
int	virtch_setpatch		(struct xmp_context *, int, int, int, int,
				 int, int, int, int, int);
int	virtch_cvt8bit		(void);
void	virtch_setsmp		(struct xmp_context *, int, int);
void	virtch_setnna		(struct xmp_context *, int, int);
void	virtch_pastnote		(struct xmp_context *, int, int);
void	virtch_setvol		(struct xmp_context *, int, int);
void	virtch_voicepos		(struct xmp_context *, int, int);
void	virtch_setbend		(struct xmp_context *, int, int);
void	virtch_setpan		(struct xmp_context *, int, int);
void	virtch_seteffect	(struct xmp_context *, int, int, int);
int	virtch_cstat		(struct xmp_context *, int);
void	virtch_resetchannel	(struct xmp_context *, int);
void	virtch_resetvoice	(struct xmp_context *, int, int);
void	virtch_reset		(struct xmp_context *);

#endif /* __XMP_VIRTUAL_H */
