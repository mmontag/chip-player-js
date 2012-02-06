
#ifndef __MIXER_H
#define __MIXER_H

/*#define SMIX_C4NOTE	6947*/
#define SMIX_C4NOTE	6864

#define SMIX_NUMVOC	64	/* default number of softmixer voices */
#define SMIX_MAXRATE	48000	/* max sampling rate (Hz) */
#define SMIX_MINBPM	5	/* min BPM */
#define SMIX_RESMAX	(sizeof (int16))	/* max output resolution */

#define SMIX_SHIFT	16
#define SMIX_MASK	0xffff

#define OUT_MAXLEN	(5 * 2 * SMIX_MAXRATE * SMIX_RESMAX / SMIX_MINBPM / 3)

#define FILTER_PRECISION (1 << 12)

/* Anticlick ramps */
#define SLOW_ATTACK	64
#define SLOW_RELEASE	8


struct voice_info {
	int chn;		/* channel link */
	int root;		/* */
	unsigned int age;	/* */
	int note;		/* */
	int pan;		/* */
	int vol;		/* */
	int period;		/* current period */
	int pos;		/* position in sample */
	int frac;		/* interpolation */
	int fidx;		/* function index */
	int fxor;		/* function xor control */
	int ins;		/* instrument number */
	int smp;		/* sample number */
	int end;		/* loop end */
	int act;		/* nna info & status of voice */
	int sleft;		/* last left sample output, in 32bit */
	int sright;		/* last right sample output, in 32bit */
	void *sptr;		/* sample pointer */

	struct {
		int X1;		/* filter variables */
		int X2;
		int B0;
		int B1;
		int B2;
		int cutoff;
		int resonance;
	} filter;

	int attack;		/* ramp up anticlick */
	int looped_sample;	/* set if sample has looped */
};

int	mixer_on		(struct xmp_context *);
void	mixer_off		(struct xmp_context *);
void    mixer_setvol		(struct xmp_context *, int, int);
void    mixer_seteffect		(struct xmp_context *, int, int, int);
void    mixer_setpan		(struct xmp_context *, int, int);
int	mixer_numvoices		(struct xmp_context *, int);
void	mixer_softmixer		(struct xmp_context *);
void	mixer_reset		(struct xmp_context *);
void	mixer_setpatch		(struct xmp_context *, int, int);
void	mixer_voicepos		(struct xmp_context *, int, int, int);
void	mixer_setnote		(struct xmp_context *, int, int);
void	mixer_setbend		(struct xmp_context *, int, int);

#endif /* __MIXER_H */
