/* Creates a simple module */

#include "../src/common.h"
#include "../src/loaders/load.h"

void initialize_module_data(struct module_data *);
void fix_module_instruments(struct module_data *);

static char sample[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

void create_simple_module(struct context_data *ctx)
{
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int i;

	initialize_module_data(m);

	/* Create module */

	mod->len = 2;
	mod->pat = 2;
	mod->ins = 2;
	mod->chn = 4;
	mod->trk = mod->pat * mod->chn;
	mod->smp = mod->ins;
	mod->xxo[0] = 0;
	mod->xxo[1] = 1;

	PATTERN_INIT();

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);
	}

	INSTRUMENT_INIT();

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].nsm = 1;
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].vol = 0x40;
		mod->xxi[i].sub[0].sid = i;

		mod->xxs[i].len = 10;
		mod->xxs[i].lps = 0;
		mod->xxs[i].lpe = 10;
		mod->xxs[i].flg = XMP_SAMPLE_LOOP;
		mod->xxs[i].data = (uint8 *)&sample;
	}
		
	/* End of module creation */

	m->time = scan_module(ctx);

	fix_module_instruments(m);
}

void new_event(struct context_data *ctx, int pat, int row, int chn, int note, int ins, int vol, int fxt, int fxp, int f2t, int f2p)
{
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct xmp_event *e;
	int track;

	track = mod->xxp[pat]->index[chn];
	e = &mod->xxt[track]->event[row];

	e->note = note;
	e->ins = ins;
	e->vol = vol;
	e->fxt = fxt;
	e->fxp = fxp;
	e->f2t = fxt;
	e->f2p = fxp;
}
