/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifndef __XMP_H
#define __XMP_H

#ifdef __cplusplus
extern "C" {
#endif

#define XMP_NAMESIZE		64

#define XMP_KEY_OFF		0x81
#define XMP_KEY_CUT		0x82
#define XMP_KEY_FADE		0x83

/* xmp_player_ctl arguments */
#define XMP_ORD_NEXT		0x00
#define XMP_ORD_PREV		0x01
#define XMP_ORD_SET		0x02
#define XMP_MOD_STOP		0x03
#define XMP_MOD_RESTART		0x04
#define XMP_GVOL_INC		0x05
#define XMP_GVOL_DEC		0x06

/* Player control macros */
#define xmp_ord_next(p)		xmp_player_ctl((p), XMP_ORD_NEXT, 0)
#define xmp_ord_prev(p)		xmp_player_ctl((p), XMP_ORD_PREV, 0)
#define xmp_ord_set(p,x)	xmp_player_ctl((p), XMP_ORD_SET, (x))
#define xmp_mod_stop(p)		xmp_player_ctl((p), XMP_MOD_STOP, 0)
#define xmp_stop_module(p)	xmp_player_ctl((p), XMP_MOD_STOP, 0)
#define xmp_mod_restart(p)	xmp_player_ctl((p), XMP_MOD_RESTART, 0)
#define xmp_restart_module(p)	xmp_player_ctl((p), XMP_MOD_RESTART, 0)
#define xmp_gvol_inc(p)		xmp_player_ctl((p), XMP_GVOL_INC, 0)
#define xmp_gvol_dec(p)		xmp_player_ctl((p), XMP_GVOL_DEC, 0)


struct xmp_options {
	int big_endian;		/* Machine byte order */
	char *drv_id;		/* Driver ID */
	char *outfile;		/* Output file name when mixing to file */
	int dump;		/* Data dump for testing */
	int amplify;		/* Software mixing amplify volume:
				   0 = none, 1 = x2, 2 = x4, 3 = x8 */
#define XMP_FMT_UNS	(1 << 1)	/* Unsigned samples */
#define XMP_FMT_MONO	(1 << 2)	/* Mono output */
	int outfmt;		/* Software mixing output data format */
	int resol;		/* Software mixing resolution output */
	int freq;		/* Software mixing rate (Hz) */
#define XMP_CTL_ITPT	(1 << 0)	/* Mixer interpolation */
#define XMP_CTL_LOOP	(1 << 1)	/* Enable module looping */
#define XMP_CTL_VBLANK	(1 << 2)	/* Use vblank timing only */
#define XMP_CTL_VIRTUAL	(1 << 3)	/* Enable virtual channels */
#define XMP_CTL_FILTER	(1 << 4)	/* IT lowpass filter */
	int flags;		/* internal control flags, set default mode */
	int start;		/* Set initial order (default = 0) */
	int mix;		/* Percentage of L/R channel separation */
	int time;		/* Maximum playing time in seconds */
	int tempo;		/* Set initial tempo */
	char *ins_path;		/* External instrument path */
	char *parm[16];		/* Driver parameter data */
};

struct xmp_fmt_info {
	struct xmp_fmt_info *next;
	char *id;
	char *tracker;
};

#define XXM_KEY_MAX 108
#define XMP_MAXENV	32		/* max envelope points */
#define XMP_MAXORD	256		/* max number of patterns in module */
#define XMP_MAXCH	64		/* max channels */

struct xmp_channel {
	int pan;
	int vol;
#define XXM_CHANNEL_SYNTH 0x01
#define XXM_CHANNEL_FX    0x02
#define XXM_CHANNEL_MUTE  0x04
	int flg;
};

struct xmp_pattern {
	int rows;		/* Number of rows */
	int index[1];
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
	int rows;		/* Number of rows */
	struct xmp_event event[1];
};


struct xmp_envelope {
#define XXM_ENV_ON	0x01
#define XXM_ENV_SUS	0x02
#define XXM_ENV_LOOP	0x04
#define XXM_ENV_FLT	0x08
#define XXM_ENV_SLOOP	0x10
	int flg;		/* Flags */
	int npt;		/* Number of envelope points */
	int scl;		/* Envelope scaling */
	int sus;		/* Sustain start point */
	int sue;		/* Sustain end point */
	int lps;		/* Loop start point */
	int lpe;		/* Loop end point */
	short data[XMP_MAXENV * 2];
};

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
#define XXM_NNA_CUT	0x00
#define XXM_NNA_CONT	0x01
#define XXM_NNA_OFF	0x02
#define XXM_NNA_FADE	0x03
	int nna;		/* New note action -- for IT */
#define XXM_DCT_OFF	0x00
#define XXM_DCT_NOTE	0x01
#define XXM_DCT_SMP	0x02
#define XXM_DCT_INST	0x03
	int dct;		/* Duplicate check type -- for IT */
#define XXM_DCA_CUT	XXM_NNA_CUT
#define XXM_DCA_OFF	XXM_NNA_OFF
#define XXM_DCA_FADE	XXM_NNA_FADE
	int dca;		/* Duplicate check action -- for IT */
	int ifc;		/* Initial filter cutoff -- for IT */
	int ifr;		/* Initial filter resonance -- for IT */
	int hld;		/* Hold -- for MED */
};

