/* xmdp Copyright (C) 1997-2009 Claudio Matsuoka and Hipolito Carraro Jr
 * Original MDP.EXE for DOS by Future Crew
 *
 * 1.2.0: Ported to xmp 3.0
 * 1.1.0: Raw keys handled by Russel Marks' rawkey functions
 *        Links to libxmp 1.1.0, shm stuff removed
 * 1.0.2: execlp() bug fixed by Takashi Iwai
 * 1.0.1: X shared memory allocation test fixed
 * 1.0.0: public release
 * 0.0.1: X11 supported hacked into the code.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define USE_X11

#ifdef USE_X11
#   undef RAW_KEYS
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <X11/extensions/XShm.h>
#else /* SVGAlib */
#   include <vga.h>
#endif

#include "xmp.h"
#include "mdp.h"
#include "font.h"


#define rightmsg(f,x,y,s,c,b)						\
    writemsg (f, (x) - writemsg (f,0,0,s,-1,0), y, s, c, b)

#define update_counter(a,b,y) {						\
    if ((a) != (b)) {							\
	sprintf (buf, "%d  ", (int)(a));				\
	writemsg (&font2, 590, y, buf, 14, 9);				\
	b = a;								\
    }									\
}

extern char *mdp_font1[];
extern char *mdp_font2[];
extern struct font_header font1;
extern struct font_header font2;

static struct channel_info *ci;
static int shmid;
static pid_t pid;
static struct xmp_module_info mi;

#ifdef USE_X11
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
static XColor color[16];
static unsigned long __color;
#endif

static int palette[] =
{
    0x00, 0x00, 0x00,	/*  0 */	0x3f, 0x3f, 0x25,	/*  1 */
    0x3f, 0x3b, 0x12,	/*  2 */	0x3f, 0x34, 0x00,	/*  3 */
    0x3f, 0x29, 0x00,	/*  4 */	0x3f, 0x1f, 0x00,	/*  5 */
    0x3f, 0x14, 0x00,	/*  6 */	0x3f, 0x00, 0x00,	/*  7 */
    0x0d, 0x0d, 0x0d,	/*  8 */	0x0b, 0x10, 0x15,	/*  9 */
    0x0b, 0x15, 0x1a,	/* 10 */	0x05, 0x08, 0x10,	/* 11 */
    0xd0, 0x00, 0x00,	/* 12 */	0x10, 0x14, 0x32,	/* 13 */
    0x10, 0x18, 0x20,	/* 14 */	0xff, 0xff, 0xff	/* 15 */
};

static xmp_context ctx;

void signal_handler ()
{
#ifdef RAW_KEYS
    printf("Switching to normal keyboard mode\n");
    rawmode_exit();
#endif
#ifdef USE_X11
    XShmDetach(display,&shminfo);
    XDestroyImage(ximage);
    shmctl(shminfo.shmid,IPC_RMID,NULL);
    XCloseDisplay(display);
    shmdt(shminfo.shmaddr);
#else
    vga_setmode(TEXT);
#endif

    kill (pid, SIGTERM);
    waitpid (pid, NULL, 0);

    shmctl (shmid, IPC_RMID, NULL);
    shmdt ((char *)ci);

    exit(0);
}


#ifdef USE_X11

/* This function is from Russel Marks' ZX-81 emulator */
void notify (int *argc, char **argv)
{
    XWMHints hints;
    XSizeHints sizehints;
    XClassHint classhint;
    XTextProperty appname, iconname;
    char *apptext = "xmdp";
    char *icontext = "xmdp";

    XStringListToTextProperty (&apptext, 1, &appname);
    XStringListToTextProperty (&icontext, 1, &iconname);
    sizehints.flags = PSize | PMinSize | PMaxSize;
    sizehints.min_width = sizehints.max_width = 640;
    sizehints.min_height = sizehints.max_height = 480;
    hints.flags = StateHint | InputHint;
    hints.initial_state = NormalState;
    hints.input = 1;
    classhint.res_name = "xmpd";
    classhint.res_class = "Xmdp";
    XSetWMProperties (display, window, &appname, &iconname, argv,
	*argc, &sizehints, &hints, &classhint);
}


void drawhline (int x, int y, int w)
{
    int i;

    for (i = 0; i < w; i++)
	XPutPixel (ximage, x + i, y, __color);
}


void drawvline (int x, int y, int h)
{
    int i;

    for (i = 0; i < h; i++)
	XPutPixel (ximage, x, y + i, __color);
}

#endif /* USE_X11 */


