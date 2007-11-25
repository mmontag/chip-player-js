/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: xpanel.c,v 1.3 2007-11-25 13:58:11 cmatsuoka Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "xpanel.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bg2.xpm"

#define rightmsg(f,x,y,s,c,b) \
    writemsg (f, (x) - writemsg (f,0,0,s,-1,0), y, s, c, b)

#define centermsg(f,x,y,s,c,b) \
    shadowmsg (f, (x) - writemsg (f,0,0,s,-1,0) / 2, y, s, c, b)


struct chn_c {
    int s;
    int x, y, w, h;
    int old_y;
    int val;
};

#define FRQ 20

static struct chn_c chn[64];
static struct chn_c frq[FRQ];
extern struct font_header font1;
extern struct font_header font2;
extern struct ipc_info *ii;
extern int skip;
static char **bg;

void rdft(int, int, float *, int *, float *);


int new_module = 0;
#define xmp_check_parent(i) (new_module)
#define xmp_wait_parent() do { if (!new_module) return 1; } while (0)
#define xmp_tell_parent() do { new_module = 0; } while (0)


void volume_bars()
{
    int x, y, w, n, r, i, v;

    n = (RES_X - 16) / ii->mi.chn;
    r = ((RES_X - 16) - n * ii->mi.chn) >> 1;

    w = n - 2;

    for (i = 0; i < ii->mi.chn; i++) {

	v = ii->vol[i];
	if (v < 0)
	    v = 0;
	if (v > 0x40)
	    v = 0x40;
	x = i * n + 8 + r;
	y = (RES_Y - 8) - (RES_Y - 16) * v / 0x40;

	if (ii->mute[i]) {
	    if (chn[i].old_y > 0) {
		erase_rectangle (x, chn[i].old_y, w,
			RES_Y - 8 - chn[i].old_y);
		draw_rectangle (x, 8, w, RES_Y - 16);
		erase_rectangle (x + 2, 10, w - 4, RES_Y - 20);
		chn[i].old_y = -1;
	    }
	    chn[i].s = 1;
	    chn[i].y = 8;
	    chn[i].h = RES_Y - 16;
	    continue;
	} else if (chn[i].old_y < 0) {
	    draw_rectangle (x + 2, 10, w - 4, RES_Y - 20);
	    erase_rectangle (x, 8, w, RES_Y - 16);
	    chn[i].old_y = RES_Y - 8;
	    chn[i].s = 1;
	    chn[i].y = 8;
	    chn[i].h = RES_Y - 16;
	}

	if (y > chn[i].old_y) {
	    erase_rectangle (x, chn[i].old_y, w, y - chn[i].old_y);
	    if (!chn[i].s) {
		chn[i].s = 1;
		chn[i].y = chn[i].old_y;
		chn[i].h = y - chn[i].old_y;
	    }
	} else if (y < chn[i].old_y) {
	    draw_rectangle (x, y, w, chn[i].old_y - y);
	    if (!chn[i].s) {
		chn[i].s = 1;
		chn[i].y = y;
		chn[i].h = chn[i].old_y - y;
	    }
	}
	chn[i].old_y = y;
    }
}


void shadowmsg (struct font_header *f, int x, int y, char *s, int c, int b)
{
    writemsg (f, x + 2, y + 2, s, 0, b);
    writemsg (f, x, y, s, c, b);
}


void set_palette()
{
    bg = bg2;
    setpalette(bg);
}


void clear_screen()
{
    draw_xpm(bg, RES_X, RES_Y);
    update_display();
}


