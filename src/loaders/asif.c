/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdio.h>

#include "loader.h"
#include "asif.h"

#define MAGIC_FORM	MAGIC4('F','O','R','M')
#define MAGIC_ASIF	MAGIC4('A','S','I','F')
#define MAGIC_NAME	MAGIC4('N','A','M','E')
#define MAGIC_INST	MAGIC4('I','N','S','T')
#define MAGIC_WAVE	MAGIC4('W','A','V','E')

int asif_load(struct module_data *m, HIO_HANDLE *f, int i)
{
	struct xmp_module *mod = &m->mod;
	int size, pos;
	uint32 id;
	int chunk;
	int j;

	if (f == NULL)
		return -1;

	if (hio_read32b(f) != MAGIC_FORM)
		return -1;
	size = hio_read32b(f);

	if (hio_read32b(f) != MAGIC_ASIF)
		return -1;

	for (chunk = 0; chunk < 2; ) {
		id = hio_read32b(f);
		size = hio_read32b(f);
		pos = hio_tell(f) + size;

		switch (id) {
		case MAGIC_WAVE:
			//printf("wave chunk\n");
		
			hio_seek(f, hio_read8(f), SEEK_CUR);	/* skip name */
			mod->xxs[i].len = hio_read16l(f) + 1;
			size = hio_read16l(f);		/* NumSamples */
			
			//printf("WaveSize = %d\n", xxs[i].len);
			//printf("NumSamples = %d\n", size);

			for (j = 0; j < size; j++) {
				hio_read16l(f);		/* Location */
				mod->xxs[j].len = 256 * hio_read16l(f);
				hio_read16l(f);		/* OrigFreq */
				hio_read16l(f);		/* SampRate */
			}
		
			if (load_sample(m, f, SAMPLE_FLAG_UNS,
						&mod->xxs[i], NULL) < 0) {
				return -1;
			}

			chunk++;
			break;

		case MAGIC_INST:
			//printf("inst chunk\n");
		
			hio_seek(f, hio_read8(f), SEEK_CUR);	/* skip name */
		
			hio_read16l(f);			/* SampNum */
			hio_seek(f, 24, SEEK_CUR);		/* skip envelope */
			hio_read8(f);			/* ReleaseSegment */
			hio_read8(f);			/* PriorityIncrement */
			hio_read8(f);			/* PitchBendRange */
			hio_read8(f);			/* VibratoDepth */
			hio_read8(f);			/* VibratoSpeed */
			hio_read8(f);			/* UpdateRate */
		
			mod->xxi[i].nsm = 1;
			mod->xxi[i].sub[0].vol = 0x40;
			mod->xxi[i].sub[0].pan = 0x80;
			mod->xxi[i].sub[0].sid = i;

			chunk++;
		}

		hio_seek(f, pos, SEEK_SET);
	}

	return 0;
}
