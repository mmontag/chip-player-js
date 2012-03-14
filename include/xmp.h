/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/** 
 * @file xmp.h
 * @brief libxmp header file
 */

#ifndef __XMP_H
#define __XMP_H

#ifdef __cplusplus
extern "C" {
#endif

#define XMP_NAME_SIZE		64	/* Size of module name and type */

#define XMP_KEY_OFF		0x81	/* Note number for key off event */
#define XMP_KEY_CUT		0x82	/* Note number for key cut event */
#define XMP_KEY_FADE		0x83	/* Note number for fade event */

/* _xmp_ctl arguments */
#define XMP_CTL_ORD_NEXT	0x00
#define XMP_CTL_ORD_PREV	0x01
#define XMP_CTL_ORD_SET		0x02
#define XMP_CTL_MOD_STOP	0x03
#define XMP_CTL_MOD_RESTART	0x04
#define XMP_CTL_GVOL_INC	0x05
#define XMP_CTL_GVOL_DEC	0x06
#define XMP_CTL_SEEK_TIME	0x07
#define XMP_CTL_CH_MUTE		0x08
#define XMP_CTL_MIXER_AMP	0x09
#define XMP_CTL_MIXER_MIX	0x10
#define XMP_CTL_QUIRK_FX9	0x11
#define XMP_CTL_QUIRK_FXEF	0x12

/* mixer parameter macros */

/* mixer sample format */
#define XMP_FORMAT_8BIT		(1 << 0) /* Mix to 8-bit instead of 16 */
#define XMP_FORMAT_UNSIGNED	(1 << 1) /* Mix to unsigned samples */
#define XMP_FORMAT_MONO		(1 << 2) /* Mix to mono instead of stereo */

#define XMP_MAX_KEYS		121	/* Number of valid keys */
#define XMP_MAX_ENV_POINTS	32	/* Max number of envelope points */
#define XMP_MAX_MOD_LENGTH	256	/* Max number of patterns in module */
#define XMP_MAX_CHANNELS	64	/* Max number of channels in module */

struct xmp_channel {
	int pan;			/* Channel pan (0x80 is center) */
	int vol;			/* Channel volume */
#define XMP_CHANNEL_SYNTH	(1 << 0)  /* Channel is synthesized */
#define XMP_CHANNEL_MUTE  	(1 << 1)  /* Channel is muted */
	int flg;			/* Channel flags */
};

struct xmp_pattern {
	int rows;			/* Number of rows */
	int index[1];			/* Track index */
};

struct xmp_event {
	unsigned char note;		/* Note number (0 means no note) */
	unsigned char ins;		/* Patch number */
	unsigned char vol;		/* Volume (0 to basevol) */
	unsigned char fxt;		/* Effect type */
	unsigned char fxp;		/* Effect parameter */
	unsigned char f2t;		/* Secondary effect type */
	unsigned char f2p;		/* Secondary effect parameter */
	unsigned char flag;
};

struct xmp_track {
	int rows;			/* Number of rows */
	struct xmp_event event[1];	/* Event data */
};

struct xmp_envelope {
#define XMP_ENVELOPE_ON		(1 << 0)  /* Envelope is enabled */
#define XMP_ENVELOPE_SUS	(1 << 1)  /* Envelope has sustain point */
#define XMP_ENVELOPE_LOOP	(1 << 2)  /* Envelope has loop */
#define XMP_ENVELOPE_FLT	(1 << 3)  /* Envelope is used for filter */
#define XMP_ENVELOPE_SLOOP	(1 << 4)  /* Envelope has sustain loop */
	int flg;			/* Flags */
	int npt;			/* Number of envelope points */
	int scl;			/* Envelope scaling */
	int sus;			/* Sustain start point */
	int sue;			/* Sustain end point */
	int lps;			/* Loop start point */
	int lpe;			/* Loop end point */
	short data[XMP_MAX_ENV_POINTS * 2];
};

struct xmp_instrument {
	char name[32];			/* Instrument name */
	int vol;			/* Volume if QUIRK_INSVOL enabled */
	int nsm;			/* Number of samples */
	int rls;			/* Release (fadeout) */
	struct xmp_envelope aei;	/* Amplitude envelope info */
	struct xmp_envelope pei;	/* Pan envelope info */
	struct xmp_envelope fei;	/* Frequency envelope info */
	int vts;			/* Volume table speed -- for MED */
	int wts;			/* Waveform table speed -- for MED */

	struct {
		unsigned char ins;	/* Instrument number for each key */
		signed char xpo;	/* Instrument transpose for each key */
	} map[XMP_MAX_KEYS];

	struct xmp_subinstrument {
		int vol;		/* Default volume */
		int gvl;		/* Global volume */
		int pan;		/* Pan */
		int xpo;		/* Transpose */
		int fin;		/* Finetune */
		int vwf;		/* Vibrato waveform */
		int vde;		/* Vibrato depth */
		int vra;		/* Vibrato rate */
		int vsw;		/* Vibrato sweep */
		int rvv;		/* Random volume var -- for IT */
		int sid;		/* Sample number */
#define XMP_INST_NNA_CUT	0x00
#define XMP_INST_NNA_CONT	0x01
#define XMP_INST_NNA_OFF	0x02
#define XMP_INST_NNA_FADE	0x03
		int nna;		/* New note action -- for IT */
#define XMP_INST_DCT_OFF	0x00
#define XMP_INST_DCT_NOTE	0x01
#define XMP_INST_DCT_SMP	0x02
#define XMP_INST_DCT_INST	0x03
		int dct;		/* Duplicate check type -- for IT */
#define XMP_INST_DCA_CUT	XMP_INST_NNA_CUT
#define XMP_INST_DCA_OFF	XMP_INST_NNA_OFF
#define XMP_INST_DCA_FADE	XMP_INST_NNA_FADE
		int dca;		/* Duplicate check action -- for IT */
		int ifc;		/* Initial filter cutoff -- for IT */
		int ifr;		/* Initial filter resonance -- for IT */
		int hld;		/* Hold -- for MED */
	} *sub;
};

struct xmp_sample {
	char name[32];			/* Sample name */
	int len;			/* Sample length */
	int lps;			/* Loop start */
	int lpe;			/* Loop end */
#define XMP_SAMPLE_16BIT	(1 << 0)  /* 16bit sample */
#define XMP_SAMPLE_LOOP		(1 << 1)  /* Sample is looped */
#define XMP_SAMPLE_LOOP_BIDIR	(1 << 2)  /* Bidirectional sample loop */
#define XMP_SAMPLE_LOOP_REVERSE	(1 << 3)  /* Backwards sample loop */
#define XMP_SAMPLE_LOOP_FULL	(1 << 4)  /* Play full sample before looping */
#define XMP_SAMPLE_SYNTH	(1 << 15) /* Data contains synth patch */
	int flg;			/* Flags */
	unsigned char *data;		/* Sample data */
};

struct xmp_sequence {
	int entry_point;
	int duration;
};

struct xmp_module {
	char name[XMP_NAME_SIZE];	/* Module name */
	char type[XMP_NAME_SIZE];	/* Module type */
	int pat;			/* Number of patterns */
	int trk;			/* Number of tracks */
	int chn;			/* Tracks per pattern */
	int ins;			/* Number of instruments */
	int smp;			/* Number of samples */
	int spd;			/* Initial speed */
	int bpm;			/* Initial BPM */
	int len;			/* Module length in patterns */
	int rst;			/* Restart position */
	int gvl;			/* Global volume */

	struct xmp_pattern **xxp;	/* Patterns */
	struct xmp_track **xxt;		/* Tracks */
	struct xmp_instrument *xxi;	/* Instruments */
	struct xmp_sample *xxs;		/* Samples */
	struct xmp_channel xxc[64];	/* Channel info */
	unsigned char xxo[XMP_MAX_MOD_LENGTH];	/* Orders */
};

struct xmp_test_info {
	char name[XMP_NAME_SIZE];
	char type[XMP_NAME_SIZE];
};

#define XMP_PERIOD_BASE	6847		/* C4 period */

struct xmp_module_info {		/* Current module information */
	int order;			/* Current position */
	int pattern;			/* Current pattern */
	int row;			/* Current row in pattern */
	int num_rows;			/* Number of rows in current pattern */
	int frame;			/* Current frame */
	int speed;			/* Current replay speed */
	int bpm;			/* Current bpm */
	int time;			/* Current module time in ms */
	int frame_time;			/* Frame replay time in us */
	int total_time;			/* Estimated replay time in ms*/
	void *buffer;			/* Pointer to sound buffer */
	int buffer_size;		/* Used buffer size */
	int total_size;			/* Total buffer size */
	int volume;			/* Current master volume */
	int loop_count;			/* Loop counter */
	int virt_channels;		/* Number of virtual channels */
	int virt_used;			/* Used virtual channels */

	struct xmp_channel_info {	/* Current channel information */
		unsigned int period;	/* Sample period */
		unsigned int position;	/* Sample position */
		unsigned short pitchbend; /* Linear bend from base note*/
		unsigned char note;	/* Current base note number */
		unsigned char instrument; /* Current instrument number */
		unsigned char sample;	/* Current sample number */
		unsigned char volume;	/* Current volume */
		unsigned char pan;	/* Current stereo pan */
		unsigned char reserved;	/* Reserved */
		struct xmp_event event;	/* Current track event */
	} channel_info[XMP_MAX_CHANNELS];

	struct xmp_module *mod;		/* Pointer to module data */
	char *comment;			/* Comment text, if any */

	int num_sequences;		/* Number of valid sequences */
	struct xmp_sequence *sequence;	/* Pointer to sequence data */
};


typedef char *xmp_context;

extern const unsigned int xmp_version;

/* Player control macros */

#define xmp_ord_next(p)		_xmp_ctl((p), XMP_CTL_ORD_NEXT)
#define xmp_ord_prev(p)		_xmp_ctl((p), XMP_CTL_ORD_PREV)
#define xmp_ord_set(p,x)	_xmp_ctl((p), XMP_CTL_ORD_SET, (x))
#define xmp_mod_stop(p)		_xmp_ctl((p), XMP_CTL_MOD_STOP)
#define xmp_stop_module(p)	_xmp_ctl((p), XMP_CTL_MOD_STOP)
#define xmp_mod_restart(p)	_xmp_ctl((p), XMP_CTL_MOD_RESTART)
#define xmp_gvol_inc(p)		_xmp_ctl((p), XMP_CTL_GVOL_INC)
#define xmp_gvol_dec(p)		_xmp_ctl((p), XMP_CTL_GVOL_DEC)
#define xmp_seek_time(p,x)	_xmp_ctl((p), XMP_CTL_SEEK_TIME, (x))
#define xmp_channel_mute(p,x,y)	_xmp_ctl((p), XMP_CTL_CH_MUTE, (x), (y))
#define xmp_mixer_amp(p,x)	_xmp_ctl((p), XMP_CTL_MIXER_AMP, (x))
#define xmp_mixer_mix(p,x)	_xmp_ctl((p), XMP_CTL_MIXER_MIX, (x))
#define xmp_quirk_fx9(p,x)	_xmp_ctl((p), XMP_CTL_QUIRK_FX9, (x))
#define xmp_quirk_fxef(p,x)	_xmp_ctl((p), XMP_CTL_QUIRK_FXEF, (x))

xmp_context xmp_create_context  (void);
int         xmp_test_module     (char *, struct xmp_test_info *);
void        xmp_free_context    (xmp_context);
int         xmp_load_module     (xmp_context, char *);
void        xmp_release_module  (xmp_context);
int         _xmp_ctl            (xmp_context, int, ...);
int         xmp_player_start    (xmp_context, int, int);
int         xmp_player_frame    (xmp_context);
void        xmp_player_get_info (xmp_context, struct xmp_module_info *);
void        xmp_player_end      (xmp_context);
void        xmp_inject_event    (xmp_context, int, struct xmp_event *);
char      **xmp_get_format_list (void);

#ifdef __cplusplus
}
#endif

#endif	/* __XMP_H */
