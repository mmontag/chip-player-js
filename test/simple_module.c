/* Creates a simple module */

#include "../src/common.h"
#include "../src/loaders/load.h"

void initialize_module_data(struct module_data *);
void fix_module_instruments(struct module_data *);

struct context_data *simple_module()
{
	xmp_context opaque;
	struct context_data *ctx;
	struct module_data *m;
	struct xmp_module *mod;
	int i;

	opaque = xmp_create_context();
	if (opaque == NULL) {
		return NULL;
	}

	ctx = (struct context_data *)opaque;
	m = &ctx->m;
	mod = &m->mod;

	initialize_module_data(m);

	/* Create module */

	mod->len = 2;
	mod->pat = 2;
	mod->ins = 2;
	mod->chn = 4;
	mod->trk = mod->pat * mod->chn;
	mod->smp = mod->ins;

	PATTERN_INIT();

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
	}

	INSTRUMENT_INIT();

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].nsm = 1;
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].vol = 0x40;
		mod->xxi[i].sub[0].sid = i;

		mod->xxs[i].len = 101;
		mod->xxs[i].lps = 0;
		mod->xxs[i].lpe = 101;
		mod->xxs[i].flg = XMP_SAMPLE_LOOP;
	}
		
	/* End of module creation */

	fix_module_instruments(m);

	return ctx;
}
