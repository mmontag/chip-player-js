/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: xstuff.c,v 1.2 2002-05-30 12:10:51 cmatsuoka Exp $
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

#ifndef XMMS_PLUGIN

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>

#include "xpanel.h"

static Display *display;
static Visual *visual;
static int screen;
static Screen *scrptr;
static Colormap colormap;
static XSetWindowAttributes attributes;
static unsigned long attribute_mask;
static int depth;
static GC gc;
static XImage *ximage;
static Window window, root;
static XShmSegmentInfo shminfo;

#define alloc_color(d,c,x) XAllocColor(d,c,x)
#endif /* XMMS_PLUGIN */

static unsigned long __color;
static XColor color[20];
static int pmap[256];
static int indexed;

/* Fix for 16bpp true color visual */
static int mask_r = 0xfe0000;
static int mask_g = 0x00fe00;
static int mask_b = 0x0000fe;

void (*draw_rectangle) ();
void (*erase_rectangle) ();


int setcolor (int x)
{
    return __color = color[x].pixel;
}


static void draw_rectangle_rgb24 (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    if (c != 0xfefefe && c != 0xd0d0d0)
		XPutPixel (ximage, i, j, c >> 1);
	}
}


static void erase_rectangle_rgb24 (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    if (c != 0xfefefe && c != 0xd0d0d0)
		XPutPixel (ximage, i, j, c << 1);
	}
}


static void draw_rectangle_rgb16 (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    /* Erm. It seems that the gray color is now 0xce98. Weird */
	    if (c != 0xffff && c != 0xd69a && (c & ~1) != 0xce98)
		XPutPixel (ximage, i, j, c >> 1);
	}
}


static void erase_rectangle_rgb16 (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    if (c != 0xffff && c != 0xd69a && (c & ~1) != 0xce98)
		XPutPixel (ximage, i, j, c << 1);
	}
}


static void draw_rectangle_rgb15 (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    if (c != 0x7fff && c != 0x6b5a)
		XPutPixel (ximage, i, j, c >> 1);
	}
}


static void erase_rectangle_rgb15 (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    if (c != 0x7fff && c != 0x6b5a)
		XPutPixel (ximage, i, j, c << 1);
	}
}


static void draw_rectangle_indexed (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    XPutPixel (ximage, i, j, pmap[c]);
	}
}


static void erase_rectangle_indexed (int x, int y, int w, int h)
{
    int i, j, c;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;) {
	    c = XGetPixel (ximage, i, j);
	    XPutPixel (ximage, i, j, pmap[c]);
	}
}


inline void get_rectangle (int x, int y, int w, int h, int *buf)
{
    register int i, j;
    
    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;)
	    *buf++ = XGetPixel (ximage, i, j);
}


inline void put_rectangle (int x, int y, int w, int h, int *buf)
{
    register int i, j;

    for (i = x + w; i-- > x;)
	for (j = y + h; j-- > y;)
	    XPutPixel (ximage, i, j, *buf++);
}



int writemsg (struct font_header *f, int x, int y, char *s, int c, int b)
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
			i = XGetPixel (ximage, x + x1, y - y1);
			if ((*p == '#') && (i != c)) {
			    XPutPixel (ximage, x + x1, y - y1, color[c].pixel);
			} else if ((b != -1) && (*p != '#')) {
			    XPutPixel (ximage, x + x1, y - y1, color[b].pixel);
			}
		    }
		}
		if ((b != -1) && (c != -1)) {
		    for (; y1 < f->h; y1++)
			if ((i = XGetPixel (ximage, x + x1, y - y1)) != b)
			    XPutPixel (ximage, x + x1, y - y1, color[b].pixel);
		}
	    }
	    for (y1 = 0; (b != -1) && (c != -1) && (y1 < f->h); y1++)
		if ((i = XGetPixel (ximage, x + x1, y - y1)) != b)
		    XPutPixel (ximage, x + x1, y - y1, color[b].pixel);
	}
    }

    return x1;
}


void draw_xpm (char **bg, int w, int h)
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
	    XPutPixel (ximage, j, i, color[k].pixel);
	}
    }
}



void setpalette (char **bg)
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
	if (!alloc_color (display, colormap, &color[i]))
	    fprintf (stderr, "cannot allocte color cell\n");
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


#ifndef XMMS_PLUGIN

void putimage (int x, int y, int w, int h)
{
    XShmPutImage (display, window, gc, ximage, x, y, x, y, w, h, 0);
}


void settitle (char *title)
{
    XTextProperty titleprop;
    XStringListToTextProperty (&title,1,&titleprop);
    XSetTextProperty (display, window, &titleprop, XA_WM_NAME);
}


void update_display ()
{
    XSync (display, False);
}