void prepare_screen()
{
    char buf[90];

    draw_xpm (bg, RES_X, RES_Y);

    strncpy(buf, ii->mi.name, 80);

    if (writemsg (&font1, 0, 0, buf, -1, 0) > RES_X) {
	while (writemsg (&font1, 0, 0, buf, -1, 0) > RES_X - 16)
	    buf[strlen (buf) - 1] = 0;
	strcat (buf, "...");
    }

    centermsg (&font1, RES_X / 2, 26, buf, 1, -1);
    sprintf (buf, "Channels: %d", ii->mi.chn);
    centermsg (&font2, RES_X / 2, 48, buf, 2, -1);
    sprintf (buf, "Instruments: %d", ii->mi.ins);
    centermsg (&font2, RES_X / 2, 66, buf, 2, -1);
    sprintf (buf, "Length: %d patterns", ii->mi.len);
    centermsg (&font2, RES_X / 2, 84, buf, 2, -1);
    sprintf (buf, "Pattern:");
    shadowmsg (&font2, 66, 102, buf, 2, -1);
    sprintf (buf, "Row:");
    shadowmsg (&font2, 176, 102, buf, 2, -1);
    sprintf (buf, "Progress:   %%");
    centermsg (&font2, RES_X / 2, 120, buf, 2, -1);

    update_display();
}


extern xmp_context ctx;

void x11_event_callback(unsigned long i)
{
    static int chn = 0;
    static int ord = 0;
    long msg = i >> 4;
    int m, cmd = ii->cmd;

    switch (i & 0xf) {
    case XMP_ECHO_ORD:
	ord = msg & 0xff;
	ii->pat = msg >> 8;
	break;
    case XMP_ECHO_ROW:
	if ((m = msg >> 8) == 0)
	    m = 0x100;
	ii->progress = ord * 100 / ii->mi.len +
	    (msg & 0xff) * 100 / ii->mi.len / m;
	ii->row = msg & 0xff;
	break;
    case XMP_ECHO_CHN:
	chn = msg & 0xff;
	break;
    case XMP_ECHO_VOL:
	ii->vol[chn] = msg & 0xff;
	break;
    }

    switch (cmd) {
    case 'q':			/* quit */
	skip = -2;
	xmp_mod_stop(ctx);
	if (ii->pause)
	    ii->pause = xmp_mod_pause(ctx);
	break;
    case 'f':			/* jump to next order */
	xmp_ord_next(ctx);
	if (ii->pause)
	    ii->pause = xmp_mod_pause(ctx);
	break;
    case 'b':			/* jump to previous order */
	xmp_ord_prev(ctx);
	if (ii->pause)
	    ii->pause = xmp_mod_pause(ctx);
	break;
    case 'n':			/* skip to next module */
	skip = 1;
	xmp_mod_stop(ctx);
	if (ii->pause)
	    ii->pause = xmp_mod_pause(ctx);
	break;
    case 'p':			/* skip to previous module */
	skip = -1;
	xmp_mod_stop(ctx);
	if (ii->pause)
	    ii->pause = xmp_mod_pause(ctx);
	break;
    case ' ':			/* pause module */
	ii->pause = xmp_mod_pause(ctx);
	break;
    default:
        if (cmd >= '1' && cmd <= '9') {
	    xmp_channel_mute(ctx, cmd - '1', 1, -1);
	    ii->mute[cmd - '1'] = !ii->mute[cmd - '1'];
	    break;
	}
        if (cmd == '0') {
	    xmp_channel_mute(ctx, 9, 1, -1);
	    ii->mute[9] = !ii->mute[9];
	    break;
	}
        if (cmd == '!') {
	    xmp_channel_mute (ctx, 0, 64, 0);
	    for (i = 0; i < 64; i++)
		ii->mute[i] = 0;
	    break;
	}
	if (cmd < 0) {
	    xmp_channel_mute(ctx, -cmd - 1, 1, !ii->mute[-cmd - 1]);
	    ii->mute[-cmd - 1] ^= 1;
	    break;
	}
    }

    ii->cmd = 0;
}


void display_loop()
{
    panel_setup();
    while (42)
	panel_loop();
}



    static char s[8];
    static int *b, *p, *q;

