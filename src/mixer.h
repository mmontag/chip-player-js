
#ifndef __MIXER_H
#define __MIXER_H

#define SMIX_C4NOTE	6864

#define SMIX_NUMVOC	64	/* default number of softmixer voices */
#define SMIX_MAXRATE	48000	/* max sampling rate (Hz) */
#define SMIX_MINBPM	20	/* min BPM */

#define SMIX_SHIFT	16
#define SMIX_MASK	0xffff

/* frame rate = (50 * bpm / 125) Hz */
/* frame size = (sampling rate * channels * size) / frame rate */
#define OUT_MAXLEN	(5 * SMIX_MAXRATE * 2 / SMIX_MINBPM)

#define FILTER_PRECISION (1 << 12)

/* Anticlick ramps */
#define SLOW_ATTACK	16
#define SLOW_RELEASE	16


struct mixer_voice {
	int chn;		/* channel number */
	int root;		/* */
	unsigned int age;	/* */
	int note;		/* */
	int pan;		/* */
	int vol;		/* */
	int period;		/* current period */
	int pos;		/* position in sample */
	int pos0;		/* position in sample before mixing */
	int frac;		/* interpolation */
	int fidx;		/* function index */
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
	int sample_loop;	/* set if sample has looped */
};

int	mixer_on		(struct context_data *);
void	mixer_off		(struct context_data *);
void    mixer_setvol		(struct context_data *, int, int);
void    mixer_seteffect		(struct context_data *, int, int, int);
void    mixer_setpan		(struct context_data *, int, int);
int	mixer_numvoices		(struct context_data *, int);
void	mixer_softmixer		(struct context_data *);
void	mixer_reset		(struct context_data *);
void	mixer_setpatch		(struct context_data *, int, int);
void	mixer_voicepos		(struct context_data *, int, int, int);
void	mixer_setnote		(struct context_data *, int, int);
void	mixer_setbend		(struct context_data *, int, int);

#endif /* __MIXER_H */
