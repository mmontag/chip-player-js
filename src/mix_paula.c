#ifndef LIBXMP_CORE_PLAYER
/*
 * Based on Antti S. Lankila's reference code, modified for libxmp
 * by Claudio Matsuoka.
 */
#include "common.h"
#include "paula.h"
#include "precomp_blep.h"

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
}

void smix_stereo_a500(struct mixer_data *s, struct mixer_voice *vi, int *buffer,
		int count, int vl, int vr, int step, int led)
{
}

#endif /* !LIBXMP_CORE_PLAYER */
