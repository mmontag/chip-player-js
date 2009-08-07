/* xmdp Copyright (C) 1997-2009 Claudio Matsuoka and Hipolito Carraro Jr
 * Original MDP.EXE for DOS by Future Crew
 *
 * 1.2.0: Ported to xmp 3.0 and sdl
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
#include <SDL/SDL.h>
#include "mdp.h"
#include "xmp.h"
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

static SDL_Surface *screen;
static struct channel_info ci[40];
static struct xmp_module_info mi;

static int palette[] = {
    0x00, 0x00, 0x00,	/*  0 */	0x3f, 0x3f, 0x25,	/*  1 */
    0x3f, 0x3b, 0x12,	/*  2 */	0x3f, 0x34, 0x00,	/*  3 */
    0x3f, 0x29, 0x00,	/*  4 */	0x3f, 0x1f, 0x00,	/*  5 */
    0x3f, 0x14, 0x00,	/*  6 */	0x3f, 0x00, 0x00,	/*  7 */
    0x0d, 0x0d, 0x0d,	/*  8 */	0x0b, 0x10, 0x15,	/*  9 */
    0x0b, 0x15, 0x1a,	/* 10 */	0x05, 0x08, 0x10,	/* 11 */
    0xd0, 0x00, 0x00,	/* 12 */	0x10, 0x14, 0x32,	/* 13 */
    0x10, 0x18, 0x20,	/* 14 */	0xff, 0xff, 0xff	/* 15 */
};

static SDL_Color color[16];
static Uint32 mapped_color[16];
static int __color;
static xmp_context ctx;

static inline void setcolor(int c)
{
	__color = c;
}

static inline void put_pixel(int x, int y, int c)
{
	Uint32 pixel;
	Uint8 *bits, bpp;

	pixel = mapped_color[c];

	bpp = screen->format->BytesPerPixel;
	bits = ((Uint8 *) screen->pixels) + y * screen->pitch + x * bpp;

	/* Set the pixel */
	switch (bpp) {
	case 1:
		*(Uint8 *)(bits) = pixel;
		break;
	case 2:
		*((Uint16 *) (bits)) = (Uint16) pixel;
		break;
	case 3:{
		Uint8 r, g, b;
		r = (pixel >> screen->format->Rshift) & 0xff;
		g = (pixel >> screen->format->Gshift) & 0xff;
		b = (pixel >> screen->format->Bshift) & 0xff;
		*((bits) + screen->format->Rshift / 8) = r;
		*((bits) + screen->format->Gshift / 8) = g;
		*((bits) + screen->format->Bshift / 8) = b;
		}
		break;
	case 4:
		*((Uint32 *)(bits)) = (Uint32)pixel;
		break;
	}
}

void drawhline (int x, int y, int w)
{
    int i;

    for (i = 0; i < w; i++)
	put_pixel(x + i, y, __color);
}


void drawvline (int x, int y, int h)
{
    int i;

    for (i = 0; i < h; i++)
	put_pixel(x, y + i, __color);
}


void drawpixel(int x, int y)
{
    put_pixel(x, y, __color);
}


int init_video()
{
	int i, mode;
    
	if (SDL_Init (SDL_INIT_VIDEO /*| SDL_INIT_AUDIO*/) < 0) {
		fprintf (stderr, "sdl: can't initialize: %s\n",
			SDL_GetError ());
		return -1;
	}

	mode = SDL_SWSURFACE | SDL_ANYFORMAT;

#if 0
	if (opt.fullscreen)
		mode |= SDL_FULLSCREEN;
#endif

	if ((screen = SDL_SetVideoMode (640, 480, 8, mode)) == NULL) {
		fprintf (stderr, "sdl: can't set video mode: %s\n",
			SDL_GetError ());
		return -1;
	}
	atexit(SDL_Quit);

	SDL_WM_SetCaption("xmdp", "xmdp");

	for (i = 0; i < 16; i++) {
		color[i].r = palette[i * 3] << 2;
		color[i].g = palette[i * 3 + 1] << 2;
		color[i].b = palette[i * 3 + 2] << 2;
		mapped_color[i] = SDL_MapRGB(screen->format,
			color[i].r, color[i].g, color[i].b);
	}
	SDL_SetColors(screen, color, 0, 16);

	return 0;
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
    int y = a;

    setcolor (c);
    i = i * 16 + 1;
    for (; a < b; a++)
	drawhline (i, a, 13);
    SDL_UpdateRect(screen, i, y, 13, a - y + 1);
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
    SDL_UpdateRect(screen, 11, 58, 127, 7);
}


