#ifndef __SPECTRUM_H
#define __SPECTRUM_H

#define SPECTRUM_MAX_STICK 32

struct spectrum_stick {
	int tone;
	int tone_inc;
	int vol;
	int env_inc;
	int flags;
#define SPECTRUM_FLAG_VOLSLIDE	0x0001
#define SPECTRUM_FLAG_ENVELOPE	0x0004
#define SPECTRUM_FLAG_MIXTONE	0x0010
#define SPECTRUM_FLAG_MIXNOISE	0x0020
};

struct spectrum_sample {
	int loop;
	int length;
	struct spectrum_stick stick[SPECTRUM_MAX_STICK];
};


#define SPECTRUM_CLOCK		1773400

#endif
