#ifndef LIBXMP_PLAYER_H
#define LIBXMP_PLAYER_H

#include "lfo.h"
#include "envelope.h"

/* Quirk control */
#define HAS_QUIRK(x)	(m->quirk & (x))

/* Channel flag control */
#define SET(f)		SET_FLAG(xc->flags,(f))
#define RESET(f) 	RESET_FLAG(xc->flags,(f))
#define TEST(f)		TEST_FLAG(xc->flags,(f))

/* Persistent effect flag control */
#define SET_PER(f)	SET_FLAG(xc->per_flags,(f))
#define RESET_PER(f)	RESET_FLAG(xc->per_flags,(f))
#define TEST_PER(f)	TEST_FLAG(xc->per_flags,(f))

/* Note flag control */
#define SET_NOTE(f)	SET_FLAG(xc->note_flags,(f))
#define RESET_NOTE(f)	RESET_FLAG(xc->note_flags,(f))
#define TEST_NOTE(f)	TEST_FLAG(xc->note_flags,(f))

struct retrig_control {
	int s;
	int m;
	int d;
};

/* The following macros are used to set the flags for each channel */
#define VOL_SLIDE	(1 << 0)
#define PAN_SLIDE	(1 << 1)
#define TONEPORTA	(1 << 2)
#define PITCHBEND	(1 << 3)
#define VIBRATO		(1 << 4)
#define TREMOLO		(1 << 5)
#define FINE_VOLS	(1 << 6)
#define FINE_BEND	(1 << 7)
#define OFFSET		(1 << 8)
#define TRK_VSLIDE	(1 << 9)
#define TRK_FVSLIDE	(1 << 10)
#define NEW_INS		(1 << 11)
#define NEW_VOL		(1 << 12)
#define VOL_SLIDE_2	(1 << 13)
#define NOTE_SLIDE	(1 << 14)
#define FINE_NSLIDE	(1 << 15)
#define NEW_NOTE	(1 << 16)
#define FINE_TPORTA	(1 << 17)
#define RETRIG		(1 << 18)
#define PANBRELLO	(1 << 19)
#define GVOL_SLIDE	(1 << 20)
#define TEMPO_SLIDE	(1 << 21)

#define NOTE_FADEOUT	(1 << 0)
#define NOTE_RELEASE	(1 << 1)
#define NOTE_END	(1 << 2)

#define IS_VALID_INSTRUMENT(x) ((uint32)(x) < mod->ins && mod->xxi[(x)].nsm > 0)
#define IS_VALID_INSTRUMENT_OR_SFX(x) (((uint32)(x) < mod->ins && mod->xxi[(x)].nsm > 0) || (smix->ins > 0 && (uint32)(x) < mod->ins + smix->ins))

struct instrument_vibrato {
	int phase;
	int sweep;
};

struct channel_data {
	int flags;		/* Channel flags */
	int per_flags;		/* Persistent effect channel flags */
	int note_flags;		/* Note release, fadeout or end */
	int note;		/* Note number */
	int key;		/* Key number */
	double period;		/* Amiga or linear period */
	double per_adj;		/* MED period/pitch adjustment factor hack */
	int finetune;		/* Guess what */
	int ins;		/* Instrument number */
	int old_ins;		/* Last instruemnt */
	int old_insvol;		/* Last instrument that did set a note */
	int smp;		/* Sample number */
	int masterpan;		/* Master pan -- for S3M set pan effect */
	int mastervol;		/* Master vol -- for IT track vol effect */
	int delay;		/* Note delay in frames */
	int keyoff;		/* Key off counter */
	int fadeout;		/* Current fadeout (release) value */
	int gliss;		/* Glissando active */
	int volume;		/* Current volume */
	int gvl;		/* Global volume for instrument for IT */
	int offset;		/* Sample offset memory */
	int offset_val;		/* Sample offset */

	uint16 v_idx;		/* Volume envelope index */
	uint16 p_idx;		/* Pan envelope index */
	uint16 f_idx;		/* Freq envelope index */

	struct lfo vibrato;
	struct lfo tremolo;
	struct lfo panbrello;

	struct {
        	int8 val[16];	/* 16 for Smaksak MegaArps */
		int size;
		int count;
		int memory;
	} arpeggio;

	struct {
		struct lfo lfo;
		int sweep;
	} insvib;

	struct {
		int val;	/* Retrig value */
		int count;	/* Retrig counter */
		int type;	/* Retrig type */
	} retrig;

	struct {
		int val;	/* Tremor value */
		int count;	/* Tremor counter */
		int memory;	/* Tremor memory */
	} tremor;

	struct {
		int slide;	/* Volume slide value */
		int fslide;	/* Fine volume slide value */
		int slide2;	/* Volume slide value */
		int memory;	/* Volume slide effect memory */
	} vol;

	struct {
		int slide;	/* Global volume slide value */
		int fslide;	/* Fine global volume slide value */
		int memory;	/* Global volume memory is saved per channel */
	} gvol;

	struct {
		int slide;	/* Track volume slide value */
		int fslide;	/* Track fine volume slide value */
		int memory;	/* Track volume slide effect memory */
	} trackvol;

	struct {
		int slide;	/* Frequency slide value */
		double fslide;	/* Fine frequency slide value */
		int memory;	/* Portamento effect memory */
	} freq;

	struct {
		double target;	/* Target period for tone portamento */
		int dir;	/* Tone portamento up/down directionh */
		int slide;	/* Delta for tone portamento */
		int memory;	/* Tone portamento effect memory */
	} porta;

	struct {
		int val;	/* Current pan value */
		int slide;	/* Pan slide value */
		int fslide;	/* Pan fine slide value */
		int memory;	/* Pan slide effect memory */
	} pan;	

#ifndef LIBXMP_CORE_PLAYER
	struct {
		int slide;	/* PTM note slide amount */
		int fslide;	/* OKT fine note slide amount */
		int speed;	/* PTM note slide speed */
		int count;	/* PTM note slide counter */
	} noteslide;
#endif

	struct {
		int slide;	/* IT tempo slide */
	} tempo;

	struct {
		int speed;
		int count;
		int pos;
	} invloop;

	struct {
		int cutoff;	/* IT filter cutoff frequency */
		int resonance;	/* IT filter resonance */
	} filter;

#ifndef LIBXMP_CORE_PLAYER
	void *extra;
#endif

	struct xmp_event delayed_event;
	int delayed_ins;	/* IT save instrument emulation */

	int info_period;	/* Period */
	int info_pitchbend;	/* Linear pitchbend */
	int info_position;	/* Position before mixing */
	int info_finalvol;	/* Final volume including envelopes */
	int info_finalpan;	/* Final pan including envelopes */
};


void process_fx(struct context_data *, struct channel_data *, int, uint8, uint8, uint8, int);
void filter_setup(int, int, int, int*, int*, int *);
int read_event(struct context_data *, struct xmp_event *, int);

#endif /* LIBXMP_PLAYER_H */