struct xmp_instrument {
	char name[32];		/* Instrument name */
	int vol;		/* Volume */
	int nsm;		/* Number of samples */
	int rls;		/* Release (fadeout) */
	struct xmp_envelope aei;	/* Amplitude envelope info */
	struct xmp_envelope pei;	/* Pan envelope info */
	struct xmp_envelope fei;	/* Frequency envelope info */
	int vts;		/* Volume table speed -- for MED */
	int wts;		/* Waveform table speed -- for MED */

	struct {
		unsigned char ins;	/* Instrument number for each key */
		signed char xpo;	/* Instrument transpose for each key */
	} map[XXM_KEY_MAX];

	struct xmp_subinstrument *sub;
};

struct xmp_sample {
	char name[32];		/* Sample name */
	int len;		/* Sample length */
	int lps;		/* Loop start */
	int lpe;		/* Loop end */
#define XMP_SAMPLE_16BIT	0x0001
#define XMP_SAMPLE_LOOP		0x0002
#define XMP_SAMPLE_LOOP_BIDIR	0x0004	/* Bidirectional loop */
#define XMP_SAMPLE_LOOP_REVERSE	0x0008	/* Backwards loop */
#define XMP_SAMPLE_LOOP_FULL	0x0010	/* Play entire sample before looping */
#define XMP_SAMPLE_LOOP_FIRST	0x0020	/* First run control bit */
#define XMP_SAMPLE_SYNTH	0x1000
	int flg;		/* Flags */
	unsigned char *data;
};



struct xmp_module {
	char name[XMP_NAMESIZE];	/* module name */
	char type[XMP_NAMESIZE];	/* module type */
#define XXM_FLG_LINEAR	0x01
#define XXM_FLG_MODRNG	0x02
#define XXM_FLG_INSVOL  0x04
	int flg;		/* Flags */
	int pat;		/* Number of patterns */
	int trk;		/* Number of tracks */
	int chn;		/* Tracks per pattern */
	int ins;		/* Number of instruments */
	int smp;		/* Number of samples */
	int tpo;		/* Initial tempo */
	int bpm;		/* Initial BPM */
	int len;		/* Module length in patterns */
	int rst;		/* Restart position */
	int gvl;		/* Global volume */

	struct xmp_pattern **xxp;	/* Patterns */
	struct xmp_track **xxt;		/* Tracks */
	struct xmp_instrument *xxi;	/* Instruments */
	struct xmp_sample *xxs;		/* Samples */
	struct xmp_channel xxc[64];	/* Channel info */
	unsigned char xxo[XMP_MAXORD];		/* Orders */
};

/*
 * Playing module information
 */
struct xmp_module_info {
	int order;
	int pattern;
	int row;
	int frame;
	int tempo;
	int bpm;
	int time;
	void *buffer;
	int buffer_size;

	struct {
		int period;
		int position;
		unsigned short flags;
		unsigned char note;
		unsigned char instrument;
		unsigned char sample;
		unsigned char volume;
		unsigned char pan;
	} channel_info[XMP_MAXCH];

	struct xmp_module *mod;
};


typedef char *xmp_context;

extern char *global_filename;	/* FIXME: hack for the wav driver */

void *xmp_create_context(void);
void xmp_free_context(xmp_context);
void xmp_unlink_tempfiles(void);
struct xmp_options *xmp_get_options(xmp_context);

void xmp_init(void);
void xmp_deinit(void);
int xmp_load_module(xmp_context, char *);
int xmp_test_module(xmp_context, char *, char *);
struct xmp_fmt_info *xmp_get_fmt_info(struct xmp_fmt_info **);
void xmp_channel_mute(xmp_context, int, int, int);
int xmp_player_ctl(xmp_context, int, int);
int xmp_player_start(xmp_context);
int xmp_player_frame(xmp_context);
void xmp_player_get_info(xmp_context, struct xmp_module_info *);
void xmp_player_end(xmp_context);
void xmp_release_module(xmp_context);
int xmp_seek_time(xmp_context, int);

#ifdef __cplusplus
}
#endif

#endif	/* __XMP_H */
