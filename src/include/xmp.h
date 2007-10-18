/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: xmp.h,v 1.21 2007-10-18 23:07:10 cmatsuoka Exp $
 */

#ifndef __XMP_H
#define __XMP_H

#define XMP_NAMESIZE		64

#define XMP_OK			0

#define XMP_KEY_OFF		0x61
#define XMP_KEY_CUT		0x62
#define XMP_KEY_FADE		0x63

/* DSP effects */
#define XMP_FX_CHORUS		0x00
#define XMP_FX_REVERB		0x01
#define XMP_FX_CUTOFF		0x02
#define XMP_FX_RESONANCE	0x03
#define XMP_FX_FILTER_B0	0xb0
#define XMP_FX_FILTER_B1	0xb1
#define XMP_FX_FILTER_B2	0xb2

/* Event echo messages */
#define XMP_ECHO_NONE		0x00
#define XMP_ECHO_END		0x01
#define XMP_ECHO_BPM		0x02
#define XMP_ECHO_VOL		0x03
#define XMP_ECHO_INS		0x04
#define XMP_ECHO_ORD		0x05
#define XMP_ECHO_ROW		0x06
#define XMP_ECHO_CHN		0x07
#define XMP_ECHO_PBD		0x08
#define XMP_ECHO_GVL		0x09
#define XMP_ECHO_NCH		0x0a
#define XMP_ECHO_FRM		0x0b

/* xmp_player_ctl arguments */
#define XMP_ORD_NEXT		0x00
#define XMP_ORD_PREV		0x01
#define XMP_ORD_SET		0x02
#define XMP_MOD_STOP		0x03
#define XMP_MOD_RESTART		0x04
#define XMP_MOD_PAUSE		0x05
#define XMP_GVOL_INC		0x06
#define XMP_GVOL_DEC		0x07
#define XMP_TIMER_STOP          0x08
#define XMP_TIMER_RESTART       0x09

/* Fetch control */
#define XMP_MODE_ST3		(XMP_CTL_NCWINS | XMP_CTL_IGNWINS | \
				 XMP_CTL_S3MLOOP | XMP_CTL_RTGINS)
#define XMP_MODE_FT2		(XMP_CTL_OINSMOD | XMP_CTL_CUTNWI | \
				 XMP_CTL_OFSRST)
#define XMP_MODE_IT		(XMP_CTL_NCWINS | XMP_CTL_INSPRI | \
				 XMP_CTL_ENVFADE | XMP_CTL_S3MLOOP | \
				 XMP_CTL_OFSRST | XMP_CTL_ITENV)

/* Player control macros */
#define xmp_ord_next(p)		xmp_player_ctl((p), XMP_ORD_NEXT, 0)
#define xmp_ord_prev(p)		xmp_player_ctl((p), XMP_ORD_PREV, 0)
#define xmp_ord_set(p,x)	xmp_player_ctl((p), XMP_ORD_SET, x)
#define xmp_mod_stop(p)		xmp_player_ctl((p), XMP_MOD_STOP, 0)
#define xmp_stop_module(p)	xmp_player_ctl((p), XMP_MOD_STOP, 0)
#define xmp_mod_restart(p)	xmp_player_ctl((p), XMP_MOD_RESTART, 0)
#define xmp_restart_module(p)	xmp_player_ctl((p), XMP_MOD_RESTART, 0)
#define xmp_mod_pause(p)	xmp_player_ctl((p), XMP_MOD_PAUSE, 0)
#define xmp_pause_module(p)	xmp_player_ctl((p), XMP_MOD_PAUSE, 0)
#define xmp_timer_stop(p)	xmp_player_ctl((p), XMP_TIMER_STOP, 0)
#define xmp_timer_restart(p)	xmp_player_ctl((p), XMP_TIMER_RESTART, 0)
#define xmp_gvol_inc(p)		xmp_player_ctl((p), XMP_GVOL_INC, 0)
#define xmp_gvol_dec(p)		xmp_player_ctl((p), XMP_GVOL_DEC, 0)
#define xmp_mod_load		xmp_load_module
#define xmp_mod_test		xmp_test_module
#define xmp_mod_play		xmp_play_module

/* Error messages */
#define XMP_ERR_NOCTL		-1
#define XMP_ERR_NODRV		-2
#define XMP_ERR_DSPEC		-3
#define XMP_ERR_DOPEN		-4
#define XMP_ERR_DINIT		-5
#define XMP_ERR_PATCH		-6
#define XMP_ERR_VIRTC		-7
#define XMP_ERR_ALLOC		-8


struct xmp_control {
    char *drv_id;	/* Driver ID */
    char *description;	/* Driver description */
    char **help;	/* Driver help info */
    char *outfile;	/* Output file name when mixing to file */

