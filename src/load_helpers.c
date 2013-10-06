/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <string.h>
#include <stdlib.h>
#include <fnmatch.h>
#include "common.h"
#include "synth.h"


/*
 * Handle special "module quirks" that can't be detected automatically
 * such as Protracker 2.x compatibility, vblank timing, etc.
 */

struct module_quirk {
	uint8 md5[16];
	int flags;
};

const struct module_quirk mq[] = {
	/* "No Mercy" by Alf/VTL (added by Martin Willers) */
	{
		{ 0x36, 0x6e, 0xc0, 0xfa, 0x96, 0x2a, 0xeb, 0xee,
	  	  0x03, 0x4a, 0xa2, 0xdb, 0xaa, 0x49, 0xaa, 0xea },
		XMP_FLAGS_FX9BUG
	},

	/* mod.souvenir of china */
	{
		{ 0x93, 0xf1, 0x46, 0xae, 0xb7, 0x58, 0xc3, 0x9d,
		  0x8b, 0x5f, 0xbc, 0x98, 0xbf, 0x23, 0x7a, 0x43 },
		XMP_FLAGS_FIXLOOP
	},

	/* "siedler ii" (added by Daniel Åkerud) */
	{
		{ 0x70, 0xaa, 0x03, 0x4d, 0xfb, 0x2f, 0x1f, 0x73,
		  0xd9, 0xfd, 0xba, 0xfe, 0x13, 0x1b, 0xb7, 0x01 },
		XMP_FLAGS_VBLANK
	},

	/* "Klisje paa klisje" (added by Kjetil Torgrim Homme) */
	{
		{ 0xe9, 0x98, 0x01, 0x2c, 0x70, 0x0e, 0xb4, 0x3a,
		  0xf0, 0x32, 0x17, 0x11, 0x30, 0x58, 0x29, 0xb2 },
		XMP_FLAGS_VBLANK
	},

	{
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		0
	}
};

static void module_quirks(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	int i;

	for (i = 0; mq[i].flags != 0; i++) {
		if (!memcmp(m->md5, mq[i].md5, 16)) {
			p->flags |= mq[i].flags;
		}
	}
}

static void check_envelope(struct xmp_envelope *env)
{
	if (env->lps >= env->npt || env->lpe >= env->npt)
		env->flg &= ~XMP_ENVELOPE_LOOP;
}

/* 
 * Check whether the given string matches one of the blacklisted glob
 * patterns. Used to filter file names stored in archive files.
 */
int exclude_match(char *name)
{
	int i;

	static const char *const exclude[] = {
		"README", "readme",
		"*.DIZ", "*.diz",
		"*.NFO", "*.nfo",
		"*.DOC", "*.Doc", "*.doc",
		"*.INFO", "*.info", "*.Info",
		"*.TXT", "*.txt",
		"*.EXE", "*.exe",
		"*.COM", "*.com",
		"*.README", "*.readme", "*.Readme", "*.ReadMe",
		NULL
	};

	for (i = 0; exclude[i] != NULL; i++) {
		if (fnmatch(exclude[i], name, 0) == 0) {
			return 1;
		}
	}

	return 0;
}

void load_prologue(struct context_data *ctx)
{
	struct module_data *m = &ctx->m;
	int i;

	/* Reset variables */
	memset(m->mod.name, 0, XMP_NAME_SIZE);
	memset(m->mod.type, 0, XMP_NAME_SIZE);
	m->rrate = PAL_RATE;
	m->c4rate = C4_PAL_RATE;
	m->volbase = 0x40;
	m->gvolbase = 0x40;
	m->vol_table = NULL;
	m->quirk = 0;
	m->read_event_type = READ_EVENT_MOD;
	m->comment = NULL;

	/* Set defaults */
    	m->mod.pat = 0;
    	m->mod.trk = 0;
    	m->mod.chn = 4;
    	m->mod.ins = 0;
    	m->mod.smp = 0;
    	m->mod.spd = 6;
    	m->mod.bpm = 125;
    	m->mod.len = 0;
    	m->mod.rst = 0;

	m->synth = &synth_null;
	m->extra = NULL;
	m->time_factor = DEFAULT_TIME_FACTOR;

	for (i = 0; i < 64; i++) {
		m->mod.xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
		m->mod.xxc[i].vol = 0x40;
		m->mod.xxc[i].flg = 0;
	}
}

void load_epilogue(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int i, j;

    	mod->gvl = m->gvolbase;

	/* Fix cases where the restart value is invalid e.g. kc_fall8.xm
	 * from http://aminet.net/mods/mvp/mvp_0002.lha (reported by
	 * Ralf Hoffmann <ralf@boomerangsworld.de>)
	 */
	if (mod->rst >= mod->len) {
		mod->rst = 0;
	}

	/* Sanity check */
	if (mod->spd == 0) {
		mod->spd = 6;
	}
	if (mod->bpm == 0) {
		mod->bpm = 125;
	}

	/* Set appropriate values for instrument volumes and subinstrument
	 * global volumes when QUIRK_INSVOL is not set, to keep volume values
	 * consistent if the user inspects struct xmp_module. We can later
	 * set volumes in the loaders and eliminate the quirk.
	 */
	for (i = 0; i < mod->ins; i++) {
		if (~m->quirk & QUIRK_INSVOL) {
			mod->xxi[i].vol = m->volbase;
		}
		for (j = 0; j < mod->xxi[i].nsm; j++) {
			if (~m->quirk & QUIRK_INSVOL) {
				mod->xxi[i].sub[j].gvl = m->volbase;
			}
		}
	}

	/* Sanity check for envelopes
	 */
	for (i = 0; i < mod->ins; i++) {
		check_envelope(&mod->xxi[i].aei);
		check_envelope(&mod->xxi[i].fei);
		check_envelope(&mod->xxi[i].pei);
	}

	p->flags = p->player_flags;
	module_quirks(ctx);
}

int prepare_scan(struct context_data *ctx)
{
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int i, ord;

	ord = 0;
	while (ord < mod->len && mod->xxo[ord] >= mod->pat) {
		ord++;
	}
	if (ord >= mod->len)
		mod->len = 0;

	m->scan_cnt = calloc(sizeof (char *), mod->len);
	if (m->scan_cnt == NULL)
		return -XMP_ERROR_SYSTEM;

	for (i = 0; i < mod->len; i++) {
		int pat = mod->xxo[i];
		m->scan_cnt[i] = calloc(1, pat >= mod->pat ?  1 :
			mod->xxp[pat]->rows ? mod->xxp[pat]->rows : 1);
		if (m->scan_cnt[i] == NULL)
			return -XMP_ERROR_SYSTEM;
	}
 
	return 0;
}