void setpalette ()
{
    int i;

    for (i = 0; i < 16; i++) {
#ifdef USE_X11
	color[i].red = palette[i * 3] << 10;
	color[i].green = palette[i * 3 + 1] << 10;
	color[i].blue = palette[i * 3 + 2] << 10;
	XAllocColor (display, colormap, &color[i]);
#else
	vga_setpalette (i, palette[i*3], palette[i*3 + 1], palette[i*3 + 2]);
#endif
    }
}


void prepare_screen ()
{
    int i, j, x, y;

    setcolor (10);
    drawhline (0, 0, 640);
    drawhline (0, 1, 640);
    setcolor (9);
    for (i = 2; i < 72; i++)
	drawhline (0, i, 640);
    setcolor (11);
    drawhline (0, 72, 640);
    drawhline (0, 73, 640);
    setcolor (8);
    for (i = 0; i < 39; i++) {
	x = i * 16 + 15;
	drawvline (x, 85, 386);
	for (j = 0; j < 17; j++) {
	    y = 470 - j * 24;
	    drawhline (x - 1, y, 3);
	    if (!(j % 2))
		drawhline (x - 1, y - 1, 3);
	}
    }
    setcolor (10);
    drawhline (10, 65, 129);
    drawvline (138, 57, 9);
    setcolor (11);
    drawhline (10, 57, 129);
    drawvline (10, 57, 9);
}


void draw_lines (int i, int a, int b, int c)
{
#ifdef USE_X11
    int y = a;
#endif

    setcolor (c);
    i = i * 16 + 1;
    for (; a < b; a++)
	drawhline (i, a, 13);
#ifdef USE_X11
    XShmPutImage (display, window, gc, ximage, i, y, i, y, 13, a - y + 1, 0);
#endif
}


void draw_bars ()
{
    int p, v, i, y0, y1, y2, y3, k;

    for (i = 0; i < 40; i++) {
	y2 = ci[i].y2;
	y3 = ci[i].y3;

	if ((v = ci[i].vol))
	    ci[i].vol--;

	if (ci[i].timer && !--ci[i].timer) {
	    draw_lines (i, y2, y3, 0);
	    ci[i].y2 = ci[i].y3 = ci[i].note = 0;
	    continue;
	}

	if (!ci[i].note)
	    continue;

	p = 470 - ci[i].note * 4 - ci[i].bend;

	if (p < 86)
	    p = 86;
	if (p > 470)
	    p = 470;
	v++;

	ci[i].y2 = y0 = p - v;
	ci[i].y3 = y1 = p + v;
	k = abs (((i + 10) % 12) - 6) + 1;

	if ((y0 == y2) && (y1 == y3))
	    continue;

	if ((y1 < y2) || (y3 < y0)) {
	    draw_lines (i, y2, y3, 0);
	    draw_lines (i, y0, y1, k);
	    continue;
	}

	if (y1 < y3)
	    draw_lines (i, y1, y3, 0);
	if (y2 < y0)
	    draw_lines (i, y2, y0, 0);
	if (y0 < y2)
	    draw_lines (i, y0, y2, k);
	if (y3 < y1)
	    draw_lines (i, y3, y1, k);
    }
}


void draw_progress (int pos)
{
    static int i, oldpos = 0;

    if (pos == oldpos)
	return;
    if (pos > oldpos) {
	setcolor (13);
	for (i = oldpos + 1; i <= pos; i++)
	    drawvline (11 + i, 58, 7);
    }
    if (pos < oldpos) {
	setcolor (9);
	for (i = pos + 1; i <= oldpos; i++)
	    drawvline (11 + i, 58, 7);
    }
    oldpos = pos;
#ifdef USE_X11
    XShmPutImage (display, window, gc, ximage, 11, 58, 11, 58, 127, 7, 0);
#endif
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
			i = getpixel (x + x1, y - y1);
			if ((*p == '#') && (i != c)) {
			    setcolor (c);
			    drawpixel (x + x1, y - y1);
			} else if ((b != -1) && (*p != '#')) {
			    setcolor (b);
			    drawpixel (x + x1, y - y1);
			}
		    }
		}
		if ((b != -1) && (c != -1)) {
		    setcolor (b);
		    for (; y1 < f->h; y1++)
			if ((i = getpixel (x + x1, y - y1)) != b)
			    drawpixel (x + x1, y - y1);
		}
	    }
	    for (y1 = 0; (b != -1) && (c != -1) && (y1 < f->h); y1++)
		if ((i = getpixel (x + x1, y - y1)) != b)
		    drawpixel (x + x1, y - y1);
	}
    }

