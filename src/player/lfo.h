#ifndef __XMP_LFO_H
#define __XMP_LFO_H

struct lfo {
	int type;
	int rate;
	int depth;
	int phase;
};


int get_lfo(struct lfo *);
void update_lfo(struct lfo *);
void set_lfo_phase(struct lfo *, int);
void set_lfo_notzero(struct lfo *, int, int);
void set_lfo_waveform(struct lfo *, int);

#endif
