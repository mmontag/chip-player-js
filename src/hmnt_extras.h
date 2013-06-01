#ifndef XMP_HMNT_EXTRAS_H
#define XMP_HMNT_EXTRAS_H

#define HMNT_EXTRAS_MAGIC 0x041bc81a

struct hmnt_extras {
	uint32 magic;
	int dataloopstart;
	int dataloopend;
	uint8 data[64];
	uint8 progvolume[64];
};

#define HMNT_EXTRA(x) ((struct hmnt_extras *)(x).extra)
#define HAS_HMNT_EXTRAS(x) \
	(HMNT_EXTRA(x) != NULL && HMNT_EXTRA(x)->magic == HMNT_EXTRAS_MAGIC)

void hmnt_synth(struct context_data *, int, struct channel_data *, int);

#endif