int writemsg (struct font_header *f, int x, int y, char *s, int c, int b)
{
    int w, x1 = 0, y1 = 0;
    char *p;

    if (!*s)
	return 0;

    for (; *s; s++, x1++) {
	for (w = 0; *((p = f->map[f->index[(int) *s] + w])); w++) {
	    for (; *p; x1++) {
		for (y1 = 0; *p; p++, y1++) {
		    if (c >= 0) {
			if ((*p == '#')) {
			    setcolor (c);
			    drawpixel(x + x1, y - y1);
			} else if ((b != -1) && (*p != '#')) {
			    setcolor (b);
			    drawpixel(x + x1, y - y1);
			}
		    }
		}
		if ((b != -1) && (c != -1)) {
		    setcolor (b);
		    for (; y1 < f->h; y1++)
			drawpixel(x + x1, y - y1);
		}
	    }
	    for (y1 = 0; (b != -1) && (c != -1) && (y1 < f->h); y1++)
		drawpixel(x + x1, y - y1);
	}
    }

    if (c != -1)
	SDL_UpdateRect(screen, x, y - f->h + 1, x1, f->h);

    return x1;
}


void shadowmsg (struct font_header *f, int x, int y, char *s, int c, int b)
{
    writemsg (f, x + 2, y + 2, s, 0, b);
    writemsg (f, x, y, s, c, b);
}


void process_events ()
{
    SDL_Event event;

    while (SDL_PollEvent (&event) > 0) {
	if (event.type == SDL_KEYDOWN) {
	    switch ((int)event.key.keysym.sym) {
	    case SDLK_F10:
	    case SDLK_ESCAPE:
		xmp_stop_module(ctx);
		break;
#if 0
	    case SDLK_SPACE:
		xmp_pause_module(ctx);
		break;
#endif
	    case SDLK_LEFT:
		xmp_ord_prev(ctx);
		break;
	    case SDLK_RIGHT:
		xmp_ord_next(ctx);
		break;
	    case SDLK_MINUS:
		xmp_gvol_dec(ctx);
		break;
	    case SDLK_PLUS:
		xmp_gvol_inc(ctx);
		break;
	    }
	}
    }
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
	process_events ();
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
    printf ("Usage: xmdp [-v] [-r refresh_rate] module\n");
}


int main (int argc, char **argv)
{
    int o;
    struct xmp_options opt;

    init_drivers();
    ctx = xmp_create_context();
    xmp_init(ctx, argc, argv);

    while ((o = getopt (argc, argv, "v")) != -1) {
	switch (o) {
	case 'v':
	    printf ("Version %s\n", VERSION);
	    exit (0);
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

    init_video();

#if 0
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
#endif

    prepare_screen ();

    /* Pipc->pctl.gvl = 64; */
    rightmsg (&font2, 500, 36, "Spacebar to pause", 14, -1);
    rightmsg (&font2, 500, 52, "+/- to change volume", 14, -1);
    rightmsg (&font2, 500, 20, "F10 to quit", 14, -1);
    rightmsg (&font2, 500, 68, "Arrows to change pattern", 14, -1);
    writemsg (&font2, 540, 20, "Ord", 14, -1);
    writemsg (&font2, 540, 36, "Pat", 14, -1);
    writemsg (&font2, 540, 52, "Row", 14, -1);
    writemsg (&font2, 540, 68, "Vol", 14, -1);
    writemsg (&font2, 580, 20, ":", 14, -1);
    writemsg (&font2, 580, 36, ":", 14, -1);
    writemsg (&font2, 580, 52, ":", 14, -1);
    writemsg (&font2, 580, 68, ":", 14, -1);

    SDL_UpdateRect(screen, 0, 0, 640, 480);

    xmp_register_event_callback(ctx, event_callback);

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
	shadowmsg(&font1, 10, 26, mi.name, 15, -1);

	xmp_player_start(ctx);
	while (xmp_player_frame(ctx) == 0) {
		xmp_play_buffer(ctx);
		draw_bars();
	}
	xmp_player_end(ctx);
	break;
    }

    xmp_close_audio(ctx);

    return 0;
}
