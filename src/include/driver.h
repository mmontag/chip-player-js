
#ifndef __XMP_DRIVER_H
#define __XMP_DRIVER_H

#include "common.h"

#define XMP_ACT_CUT		XXM_NNA_CUT
#define XMP_ACT_CONT		XXM_NNA_CONT
#define XMP_ACT_OFF		XXM_NNA_OFF
#define XMP_ACT_FADE		XXM_NNA_FADE

#define XMP_CHN_ACTIVE		0x100
#define XMP_CHN_DUMB		-1

#define parm_init() for (parm = o->parm; *parm; parm++) { \
	char s[80]; strncpy(s, *parm, 80); \
	token = strtok(s, ":="); token = strtok(NULL, "");
#define parm_end() }
#define parm_error() do { \
	fprintf(stderr, "xmp: incorrect parameters in -D %s\n", s); \
	exit(-4); } while (0)
#define chkparm0(x,y) { \
	if (!strcmp(s, x)) { \
	    if (token != NULL) parm_error(); else { y; } } }
#define chkparm1(x,y) { \
	if (!strcmp(s, x)) { \
	    if (token == NULL) parm_error(); else { y; } } }
#define chkparm2(x,y,z,w) { if (!strcmp(s, x)) { \
	if (2 > sscanf(token, y, z, w)) parm_error(); } }


/* PROTOTYPES */

int	xmp_drv_on		(struct xmp_context *, int);
void	xmp_drv_off		(struct xmp_context *);
void	xmp_drv_mute		(struct xmp_context *, int, int);
int	xmp_drv_setpatch	(struct xmp_context *, int, int, int, int, int, int, int, int, int);
int	xmp_drv_cvt8bit		(void);
void	xmp_drv_setsmp		(struct xmp_context *, int, int);
void	xmp_drv_setnna		(struct xmp_context *, int, int);
void	xmp_drv_pastnote	(struct xmp_context *, int, int);
void	xmp_drv_setvol		(struct xmp_context *, int, int);
void	xmp_drv_voicepos	(struct xmp_context *, int, int);
void	xmp_drv_setbend		(struct xmp_context *, int, int);
void	xmp_drv_setpan		(struct xmp_context *, int, int);
void	xmp_drv_seteffect	(struct xmp_context *, int, int, int);
int	xmp_drv_cstat		(struct xmp_context *, int);
void	xmp_drv_resetchannel	(struct xmp_context *, int);
void	xmp_drv_resetvoice	(struct xmp_context *, int, int);
void	xmp_drv_reset		(struct xmp_context *);

struct xmp_drv_info *xmp_drv_array (void);

#endif /* __XMP_DRIVER_H */
