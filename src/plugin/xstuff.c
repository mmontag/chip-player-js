/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: xstuff.c,v 1.6 2007-11-23 11:40:59 cmatsuoka Exp $
 */

/*
 * Tue, 29 Sep 1998 21:39:23 +0200 (CEST)  John v/d Kamp <blade_@dds.nl>
 * Made a fix to support True Color visuals with 15 bpp.
 */

/*
 * Fri, 09 Oct 1998 11:33:08 +0100  Adam Hodson <A.Hodson-CSSE96@cs.bham.ac.uk>
 * It is possible to get xxmp to compile under FreeBSD. In the file xstuff.c
 * add #include<sys/types.h> before #include<sys/ipc.h>.
 */

static XColor color[20];
static int pmap[256];
static int indexed;

/* Fix for 16bpp true color visual */
static int mask_r = 0xfe0000;
static int mask_g = 0x00fe00;
static int mask_b = 0x0000fe;

void (*draw_rectangle)(int, int, int, int);
void (*erase_rectangle)(int, int, int, int);


static void draw_rectangle_rgb24(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    if (c != 0xfefefe && c != 0xd0d0d0)
		XPutPixel(ximage, i, j, c >> 1);
	}
}


static void erase_rectangle_rgb24(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    if (c != 0xfefefe && c != 0xd0d0d0)
		XPutPixel(ximage, i, j, c << 1);
	}
}


static void draw_rectangle_rgb16(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    /* Erm. It seems that the gray color is now 0xce98. Weird */
	    if (c != 0xffff && c != 0xd69a && (c & ~1) != 0xce98)
		XPutPixel(ximage, i, j, c >> 1);
	}
}


static void erase_rectangle_rgb16(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    if (c != 0xffff && c != 0xd69a && (c & ~1) != 0xce98)
		XPutPixel(ximage, i, j, c << 1);
	}
}


static void draw_rectangle_rgb15(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    if (c != 0x7fff && c != 0x6b5a)
		XPutPixel(ximage, i, j, c >> 1);
	}
}


static void erase_rectangle_rgb15(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    if (c != 0x7fff && c != 0x6b5a)
		XPutPixel(ximage, i, j, c << 1);
	}
}


static void draw_rectangle_indexed(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    XPutPixel(ximage, i, j, pmap[c]);
	}
}


static void erase_rectangle_indexed(int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel(ximage, i, j);
	    XPutPixel(ximage, i, j, pmap[c]);
	}
}


inline void get_rectangle(int x, int y, int w, int h, int *buf)
{
    register int i, j;
    
    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;)
	    *buf++ = XGetPixel(ximage, i, j);
}


inline void put_rectangle(int x, int y, int w, int h, int *buf)
{
    register int i, j;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;)
	    XPutPixel(ximage, i, j, *buf++);
}


int writemsg(struct font_header *f, int x, int y, char *s, int c, int b)
{
    int w, x1 = 0, y1 = 0, i;
    char *p;

    if (!*s)
	return 0;

    for (; *s; s++, x1++) {
	for (w = 0; *((p = f->map[f->index[(int) *s] + w])); w++) {
	    for (; *p; x1++) {
		for (y1 = 0; *p; p++, y1++) {
		    if (c >= 0) {
			i = XGetPixel(ximage, x + x1, y - y1);
			if ((*p == '#') && (i != c)) {
			    XPutPixel(ximage, x + x1, y - y1, color[c].pixel);
			} else if ((b != -1) && (*p != '#')) {
			    XPutPixel(ximage, x + x1, y - y1, color[b].pixel);
			}
		    }
		}
		if ((b != -1) && (c != -1)) {
		    for (; y1 < f->h; y1++)
			if ((i = XGetPixel(ximage, x + x1, y - y1)) != b)
			    XPutPixel(ximage, x + x1, y - y1, color[b].pixel);
		}
	    }
	    for (y1 = 0; (b != -1) && (c != -1) && (y1 < f->h); y1++)
		if ((i = XGetPixel(ximage, x + x1, y - y1)) != b)
		    XPutPixel(ximage, x + x1, y - y1, color[b].pixel);
	}
    }

    return x1;
}


void draw_xpm(char **bg, int w, int h)
{
    int i, j, k;

    for (i = 0; i < h; i++) {
	for (j = 0; j < w; j++) {
	    switch (k = bg[9 + i][j]) {
	    case '.':
		k = 4;
		break;
	    case '#':
		k = 5;
		break;
	    default:
		k = k - 'a' + 6;
	    }
	    XPutPixel(ximage, j, i, color[k].pixel);
	}
    }
}


void setpalette(char **bg)
{
    int i;
    unsigned long rgb;

    color[0].red = color[0].green = color[0].blue = 0x02;
    color[1].red = color[1].green = color[1].blue = 0xfe;
    color[2].red = color[2].green = color[2].blue = 0xd0;

    for (i = 4; i < 12; i++) {
	rgb = strtoul (&bg[i - 3][5], NULL, 16);
	color[i].red = (rgb & mask_r) >> 16;
	color[i].green = (rgb & mask_g) >> 8;
	color[i].blue = rgb & mask_b;
	color[i + 8].red = color[i].red >> 1;
	color[i + 8].green = color[i].green >> 1;
	color[i + 8].blue = color[i].blue >> 1;
    }

    for (i = 0; i < 20; i++) {
	color[i].red <<= 8;
	color[i].green <<= 8;
	color[i].blue <<= 8;
	if (!alloc_color(display, colormap, &color[i]))
	    fprintf(stderr, "cannot allocte color cell\n");
    }

    if (indexed) {
	for (i = 0; i < 3; i++)
	    pmap[color[i].pixel] = color[i].pixel;
	for (i = 4; i < 12; i++)
	    pmap[color[i].pixel] = color[i + 8].pixel;
	for (i = 12; i < 20; i++)
	    pmap[color[i].pixel] = color[i - 8].pixel;
    }
}