    int memavl;		/* Memory availble in the sound card */
    int verbose;	/* Verbosity level */
#define XMP_FMT_FM	0x00000001	/* Active mode FM */
#define XMP_FMT_UNS	0x00000002	/* Unsigned samples */
#define XMP_FMT_MONO	0x00000004	/* Mono output */
    int outfmt;		/* Software mixing output data format */
    int resol;		/* Software mixing resolution output */
    int freq;		/* Software mixing rate (Hz) */
			/* Global mode control */
#define XMP_CTL_ITPT	0x00000001	/* Mixer interpolation */
#define XMP_CTL_REVERSE	0x00000002	/* Reverse stereo */
#define XMP_CTL_8BIT	0x00000004	/* Convert 16 bit samples to 8 bit */
#define XMP_CTL_LOOP	0x00000010	/* Enable module looping */
#define XMP_CTL_VIRTUAL	0x00000040	/* Enable virtual channels */
#define XMP_CTL_DYNPAN	0x00000080	/* Enable dynamic pan */
			/* Fetch control */
#define XMP_CTL_MEDBPM	0x00000100	/* Enable MED BPM timing */
#define XMP_CTL_S3MLOOP 0x00000200      /* Active s3m loop mode */
#define XMP_CTL_ENVFADE	0x00000400	/* Fade at end of envelope */
#define XMP_CTL_ITENV	0x00000800	/* IT envelope mode */
#define XMP_CTL_IGNWINS	0x00001000	/* Ignore invalid instrument */
#define XMP_CTL_NCWINS	0x00002000	/* Don't cut invalid instrument */
#define XMP_CTL_INSPRI	0x00004000	/* Reset note for every new != ins */
#define XMP_CTL_CUTNWI	0x00008000	/* Cut only when note + invalid ins */
#define XMP_CTL_OINSMOD	0x00010000	/* XM old instrument mode */
#define XMP_CTL_OFSRST	0x00020000	/* Always reset sample offset */
#define XMP_CTL_FX9BUG	0x00040000	/* Protracker effect 9 bug emulation */ 
#define XMP_CTL_ST3GVOL	0x00080000	/* ST 3 weird global volume effect */
#define XMP_CTL_FINEFX	0x00100000	/* Enable 0xf/0xe for fine effects */
#define XMP_CTL_VSALL	0x00200000	/* Volume slides in all frames */
#define XMP_CTL_FIXLOOP	0x00400000	/* Fix sample loop start */
#define XMP_CTL_RTGINS	0x00800000	/* Retrig instrument on toneporta */
#define XMP_CTL_FILTER	0x01000000	/* IT lowpass filter */
#define XMP_CTL_PBALL	0x02000000	/* Pitch bending in all frames */
    int flags;		/* xmp internal control flags, set default mode */
    int numtrk;		/* Number of tracks */
    int numchn;		/* Number of virtual channels needed by the module */
    int numvoc;		/* Number of voices currently in use */
    int numbuf;		/* Number of output buffers */
    int maxvoc;		/* Channel max number of voice */
    int crunch;		/* Sample crunching ratio */
    int pause;		/* Player pause */
    int start;		/* Set initial order (default = 0) */
    int mix;		/* Percentage of L/R channel separation */
    int time;		/* Maximum playing time in seconds */
    int tempo;		/* Set initial tempo */
    int chorus;		/* Chorus level */
    int reverb;		/* Reverb leval */
    char *parm[64];	/* Driver parameter data */
};

struct xmp_fmt_info {
    char *suffix;
    char *tracker;
    struct xmp_fmt_info *next;
};

struct xmp_drv_info {
    char *id;
    char *description;
    char **help;
    int (*init)();
    void (*shutdown)();
    int (*numvoices)();
    void (*voicepos)();
    void (*echoback)();
    void (*setpatch)();
    void (*setvol)();
    void (*setnote)();
    void (*setpan)();
    void (*setbend)();
    void (*seteffect)();
    void (*starttimer)();
    void (*stoptimer)();
    void (*reset)();
    void (*bufdump)();
    void (*bufwipe)();
    void (*clearmem)();
    void (*sync)();
    int (*writepatch)();
    int (*getmsg)();
    struct xmp_drv_info *next;
};

struct xmp_module_info {
    char name[0x40];
    char type[0x40];
    int chn;
    int pat;
    int ins;
    int trk;
    int smp;
    int len;
    int bpm;
    int tpo;
};

typedef char *xmp_context;

extern char *xmp_version;
extern char *xmp_date;
extern char *xmp_build;
extern char *global_filename;	/* FIXME: hack for the wav driver */

void *xmp_create_context(void);
void xmp_free_context(xmp_context);

void	xmp_init			(int, char **, struct xmp_control *);
int	xmp_load_module			(xmp_context, char *);
int	xmp_test_module			(char *, char *);
struct xmp_module_info*
	xmp_get_module_info		(xmp_context, struct xmp_module_info *);
struct xmp_fmt_info*
	xmp_get_fmt_info		(struct xmp_fmt_info **);
struct xmp_drv_info*
	xmp_get_drv_info		(struct xmp_drv_info **);
char*	xmp_get_driver_description 	(void);
void	xmp_set_driver_parameter 	(struct xmp_control *, char *);
void	xmp_get_driver_cfg		(int *, int *, int *, int *);
void	xmp_channel_mute		(int, int, int);
void	xmp_display_license		(void);
void	xmp_register_event_callback	(void (*)());
void	xmp_register_driver_callback	(void (*)(void *, int));
void	xmp_init_callback		(struct xmp_control *,
					 void (*)(void *, int));
int	xmp_player_ctl			(xmp_context, int, int);
int	xmp_open_audio			(struct xmp_control *);
void	xmp_close_audio			(void);
int	xmp_play_module			(xmp_context);
void	xmp_release_module		(xmp_context);
int	xmp_tell_parent			(void);
int	xmp_wait_parent			(void);
int	xmp_check_parent		(int);
int	xmp_tell_child			(void);
int	xmp_wait_child			(void);
int	xmp_check_child			(int);
void*	xmp_get_shared_mem		(int);
void	xmp_detach_shared_mem		(void *);
int	xmp_verbosity_level		(int);
int	xmp_seek_time			(xmp_context, int);
void	xmp_init_formats		(void);

#endif /* __XMP_H */
