#ifndef XMP_MED_EXTRAS_H
#define XMP_MED_EXTRAS_H

struct med_extras {
	int vts;		/* Volume table speed */
        int wts;		/* Waveform table speed */
};

#define MED_EXTRA(x) ((struct med_extras *)(x).extra)

#endif