#ifdef USE_X11
    XShmPutImage (display, window, gc, ximage, x, y - y1, x, y - y1, x1, y1, 0);
#endif

    return x1;
}


void shadowmsg (struct font_header *f, int x, int y, char *s, int c, int b)
{
    writemsg (f, x + 2, y + 2, s, 0, b);
    writemsg (f, x, y, s, c, b);
}


void process_events ()
{
#ifdef USE_X11
    static XEvent event;
#else
    int i;
#endif

#ifdef USE_X11
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
	    switch (event.xkey.keycode) {
#else
    switch (i = GET_KEY ()) {
#endif
    case KEY_QUIT:
	xmp_stop_module(ctx);
	break;
#if 0
    case KEY_PAUSE:
	xmp_pause_module ();
	break;
#endif
    case KEY_LEFT:
	xmp_ord_prev(ctx);
	break;
    case KEY_RIGHT:
	xmp_ord_next(ctx);
	break;
    case KEY_MINUS:
#if defined(USE_X11)||defined(RAW_KEYS)
    case KEY_MINUS2:
#endif
	xmp_gvol_dec(ctx);
	break;
    case KEY_PLUS:
#if defined(USE_X11)||defined(RAW_KEYS)
    case KEY_PLUS2:
#endif
	xmp_gvol_inc(ctx);
	break;
    }
#ifdef USE_X11
	    break;
	} /* switch */
    } /* while */
#endif
}


static void event_callback(unsigned long i)
{
    static int ord = -1, pat = -1, row = -1, gvl = -1, ins = 0;
    static int global_vol;
    static char buf[80];
    long msg = i >> 4;

    switch (i & 0xf) {
    case XMP_ECHO_ORD:
	update_counter (msg & 0xff, ord, 20);
	update_counter (msg >> 8, pat, 36);
	break;
    case XMP_ECHO_GVL:
	global_vol = msg & 0xff;
	break;
    case XMP_ECHO_ROW:
	update_counter (msg & 0xff, row, 52);
	update_counter (global_vol, gvl, 68); 
	draw_progress (ord * 126 / mi.len +
	    (msg & 0xff) * 126 / mi.len / (msg >> 8));
#ifdef USE_X11
	process_events ();
	XFlush (display);
#endif
	break;
    case XMP_ECHO_INS:
	ins = msg &0xff;
	if (ins < 40)
	    ci[ins].note = msg >> 8;
	break;
    case XMP_ECHO_PBD:
	if (ins < 40)
	    ci[ins].bend = msg / 25;
	break;
    case XMP_ECHO_VOL:
	if (ins < 40) {
            ci[ins].vol = msg & 0xff;
	    ci[ins].timer = MAX_TIMER / 30;
            if (ci[ins].vol > 8)
		ci[ins].vol = 8;
	}
	break;
    }
}


void usage ()
{
    printf ("Usage: %s [-v] [-r refresh_rate] module\n", MDP);
}


