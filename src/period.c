/* Extended Module Player
 * Copyright (C) 1996-2016 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include "common.h"
#include "period.h"

#include <math.h>

#ifdef LIBXMP_PAULA_SIMULATOR
/*
 * Period table from the Protracker V2.1A play routine
 */
static uint16 pt_period_table[16][36] = {
	/* Tuning 0, Normal */
	{
		856,808,762,720,678,640,604,570,538,508,480,453,
		428,404,381,360,339,320,302,285,269,254,240,226,
		214,202,190,180,170,160,151,143,135,127,120,113
	},
	/* Tuning 1 */
	{
		850,802,757,715,674,637,601,567,535,505,477,450,
		425,401,379,357,337,318,300,284,268,253,239,225,
		213,201,189,179,169,159,150,142,134,126,119,113
	},
	/* Tuning 2 */
	{
		844,796,752,709,670,632,597,563,532,502,474,447,
		422,398,376,355,335,316,298,282,266,251,237,224,
		211,199,188,177,167,158,149,141,133,125,118,112
	},
	/* Tuning 3 */
	{
		838,791,746,704,665,628,592,559,528,498,470,444,
		419,395,373,352,332,314,296,280,264,249,235,222,
		209,198,187,176,166,157,148,140,132,125,118,111
	},
	/* Tuning 4 */
	{
		832,785,741,699,660,623,588,555,524,495,467,441,
		416,392,370,350,330,312,294,278,262,247,233,220,
		208,196,185,175,165,156,147,139,131,124,117,110
	},
	/* Tuning 5 */
	{
		826,779,736,694,655,619,584,551,520,491,463,437,
		413,390,368,347,328,309,292,276,260,245,232,219,
		206,195,184,174,164,155,146,138,130,123,116,109
	},
	/* Tuning 6 */
	{
		820,774,730,689,651,614,580,547,516,487,460,434,
		410,387,365,345,325,307,290,274,258,244,230,217,
		205,193,183,172,163,154,145,137,129,122,115,109
	},
	/* Tuning 7 */
	{
		814,768,725,684,646,610,575,543,513,484,457,431,
		407,384,363,342,323,305,288,272,256,242,228,216,
		204,192,181,171,161,152,144,136,128,121,114,108
	},
	/* Tuning -8 */
	{
		907,856,808,762,720,678,640,604,570,538,508,480,
		453,428,404,381,360,339,320,302,285,269,254,240,
		226,214,202,190,180,170,160,151,143,135,127,120
	},
	/* Tuning -7 */
	{
		900,850,802,757,715,675,636,601,567,535,505,477,
		450,425,401,379,357,337,318,300,284,268,253,238,
		225,212,200,189,179,169,159,150,142,134,126,119
	},
	/* Tuning -6 */
	{
		894,844,796,752,709,670,632,597,563,532,502,474,
		447,422,398,376,355,335,316,298,282,266,251,237,
		223,211,199,188,177,167,158,149,141,133,125,118
	},
	/* Tuning -5 */
	{
		887,838,791,746,704,665,628,592,559,528,498,470,
		444,419,395,373,352,332,314,296,280,264,249,235,
		222,209,198,187,176,166,157,148,140,132,125,118
	},
	/* Tuning -4 */
	{
		881,832,785,741,699,660,623,588,555,524,494,467,
		441,416,392,370,350,330,312,294,278,262,247,233,
		220,208,196,185,175,165,156,147,139,131,123,117
	},
	/* Tuning -3 */
	{
		875,826,779,736,694,655,619,584,551,520,491,463,
		437,413,390,368,347,328,309,292,276,260,245,232,
		219,206,195,184,174,164,155,146,138,130,123,116
	},
	/* Tuning -2 */
	{
		868,820,774,730,689,651,614,580,547,516,487,460,
		434,410,387,365,345,325,307,290,274,258,244,230,
		217,205,193,183,172,163,154,145,137,129,122,115
	},
	/* Tuning -1 */
	{
		862,814,768,725,684,646,610,575,543,513,484,457,
		431,407,384,363,342,323,305,288,272,256,242,228,
		216,203,192,181,171,161,152,144,136,128,121,114
	}
};
#endif

#ifdef _MSC_VER
static inline double round(double val)
{
	return floor(val + 0.5);
}
#endif

#ifdef LIBXMP_PAULA_SIMULATOR
/* Get period from note using Protracker tuning */
static inline int note_to_period_pt(int n, int f)
{
	if (n < MIN_NOTE_MOD || n > MAX_NOTE_MOD) {
		return -1;
	}

	n -= 48;
	f >>= 4;
	if (f < -8 || f > 7) {
		return 0;
	}

	if (f < 0) {
		f += 16;
	}

	return (int)pt_period_table[f][n];
}
#endif

/* Get period from note */
double note_to_period(struct context_data *ctx, int n, int f, int type,
		      double adj)
{
	double d, per;
#ifdef LIBXMP_PAULA_SIMULATOR
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;

	/* If mod replayer, modrng and classic play are active */
	if (p->flags & XMP_FLAGS_CLASSIC) {
		if (IS_CLASSIC_MOD()) {
			return note_to_period_pt(n, f);
		}
	}
#endif

	d = (double)n + (double)f / 128;

	per = type ? (240.0 - d) * 16 :	/* Linear */
	    13694.0 / pow(2, d / 12);	/* Amiga */

#ifndef LIBXMP_CORE_PLAYER
	if (adj > 0.1) {
		per *= adj;
	}
#endif

	return per;
}

/* For the software mixer */
int note_to_period_mix(int n, int b)
{
	double d = (double)n + (double)b / 12800;
	return (int)(8192.0 * XMP_PERIOD_BASE / pow(2, d / 12));
}

/* Get note from period */
/* This function is used only by the MOD loader */
int period_to_note(int p)
{
	if (p <= 0) {
		return 0;
	}

	return round(log(pow(13694.0 / p, 12)) / M_LN2) + 1;
}

/* Get pitchbend from base note and amiga period */
int period_to_bend(struct context_data *ctx, double p, int n, int type,
		   double adj)
{
	double d;

	if (n == 0) {
		return 0;
	}

	if (type) {
		return 100 * (8 * (((240 - n) << 4) - p));	/* Linear */
	}

	d = note_to_period(ctx, n, 0, 0, adj);
	return round(100.0 * ((1536.0 * log(d / p) / M_LN2)));	/* Amiga */
}

/* Convert finetune = 1200 * log2(C2SPD/8363))
 *
 *      c = (1200.0 * log(c2spd) - 1200.0 * log(c4_rate)) / M_LN2;
 *      xpo = c/100;
 *      fin = 128 * (c%100) / 100;
 */
void c2spd_to_note(int c2spd, int *n, int *f)
{
	int c;

	if (c2spd == 0) {
		*n = *f = 0;
		return;
	}

	c = (int)(1536.0 * log((double)c2spd / 8363) / M_LN2);
	*n = c / 128;
	*f = c % 128;
}
