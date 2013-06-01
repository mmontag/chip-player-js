#ifndef LIBXMP_MED_EXTRAS_H
#define LIBXMP_MED_EXTRAS_H

#define MED_EXTRAS_MAGIC 0x7f20ca5

struct med_extras {
	uint32 magic;
	int vts;		/* Volume table speed */
        int wts;		/* Waveform table speed */
};

#define MED_EXTRA(x) ((struct med_extras *)(x).extra)

#endif
