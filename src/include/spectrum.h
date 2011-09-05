#ifndef __SPECTRUM_H
#define __SPECTRUM_H

#include <synth.h>

#define SPECTRUM_MAX_STICK	48 
#define SPECTRUM_MAX_OTICK	80
#define SPECTRUM_NUM_ORNAMENTS	15

struct spectrum_ornament {
	int length;
	int loop;
	int8 val[SPECTRUM_MAX_OTICK];
};

struct spectrum_extra {
	struct spectrum_ornament ornament[SPECTRUM_NUM_ORNAMENTS];
};

struct spectrum_stick {
	int16 tone_inc;
	int8 vol;
	int8 noise_env_inc;
	int flags;
#define SPECTRUM_FLAG_TONEINC	0x0001
#define SPECTRUM_FLAG_VOLSLIDE	0x0002
#define SPECTRUM_FLAG_VSLIDE_UP	0x0004
#define SPECTRUM_FLAG_ENVELOPE	0x0008
#define SPECTRUM_FLAG_ENV_NOISE	0x0010
#define SPECTRUM_FLAG_MIXTONE	0x0020
#define SPECTRUM_FLAG_MIXNOISE	0x0040
};

struct spectrum_sample {
	int loop;
	int length;
	struct spectrum_stick stick[SPECTRUM_MAX_STICK];
};

#endif