int main (int argc, char **argv)
{
    int o, rate;
    struct xmp_options opt;

    init_drivers();
    ctx = xmp_create_context();
    xmp_init(ctx, argc, argv);

    while ((o = getopt (argc, argv, "vr:")) != -1) {
	switch (o) {
	case 'v':
	    printf ("Version %s\n", VERSION);
	    exit (0);
	case 'r':
	    rate = atoi (optarg);
	    break;
	default:
	    exit (-1);
	}
    }

    if (!argv[optind]) {
	usage ();
	exit (-1);
    }

    xmp_open_audio(ctx);

    {
        int srate, res, chn, itpt;

        xmp_get_driver_cfg(ctx, &srate, &res, &chn, &itpt);
        fprintf (stderr, "Using %s\n", xmp_get_driver_description(ctx));
        if (srate) {
            fprintf (stderr, "Mixer set to %dbit, %d Hz, %s%s\n", res, srate,
                itpt ? "interpolated " : "", chn > 1 ? "stereo" : "mono");
        }
    }

#ifdef USE_X11
    if ((display = XOpenDisplay (NULL)) == NULL) {
	printf ("No connection to server - aborting\n");
	exit (-1);
    }

    screen = DefaultScreen (display);
    scrptr = DefaultScreenOfDisplay (display);
    visual = DefaultVisual (display, screen);
    root = DefaultRootWindow (display);
    depth = DefaultDepth (display, screen);
    colormap = DefaultColormap (display, screen);
    attribute_mask = CWEventMask;
    attributes.event_mask |= ExposureMask | KeyPressMask;
    window = XCreateWindow (display, root, 0, 0, 640, 480, 1,
	DefaultDepthOfScreen (scrptr), InputOutput, CopyFromParent,
	attribute_mask, &attributes);

    if (!window) {
	printf ("Can't create window!\n");
	exit (-1);
    }

    notify (&argc, argv);
    gc = XCreateGC (display, window, 0, NULL);
    ximage = XShmCreateImage (display, visual, depth, ZPixmap,
	NULL, &shminfo, 640, 480);

    if (!ximage) {
	printf ("Can't create image!\n");
	exit (-1);
    }

    shminfo.shmid = shmget (IPC_PRIVATE, ximage->bytes_per_line *
	(ximage->height + 1), IPC_CREAT | 0600);

    if (shminfo.shmid == -1) {
	printf ("Can't allocate X shared memory!\n");
	exit (-1);
    }

    shminfo.shmaddr = ximage->data = shmat (shminfo.shmid, 0, 0);
    shminfo.readOnly = 0;
    XShmAttach (display, &shminfo);

    XMapWindow (display, window);
    XSetWindowBackground (display, window, BlackPixel (display, screen));
    XClearWindow (display, window);
    XFlush (display);
#else
    vga_init ();
    vga_screenoff ();
    vga_setmode (VGAMODE);
#endif

    shmid = shmget (IPC_PRIVATE, 40 * sizeof (struct channel_info),
	IPC_CREAT | 0600);

    if (shmid < 0) {
        fprintf (stderr, "Cannot allocate shared memory");
        exit (-1);
    }

    ci = (struct channel_info*)shmat (shmid, 0, 0);

    if ((int)ci == -1) {
        fprintf (stderr, "Error attaching shared memory segment\n");
        exit (-1);
    }

    signal (SIGSEGV, signal_handler);
    signal (SIGQUIT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGFPE, signal_handler);
    signal (SIGINT, signal_handler);

    setpalette ();
    prepare_screen ();

    /* Pipc->pctl.gvl = 64; */
    rightmsg (&font2, 500, 36, "Spacebar to pause", 14, -1);
    rightmsg (&font2, 500, 52, "+/- to change volume", 14, -1);
#if(defined RAW_KEYS)||(defined USE_X11)
    rightmsg (&font2, 500, 20, "F10 to quit", 14, -1);
    rightmsg (&font2, 500, 68, "Arrows to change pattern", 14, -1);
#else
    rightmsg (&font2, 500, 10, "q to quit", 14, -1);
    rightmsg (&font2, 500, 68, "j/k to change pattern", 14, -1);
#endif
    writemsg (&font2, 540, 20, "Ord", 14, -1);
    writemsg (&font2, 540, 36, "Pat", 14, -1);
    writemsg (&font2, 540, 52, "Row", 14, -1);
    writemsg (&font2, 540, 68, "Vol", 14, -1);
    writemsg (&font2, 580, 20, ":", 14, -1);
    writemsg (&font2, 580, 36, ":", 14, -1);
    writemsg (&font2, 580, 52, ":", 14, -1);
    writemsg (&font2, 580, 68, ":", 14, -1);

#ifdef USE_X11
    XShmPutImage (display, window, gc, ximage, 0, 0, 0, 0, 640, 480, 0);
#else
    vga_screenon ();
#endif

#ifdef RAW_KEYS
    printf ("Switching to raw keyboard mode\n");
    rawmode_init ();
#endif

    xmp_register_event_callback(ctx, event_callback);

    if (!(pid = fork ())) {
	while (42) {
	    usleep (50000);
	    draw_bars ();
#ifdef USE_X11
	    XFlush (display);
#endif
	}
    }

    switch (xmp_load_module(ctx, argv[optind])) {
    case -1:
	fprintf (stderr, "%s: %s: unrecognized file format\n",
	    argv[0], argv[optind]);
        break;
    case -2:
	fprintf (stderr, "%s: %s: possibly corrupted file\n",
	    argv[0], argv[optind]);
	break;
    case -3: {
	char *line;
	line = malloc (strlen (argv[0]) + strlen (argv[optind]) + 10);
	sprintf (line, "%s: %s", argv[0], argv[optind]);
	perror (line);
	break; }
    default:
	xmp_get_module_info(ctx, &mi);
	shadowmsg (&font1, 10, 26, mi.name, 15, -1);
	xmp_play_module(ctx);
	break;
    }

    xmp_close_audio(ctx);

    signal_handler ();

    return 0;
}
