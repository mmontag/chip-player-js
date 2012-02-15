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

#define XMP_NAME_SIZE		64

#define XMP_KEY_OFF		0x81
#define XMP_KEY_CUT		0x82
#define XMP_KEY_FADE		0x83

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
#define XMP_CTL_GET_FORMATS	0x09

#define XMP_FORMAT_8BIT		(1 << 0)
#define XMP_FORMAT_UNSIGNED	(1 << 1)
#define XMP_FORMAT_MONO		(1 << 2)

#define XMP_MAX_KEYS		121	/**< Number of valid keys */
#define XMP_MAX_ENV_POINTS	32	/**< Max number of envelope points */
#define XMP_MAX_MOD_LENGTH	256	/**< Max number of patterns in module */
#define XMP_MAX_CHANNELS	64	/**< Max number of channels in module */

struct xmp_channel {
	int pan;
	int vol;
#define XMP_CHANNEL_SYNTH	(1 << 0)  /* Channel is synthesized */
#define XMP_CHANNEL_MUTE  	(1 << 1)  /* Channel is muted */
	int flg;
};

struct xmp_pattern {
	int rows;			/* Number of rows */
	int index[1];			/* Track index */
};

struct xmp_event {
	unsigned char note;		/* Note number (0==no note) */
	unsigned char ins;		/* Patch number */
	unsigned char vol;		/* Volume (0 to 64) */
	unsigned char fxt;		/* Effect type */
	unsigned char fxp;		/* Effect parameter */
	unsigned char f2t;		/* Secondary effect type */
	unsigned char f2p;		/* Secondary effect parameter */
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
		int vol;		/* [default] Volume */
		int gvl;		/* [global] Volume */
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



struct xmp_module {
	char name[XMP_NAME_SIZE];	/* module name */
	char type[XMP_NAME_SIZE];	/* module type */
	int pat;			/* Number of patterns */
	int trk;			/* Number of tracks */
	int chn;			/* Tracks per pattern */
	int ins;			/* Number of instruments */
	int smp;			/* Number of samples */
	int tpo;			/* Initial tempo */
	int bpm;			/* Initial BPM */
	int len;			/* Module length in patterns */
	int rst;			/* Restart position */
	int gvl;			/* Global volume */

	struct xmp_pattern **xxp;	/* Patterns */
	struct xmp_track **xxt;		/* Tracks */
	struct xmp_instrument *xxi;	/* Instruments */
	struct xmp_sample *xxs;		/* Samples */
	struct xmp_channel xxc[64];	/* Channel info */
	unsigned char xxo[XMP_MAX_MOD_LENGTH];		/* Orders */
};

/*
 *
 */

struct xmp_test_info {
	char name[XMP_NAME_SIZE];
	char type[XMP_NAME_SIZE];
};

/*
 * Playing module information
 */

#define XMP_PERIOD_BASE	6847		/* C4 period */

struct xmp_module_info {
	int order;			/* Current position */
	int pattern;			/* Current pattern */
	int row;			/* Current row in pattern */
	int num_rows;			/* Number of rows in current pattern */
	int frame;			/* Current frame */
	int tempo;			/* Current replay speed */
	int bpm;			/* Current bpm */
	int total_time;			/* Estimated replay time */
	int current_time;		/* Current replay time */
	void *buffer;			/* Pointer to sound buffer */
	int buffer_size;		/* Used buffer size */
	int total_size;			/* Total buffer size */
	int volume;			/* Current master volume */
	int loop_count;			/* Loop counter */
	int virt_channels;		/* Number of virtual channels */
	int virt_used;			/* Used virtual channels */

	struct xmp_channel_info {
		unsigned int period;
		unsigned int position;
		unsigned short pitchbend;
		unsigned char note;
		unsigned char instrument;
		unsigned char sample;
		unsigned char volume;
		unsigned char pan;
		unsigned char reserved;
	} channel_info[XMP_MAX_CHANNELS];

	struct xmp_module *mod;		/* Pointer to module data */
};


typedef char *xmp_context;

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
#define xmp_get_formats(p,x)	_xmp_ctl((p), XMP_CTL_GET_FORMATS, (x))

xmp_context xmp_create_context(void);
void xmp_free_context(xmp_context);
int xmp_load_module(xmp_context, char *);
int xmp_test_module(xmp_context, char *, struct xmp_test_info *);
void xmp_release_module(xmp_context);
int _xmp_ctl(xmp_context, int, ...);
int xmp_player_start(xmp_context, int, int, int);
int xmp_player_frame(xmp_context);
void xmp_player_get_info(xmp_context, struct xmp_module_info *);
void xmp_player_end(xmp_context);

#ifdef __cplusplus
}
#endif

#endif	/* __XMP_H */
