#ifndef XMP_HMN_EXTRAS_H
#define XMP_HMN_EXTRAS_H

#define HMN_EXTRAS_MAGIC 0x041bc81a

struct hmn_extras {
	uint32 magic;
	int dataloopstart;
	int dataloopend;
	uint8 data[64];
	uint8 progvolume[64];
};

#define HMN_EXTRA(x) ((struct hmn_extras *)(x).extra)
#define HAS_HMN_EXTRAS(x) \
	(HMN_EXTRA(x) != NULL && HMN_EXTRA(x)->magic == HMN_EXTRAS_MAGIC)

void hmn_play_extras(struct context_data *, int, struct channel_data *, int);
void hmn_set_arpeggio(struct channel_data *, int);

#endif

