/* Extended Module Player core player
 * Copyright (C) 1996-2014 Claudio Matsuoka and Hipolito Carraro Jr
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

/*
 * Sun, 31 May 1998 17:50:02 -0600
 * Reported by ToyKeeper <scriven@CS.ColoState.EDU>:
 * For loop-prevention, I know a way to do it which lets most songs play
 * fine once through even if they have backward-jumps. Just keep a small
 * array (256 bytes, or even bits) of flags, each entry determining if a
 * pattern in the song order has been played. If you get to an entry which
 * is already non-zero, skip to the next song (assuming looping is off).
 */

/*
 * Tue, 6 Oct 1998 21:23:17 +0200 (CEST)
 * Reported by John v/d Kamp <blade_@dds.nl>:
 * scan.c was hanging when it jumps to an invalid restart value.
 * (Fixed by hipolito)
 */


#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "effects.h"
#include "mixer.h"

#define S3M_END		0xff
#define S3M_SKIP	0xfe


static int scan_module(struct context_data *ctx, int ep, int chain)
{
    struct player_data *p = &ctx->p;
    struct module_data *m = &ctx->m;
    struct xmp_module *mod = &m->mod;
    int parm, gvol_memory, f1, f2, p1, p2, ord, ord2;
    int row, last_row, break_row, cnt_row;
    int gvl, bpm, speed, base_time, chn;
    int alltmp;
    double clock, clock_rst;
    int loop_chn, loop_flg;
    int pdelay = 0, skip_fetch;
    int loop_stk[XMP_MAX_CHANNELS];
    int loop_row[XMP_MAX_CHANNELS];
    struct xmp_event* event;
    int i, pat;
    struct ord_data *info;

    if (mod->len == 0)
	return 0;

    for (i = 0; i < mod->len; i++) {
	int pat = mod->xxo[i];
	memset(m->scan_cnt[i], 0, pat >= mod->pat ? 1 :
			mod->xxp[pat]->rows ? mod->xxp[pat]->rows : 1);
    }

    memset(loop_stk, 0, sizeof(int) * mod->chn);
    memset(loop_row, 0, sizeof(int) * mod->chn);
    loop_chn = loop_flg = 0;

    gvl = mod->gvl;
    bpm = mod->bpm;

    speed = mod->spd;
    base_time = m->rrate;

    /* By erlk ozlr <erlk.ozlr@gmail.com>
     *
     * xmp doesn't handle really properly the "start" option (-s for the
     * command-line) for DeusEx's .umx files. These .umx files contain
     * several loop "tracks" that never join together. That's how they put
     * multiple musics on each level with a file per level. Each "track"
     * starts at the same order in all files. The problem is that xmp starts
     * decoding the module at order 0 and not at the order specified with
     * the start option. If we have a module that does "0 -> 2 -> 0 -> ...",
     * we cannot play order 1, even with the supposed right option.
     *
     * was: ord2 = ord = -1;
     *
     * CM: Fixed by using different "sequences" for each loop or subsong.
     *     Each sequence has its entry point. Sequences don't overlap.
     */
    ord2 = -1;
    ord = ep - 1;

    gvol_memory = break_row = cnt_row = alltmp = 0;
    clock_rst = clock = 0.0;
    skip_fetch = 0;

    while (42) {
	if ((uint32)++ord >= mod->len) {
	    if (mod->rst > mod->len || mod->xxo[mod->rst] >= mod->pat) {
		ord = m->seq_data[chain].entry_point;
	    } else {
		if (get_sequence(ctx, mod->rst) == chain) {
	            ord = mod->rst;
		} else {
		    ord = ep;
	        }
	    }
	   
	    pat = mod->xxo[ord];
	    if (pat == S3M_END) {
		break;
	    }
	} 

	pat = mod->xxo[ord];
	info = &m->xxo_info[ord];

	if (ep != 0 && p->sequence_control[ord] != 0xff) {
	    break;
	}
	p->sequence_control[ord] = chain;

	/* All invalid patterns skipped, only S3M_END aborts replay */
	if (pat >= mod->pat) {
	    if (pat == S3M_END) {
		ord = mod->len;
	        continue;
	    }
	    continue;
	}

	if (break_row < mod->xxp[pat]->rows && m->scan_cnt[ord][break_row])
	    break;

	info->gvl = gvl;
	info->bpm = bpm;
	info->speed = speed;
	info->time = clock + m->time_factor * alltmp / bpm;

	if (info->start_row == 0 && ord != 0) {
	    if (ord == ep) {
		clock_rst = clock + m->time_factor * alltmp / bpm;
	    }

	    info->start_row = break_row;
	}

	last_row = mod->xxp[pat]->rows;
	for (row = break_row, break_row = 0; row < last_row; row++, cnt_row++) {
	    /* Prevent crashes caused by large softmixer frames */
	    if (bpm < XMP_MIN_BPM) {
	        bpm = XMP_MIN_BPM;
	    }

	    /* Date: Sat, 8 Sep 2007 04:01:06 +0200
	     * Reported by Zbigniew Luszpinski <zbiggy@o2.pl>
	     * The scan routine falls into infinite looping and doesn't let
	     * xmp play jos-dr4k.xm.
	     * Claudio's workaround: we'll break infinite loops here.
	     *
	     * Date: Oct 27, 2007 8:05 PM
	     * From: Adric Riedel <adric.riedel@gmail.com>
	     * Jesper Kyd: Global Trash 3.mod (the 'Hardwired' theme) only
	     * plays the first 4:41 of what should be a 10 minute piece.
	     * (...) it dies at the end of position 2F
	     */

	    if (cnt_row > 512)	/* was 255, but Global trash goes to 318 */
		goto end_module;

	    if (!loop_flg && m->scan_cnt[ord][row]) {
		cnt_row--;
		goto end_module;
	    }
	    m->scan_cnt[ord][row]++;

	    pdelay = 0;

	    for (chn = 0; chn < mod->chn; chn++) {
		if (row >= mod->xxt[mod->xxp[pat]->index[chn]]->rows)
		    continue;

		event = &EVENT(mod->xxo[ord], chn, row);

		/* Pattern delay + pattern break cause target row events
		 * to be ignored
		 */
		if (skip_fetch) {
		    f1 = p1 = f2 = p2 = 0;
		} else {
		    f1 = event->fxt;
		    p1 = event->fxp;
		    f2 = event->f2t;
		    p2 = event->f2p;
		}

		if (f1 == FX_GLOBALVOL || f2 == FX_GLOBALVOL) {
		    gvl = (f1 == FX_GLOBALVOL) ? p1 : p2;
		    gvl = gvl > m->gvolbase ? m->gvolbase : gvl < 0 ? 0 : gvl;
		}

		/* Process fine global volume slide */
		if (f1 == FX_GVOL_SLIDE || f2 == FX_GVOL_SLIDE) {
		    int h, l;
		    parm = (f1 == FX_GVOL_SLIDE) ? p1 : p2;

		process_gvol:
                    if (parm) {
			gvol_memory = parm;
                        h = MSN(parm);
                        l = LSN(parm);

		        if (HAS_QUIRK(QUIRK_FINEFX)) {
                            if (l == 0xf && h != 0) {
				gvl += h;
			    } else if (h == 0xf && l != 0) {
				gvl -= l;
			    } else {
		                if (m->quirk & QUIRK_VSALL) {
                                    gvl += (h - l) * speed;
				} else {
                                    gvl += (h - l) * (speed - 1);
				}
			    }
			} else {
		            if (m->quirk & QUIRK_VSALL) {
                                gvl += (h - l) * speed;
			    } else {
                                gvl += (h - l) * (speed - 1);
			    }
			}
		    } else {
                        if ((parm = gvol_memory) != 0)
			    goto process_gvol;
		    }
		}

		if ((f1 == FX_SPEED && p1) || (f2 == FX_SPEED && p2)) {
		    parm = (f1 == FX_SPEED) ? p1 : p2;
		    alltmp += cnt_row * speed * base_time;
		    cnt_row = 0;
		    if (parm) {
			if (p->flags & XMP_FLAGS_VBLANK || parm < 0x20) {
			    speed = parm;
			} else {
			    clock += m->time_factor * alltmp / bpm;
			    alltmp = 0;
			    bpm = parm;
			}
		    }
		}

#ifndef LIBXMP_CORE_PLAYER
		if (f1 == FX_SPEED_CP) {
		    f1 = FX_S3M_SPEED;
		}
		if (f2 == FX_SPEED_CP) {
		    f2 = FX_S3M_SPEED;
		}
#endif

		if ((f1 == FX_S3M_SPEED && p1) || (f2 == FX_S3M_SPEED && p2)) {
		    parm = (f1 == FX_S3M_SPEED) ? p1 : p2;
		    alltmp += cnt_row * speed * base_time;
		    cnt_row  = 0;
		    speed = parm;
		}

		if ((f1 == FX_S3M_BPM && p1) || (f2 == FX_S3M_BPM && p2)) {
		    parm = (f1 == FX_S3M_BPM) ? p1 : p2;
		    alltmp += cnt_row * speed * base_time;
		    cnt_row = 0;
		    clock += m->time_factor * alltmp / bpm;
		    alltmp = 0;
		    bpm = parm;
		}

#ifndef LIBXMP_CORE_DISABLE_IT
		if ((f1 == FX_IT_BPM && p1) || (f2 == FX_IT_BPM && p2)) {
		    parm = (f1 == FX_IT_BPM) ? p1 : p2;
		    alltmp += cnt_row * speed * base_time;
		    cnt_row = 0;
		    clock += m->time_factor * alltmp / bpm;
		    alltmp = 0;

		    if (MSN(parm) == 0) {
		        clock += m->time_factor * base_time / bpm;
			for (i = 1; i < speed; i++) {
			    bpm -= LSN(parm);
			    if (bpm < 0x20)
				bpm = 0x20;
		            clock += m->time_factor * base_time / bpm;
			}

			// remove one row at final bpm
			clock -= m->time_factor * speed * base_time / bpm;

		    } else if (MSN(parm) == 1) {
		        clock += m->time_factor * base_time / bpm;
			for (i = 1; i < speed; i++) {
			    bpm += LSN(parm);
			    if (bpm > 0xff)
				bpm = 0xff;
		            clock += m->time_factor * base_time / bpm;
			}

			// remove one row at final bpm
			clock -= m->time_factor * speed * base_time / bpm;

		    } else {
			bpm = parm;
		    }
		}
#endif

		if (f1 == FX_JUMP || f2 == FX_JUMP) {
		    ord2 = (f1 == FX_JUMP) ? p1 : p2;
		    last_row = 0;
		}

		if (f1 == FX_BREAK || f2 == FX_BREAK) {
		    parm = (f1 == FX_BREAK) ? p1 : p2;
		    break_row = 10 * MSN(parm) + LSN(parm);
		    last_row = 0;
		}

		if (f1 == FX_EXTENDED || f2 == FX_EXTENDED) {
		    parm = (f1 == FX_EXTENDED) ? p1 : p2;

		    if ((parm >> 4) == EX_PATT_DELAY) {
			pdelay = parm & 0x0f;
			alltmp += pdelay * speed * base_time;
		    }

		    if ((parm >> 4) == EX_PATTERN_LOOP) {
			if (parm &= 0x0f) {
			    if (loop_stk[chn]) {
				if (--loop_stk[chn])
				    loop_chn = chn + 1;
				else {
				    loop_flg--;
				    if (m->quirk & QUIRK_S3MLOOP)
					loop_row[chn] = row + 1;
				}
			    } else {
				if (loop_row[chn] <= row) {
				    loop_stk[chn] = parm;
				    loop_chn = chn + 1;
				    loop_flg++;
				}
			    }
			} else { 
			    loop_row[chn] = row;
			}
		    }
		}
	    }
	    skip_fetch = 0;

	    if (loop_chn) {
		row = loop_row[--loop_chn] - 1;
		loop_chn = 0;
	    }
	}

	if (break_row && pdelay) {
	    skip_fetch = 1;
	}

	if (ord2 >= 0) {
	    ord = ord2 - 1;
	    ord2 = -1;
	}

	alltmp += cnt_row * speed * base_time;
	cnt_row = 0;
    }
    row = break_row;

end_module:
    p->scan[chain].num = m->scan_cnt[ord][row];
    p->scan[chain].row = row;
    p->scan[chain].ord = ord;

    clock -= clock_rst;
    alltmp += cnt_row * speed * base_time;

    return (clock + m->time_factor * alltmp / bpm);
}

