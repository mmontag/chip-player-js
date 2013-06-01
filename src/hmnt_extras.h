#ifndef XMP_HMNT_EXTRAS_H
#define XMP_HMNT_EXTRAS_H

#define HMNT_EXTRAS_MAGIC 0x041bc81a

struct hmnt_extras {
	uint32 magic;
	int dataloopstart;
	int dataloopend;
	uint8 data[64];
};

#define HMNT_EXTRA(x) ((struct hmnt_extras *)(x).extra)

#endif

