#ifndef LIBXMP_CORE_PLAYER
/*
 * Based on Antti S. Lankila's reference code, modified for libxmp
 * by Claudio Matsuoka.
 */
#include "common.h"
#include "virtual.h"
#include "mixer.h"
#include "paula.h"
#include "precomp_blep.h"

#define UPDATE_POS(x) do { \
    frac += (x); \
    pos += frac >> SMIX_SHIFT; \
    frac &= SMIX_MASK; \
} while (0)

void paula_init(struct context_data *ctx, struct paula_data *paula)
{
	struct mixer_data *s = &ctx->s;

	paula->global_output_level = 0;
	paula->active_bleps = 0;
	paula->remainder = (double)PAULA_HZ / s->freq;
}

/* return output simulated as series of bleps */
static int16 output_sample(struct paula_data *paula, int tabnum)
{
	int i;
	int32 output;

	output = paula->global_output_level << BLEP_SCALE;
	for (i = 0; i < paula->active_bleps; i++) {
		int age = paula->blepstate[i].age;
		int level = paula->blepstate[i].level;
		output -= winsinc_integral[tabnum][age] * level;
	}
	output >>= BLEP_SCALE;

	if (output < -32768)
		output = -32768;
	else if (output > 32767)
		output = 32767;

	return output;
}

static void input_sample(struct paula_data *paula, int16 sample)
{
	if (sample != paula->global_output_level) {
		/* Start a new blep: level is the difference, age (or phase) is 0 clocks. */
		if (paula->active_bleps > MAX_BLEPS - 1) {
			fprintf(stderr, "warning: active blep list truncated!\n");
			paula->active_bleps = MAX_BLEPS - 1;
		}

		/* Make room for new blep */
		memmove(&paula->blepstate[1], &paula->blepstate[0],
			sizeof(struct blep_state) * paula->active_bleps);

		/* Update state to account for the new blep */
		paula->active_bleps++;
		paula->blepstate[0].age = 0;
		paula->blepstate[0].level = sample - paula->global_output_level;
		paula->global_output_level = sample;
	}
}

static void clock(struct paula_data *paula, unsigned int cycles)
{
	int i;

	if (cycles <= 0) {
		return;
	}

	for (i = 0; i < paula->active_bleps; i++) {
		paula->blepstate[i].age += cycles;
		if (paula->blepstate[i].age >= BLEP_SIZE) {
			paula->active_bleps = i;
			break;
		}
	}
}

void smix_mono_a500(struct mixer_data *s, struct mixer_voice *vi, int *buffer,
		int count, int vl, int vr, int step, int led)
{
	int num_in, smp_in, ministep;
	int8 *sptr = vi->sptr;
	unsigned int pos = vi->pos;
	double cinc = (double)PAULA_HZ / s->freq;
	int frac = vi->frac;
	int i;

	while (count--) {
		num_in = s->paula.remainder / MINIMUM_INTERVAL;
		ministep = step / num_in;	

		/* input is always sampled at a higher rate than output */
		for (i = 0; i < num_in - 1; i++) {
			input_sample(&s->paula, sptr[pos]);
			clock(&s->paula, MINIMUM_INTERVAL);
			UPDATE_POS(ministep);
		}
		input_sample(&s->paula, sptr[pos]);
		s->paula.remainder -= num_in * MINIMUM_INTERVAL;

		clock(&s->paula, (int)s->paula.remainder);
		smp_in = output_sample(&s->paula, led ? 0 : 1);
		clock(&s->paula, MINIMUM_INTERVAL - (int)s->paula.remainder);

		s->paula.remainder += cinc;
		*(buffer++) += smp_in * vl;

		UPDATE_POS(step - (num_in - 1) * ministep);
	}
}

#endif /* !LIBXMP_CORE_PLAYER */