int get_sequence(struct context_data *ctx, int ord)
{
	struct player_data *p = &ctx->p;
	return p->sequence_control[ord];
}

int scan_sequences(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int i, ep;
	int seq;
	unsigned char temp_ep[XMP_MAX_MOD_LENGTH];

	ep = 0;
	memset(p->sequence_control, 0xff, XMP_MAX_MOD_LENGTH);
	temp_ep[0] = 0;
	p->scan[0].time = scan_module(ctx, ep, 0);
	seq = 1;

 	while (1) { 
		/* Scan song starting at given entry point */
		/* Check if any patterns left */
		for (i = 0; i < mod->len; i++) {
			if (p->sequence_control[i] == 0xff) {
				break;
			}
		}
		if (i != mod->len && seq < MAX_SEQUENCES) {
			/* New entry point */
			ep = i;
			temp_ep[seq] = ep;
			p->scan[seq].time = scan_module(ctx, ep, seq);
			if (p->scan[seq].time > 0)
				seq++;
		} else {
			break;
		}
	}

	m->num_sequences = seq;

	/* Now place entry points in the public accessible array */
	for (i = 0; i < m->num_sequences; i++) {
		m->seq_data[i].entry_point = temp_ep[i];
		m->seq_data[i].duration = p->scan[i].time;
	}


	return 0;
}
