#ifndef LIBXMP_MED_EXTRAS_H
#define LIBXMP_MED_EXTRAS_H

#define MED_EXTRAS_MAGIC 0x7f20ca5

struct med_extras {
	uint32 magic;
	int vts;		/* Volume table speed */
        int wts;		/* Waveform table speed */
};

#define MED_EXTRA(x) ((struct med_extras *)(x).extra)
#define HAS_MED_EXTRAS(x) \
	(MED_EXTRA(x) != NULL && MED_EXTRA(x)->magic == MED_EXTRAS_MAGIC)

void med_play_extras(struct context_data *, int, struct channel_data *, int);
int med_get_arp(struct module_data *, struct channel_data *);
int med_get_vibrato(struct channel_data *);

#endif
