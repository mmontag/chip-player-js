#ifndef LIBXMP_LFO_H
#define LIBXMP_LFO_H

struct lfo {
	int type;
	int rate;
	int depth;
	int phase;
};


int get_lfo(struct lfo *, int);
void update_lfo(struct lfo *);
void set_lfo_phase(struct lfo *, int);
void set_lfo_depth(struct lfo *, int);
void set_lfo_rate(struct lfo *, int);
void set_lfo_waveform(struct lfo *, int);

#endif