void panel_setup()
{
    int c, n, r;

    p = malloc (15 * 13 * sizeof (int));
    b = malloc (22 * 13 * sizeof (int));
    q = malloc (22 * 13 * sizeof (int));

    n = (RES_X - 16) / (ii->mi.chn ? ii->mi.chn : 1);
    r = ((RES_X - 16) - n * ii->mi.chn) >> 1;

    get_rectangle (177, 106, 15, 13, p);
    get_rectangle (140, 88, 22, 13, b);
    get_rectangle (220, 88, 22, 13, q);

    for (c = 0; c < 64; c++)
	chn[c].old_y = RES_Y - 8;

    for (c = 0; c < FRQ; c++)
	frq[c].old_y = RES_Y - 8;
}


int panel_loop()
{ 
     int c, x, y;

	switch ((c = process_events (&x, &y))) {
	case -1:		/* mute channel */
	    for (c = 0; c < ii->mi.chn; c++) {
		if (x >= chn[c].x && x < (chn[c].x + chn[c].w)) {
		    c = -c - 1;
		    break;
		}
	    }
	    if (c == 64)
		break;
	default:
	    if (!ii->cmd)
		ii->cmd = c;
	}

	if (ii->pause || ii->mi.chn == 0) {
	    return 1;
	}

	put_rectangle (177, 106, 15, 13, p);
	put_rectangle (140, 88, 22, 13, b);
	put_rectangle (220, 88, 22, 13, q);

	volume_bars();

	get_rectangle (177, 106, 15, 13, p);
	get_rectangle (140, 88, 22, 13, b);
	get_rectangle (220, 88, 22, 13, q);

	sprintf (s, "%d", ii->progress);
	shadowmsg (&font2, 177, 106 + 14, s, 2, -1);
	sprintf (s, "%02d", ii->pat);
	shadowmsg (&font2, 140, 86 + 16, s, 2, -1);
	sprintf (s, "%02d", ii->row);
	shadowmsg (&font2, 220, 86 + 16, s, 2, -1);

	for (c = 0; c < ii->mi.chn; c++) {
	    if (ii->vol[c] > 4)
		ii->vol[c] -= 4;
	    else
		ii->vol[c] = 0;
	}

	/* Check if we have a new file */

	if (xmp_check_parent (40)) {
	    int n, r;

	    xmp_wait_parent();
	    put_rectangle (177, 106, 15, 13, p);
	    put_rectangle (140, 88, 22, 13, b);
	    put_rectangle (220, 88, 22, 13, q);
	    volume_bars();
	    prepare_screen();
	    get_rectangle (177, 106, 15, 13, p);
	    get_rectangle (140, 88, 22, 13, b);
	    get_rectangle (220, 88, 22, 13, q);

	    for (c = 0; c < 64; c++) {
		n = (RES_X - 16) / ii->mi.chn;
		r = ((RES_X - 16) - n * ii->mi.chn) >> 1;
        	chn[c].x = c * n + 8 + r;
        	chn[c].w = n - 2;
		chn[c].old_y = RES_Y - 8;
		ii->vol[c] = 0;
		ii->mute[c] = 0;
	    }

	    for (c = 0; c < FRQ; c++) {
		n = (RES_X - 16) / FRQ;
		r = ((RES_X - 16) - n * FRQ) >> 1;
        	frq[c].x = c * n + 8 + r;
        	frq[c].w = n - 2;
		frq[c].old_y = RES_Y - 8;
		ii->vol[c] = 0;
	    }

	    prepare_screen();
	    xmp_tell_parent();
	}

	update_display();

	{
	    for (c = 0; c < ii->mi.chn; c++) {
		if (chn[c].s) {
		    chn[c].s = 0;
		}
	    }

	    for (c = 0; c < FRQ; c++) {
		if (frq[c].s) {
		    frq[c].s = 0;
		}

#define SPEED 4
		if (frq[c].val < RES_Y - 8 - SPEED)
		    frq[c].val += SPEED;
		else
		    frq[c].val = RES_Y - 8;
	    }
	}

	update_display();

	return 1;
}