void close_window ()
{
    XSync (display, False);
    XShmDetach (display, &shminfo);
    XDestroyImage (ximage);
    XCloseDisplay (display);
    shmctl (shminfo.shmid, IPC_RMID, NULL);
    shmdt (shminfo.shmaddr);
}

int process_events (int *x, int *y)
{
    static XEvent event;
    int i = 0;
    char k;

    while (XEventsQueued (display, QueuedAfterReading)) {
	XNextEvent (display, &event);
	switch (event.type) {
	case Expose:
	    XShmPutImage (display, window, gc, ximage,
		event.xexpose.x, event.xexpose.y,
		event.xexpose.x, event.xexpose.y,
		event.xexpose.width, event.xexpose.height, 0);
	    break;
	case KeyPress:
	    XLookupString (&event.xkey, &k, 1, NULL, NULL);
	    return k;
	case ButtonPress:
	    i = -1;
	    *x = event.xbutton.x;
	    *y = event.xbutton.y;
	    break;
	}
    }

    return i;
}


int create_window (char *s, char *c, int w, int h, int argc, char **argv)
{
    XWMHints hints;
    XSizeHints sizehints;
    XClassHint classhint;
    XTextProperty appname, iconname;
    char *apptext = s;
    char *icontext = s;

    if ((display = XOpenDisplay (NULL)) == NULL) {
	fprintf (stderr, "%s: can't open display: %s\n", argv[0],
		XDisplayName (NULL));
	return -1;
    }
    screen = DefaultScreen (display);
    scrptr = DefaultScreenOfDisplay (display);
    visual = DefaultVisual (display, screen);
    root = DefaultRootWindow (display);
    depth = DefaultDepth (display, screen);
    colormap = DefaultColormap (display, screen);
    attribute_mask = CWEventMask;
    attributes.event_mask |= ExposureMask | ButtonPressMask | KeyPressMask;

    if (visual->class == PseudoColor && depth == 8) {
	draw_rectangle = draw_rectangle_indexed;
	erase_rectangle = erase_rectangle_indexed;
	indexed = 1;
    } else if (visual->class == TrueColor && depth == 24) {
	draw_rectangle = draw_rectangle_rgb24;
	erase_rectangle = erase_rectangle_rgb24;
	indexed = 0;
    } else if (visual->class == TrueColor && depth == 16) {
	mask_r = 0xf00000;
	mask_g = 0x00f800;
	mask_b = 0x0000f0;
	draw_rectangle = draw_rectangle_rgb16;
	erase_rectangle = erase_rectangle_rgb16;
	indexed = 0;
    } else if (visual->class == TrueColor && depth == 15) {
	mask_r = 0xf00000;
	mask_g = 0x00f000;
	mask_b = 0x0000f0;
	draw_rectangle = draw_rectangle_rgb15;
	erase_rectangle = erase_rectangle_rgb15;
	indexed = 0;
    } else {
	fprintf (stderr, "Visual class and depth not supported, aborting\n");
	return -1;
    }

    window = XCreateWindow (display, root, 0, 0, w, h, 1,
	DefaultDepthOfScreen (scrptr), InputOutput, CopyFromParent,
	attribute_mask, &attributes);

    if (!window) {
	fprintf (stderr, "can't create window\n");
	return -1;
    }
    XStringListToTextProperty (&apptext, 1, &appname);
    XStringListToTextProperty (&icontext, 1, &iconname);
    sizehints.flags = PSize | PMinSize | PMaxSize;
    sizehints.min_width = sizehints.max_width = w;
    sizehints.min_height = sizehints.max_height = h;
    hints.flags = StateHint | InputHint;
    hints.initial_state = NormalState;
    hints.input = 1;
    classhint.res_name = s;
    classhint.res_class = c;
    XSetWMProperties (display, window, &appname, &iconname, argv,
	argc, &sizehints, &hints, &classhint);

    gc = XCreateGC (display, window, 0, NULL);

    ximage = XShmCreateImage (display, visual, depth, ZPixmap,
	NULL, &shminfo, w, h);

    if (!ximage) {
	fprintf (stderr, "can't create image\n");
	return -1;
    }
    shminfo.shmid = shmget (IPC_PRIVATE, ximage->bytes_per_line *
	(ximage->height + 1), IPC_CREAT | 0600);

    if (shminfo.shmid == -1) {
	fprintf (stderr, "can't allocate X shared memory\n");
	return -1;
    }
    shminfo.shmaddr = ximage->data = shmat (shminfo.shmid, 0, 0);
    shminfo.readOnly = 0;
    XShmAttach (display, &shminfo);

    XMapWindow (display, window);
    XSetWindowBackground (display, window, BlackPixel (display, screen));
    XClearWindow (display, window);

    XFlush (display);
    XSync (display, False);

    return 0;
}

#endif /* !XMMS_PLUGIN */
