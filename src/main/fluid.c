/*
 * From surf.asm by Arturo R. Montesinos <arami@fi.upm.es>. This effect was
 * originally used in Iguana's Heartquake demo end credits.
 *
 * $Id: fluid.c,v 1.1 2001-06-02 20:27:53 cmatsuoka Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/*
 * UpdateTable : performs one integration step on U[CT]
 *
 * Differential equation is:  u  = a^2( u  + u  )
 *                             tt        xx   yy
 *
 * Where a^2 = tension * gravity / surface_density.
 *
 * Aproximating second derivatives by central differences:
 *
 *  [ u(t+1)-2u(t)+u(t-1) ] / dt^2 = a^2 (u(x+1)+u(x-1)+u(y+1)+u(y-1)-4u) / h^2
 *
 * (where dt = time step, h=dx=dy = mesh resolution
 *
 * From where u(t+1) may be calculated as:
 *
 *                      [   1   ] 
 * u(t+1) = a^2dt^2/h^2 [ 1 0 1 ] u - u(t-1) + (2-4a^2dt^2/h^2)u
 *                      [   1   ] 
 *
 * When a^2dt^2/h^2 = (1/2) last term vanishes, giving:
 *
 *                      [   1   ]
 *       u(t+1) = (1/2) [ 1 0 1 ] - u(t-1)
 *                      [   1   ] 
 *
 * This needs only 4 ADD/SUB and one SAR operation per mesh point!
 *
 * (note that u(t-1,x,y) is only used to calculate u(t+1,x,y) so
 *  we can use the same array for both t-1 and t+1, needing only
 *  two arrays, U[0] and U[1])
 *
 * Dampening is simulated by subtracting 1/2^n of result.
 * n=4 gives best-looking result
 * n<4 (eg 2 or 3) thicker consistency, waves die out immediately
 * n>4 (eg 8 or 12) more fluid, waves die out slowly
 */


static int width, height;
static short *U[2];
static XImage *buffer;
static int map = 0;


static void blob (int w, int h)
{
	int x, y, c;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			c = XGetPixel (buffer, x, y);
			if (c == 0xffff || c == 0xfefefe || c == 0x7fff)
				U[map][h * x + y] = 20;
		}
	}
}


static void drop (int map, int x, int y, int ph)
{
	int h = height;

	U[map][h * x + y] = ph;
	U[map][h * x + (y + 1)] = U[map][h * x + (y - 1)] =
	U[map][h * (x + 1) + y] = U[map][h * (x - 1) + y] = ph >> 1;
}


/*
 * vimg: visible image
 * bimg: {buffer,background} image
 *    U: Z map
 */

void fluid_main (XImage *vimg, XImage *bimg)
{
	int i, x, y, h = height, w = width;
	static double ang = 0;
	static int density = 4, ph = 400;
	static int mode = 0;
	int b = vimg->bytes_per_line / w;

	buffer = bimg;
	for (y = 0; y < h; y++) {
		int yw = y * w;
		int xx, yy, xywb, xxyywb;
		int d = y;
		for (x = 0; x < w; x++) {
			/* U is transposed to save a few multiplications.
			 * It also allows waves to wrap top/bottom edges
			 */
			d += h;
			xx = x + ((U[map][d] - U[map][d + h]) >> 3);
			yy = y + ((U[map][d] - U[map][d + 1]) >> 3);

			if (xx < 0) xx = 0;
			if (yy < 0) yy = 0;
			if (xx >= w) xx = w - 1;	
			if (yy >= h) yy = h - 1;	

			/* XPutPixel (vimg, x, y, XGetPixel (bimg, xx, yy)); */

			xywb = (x + yw) * b;
			xxyywb = (xx + yy * w) * b;

			vimg->data[xywb] = bimg->data[xxyywb];
			if (b > 1) {
				vimg->data[xywb + 1] = bimg->data[xxyywb + 1];
			}
			if (b > 2) {
				vimg->data[xywb + 2] = bimg->data[xxyywb + 2];
			}
		}
	}

	i = h + 1;
	for (x = 1; x < w - 1; x++) {
		for(y = 1; y < h - 1; y++, i++) {
			int n = map ^ 1;
			U[n][i] = ((U[map][i + h] + U[map][i - h] +
				U[map][i + 1] + U[map][i - 1] ) >> 1) - U[n][i];
			U[n][i]= U[n][i] - (U[n][i] >> density);
		}
	}

	/* Effects from fluid by pix/nemesis */
	if (mode == 0) {
		x = (int)(w / 2 + (w / 2 - 4) * cos (ang));
		y = (int)(h / 2 + (h / 2 - 4) * sin (ang * 1.2 + 1));
		drop (map, x, y, ph);
		ang += 0.04;
	} else if (mode == 1) {
		x = (int)(w / 2 + (w / 2 - 4) * cos (ang * 1.3));
		y = (int)(h / 2 + (h / 2 - 4) * sin (ang * 4.7));
		drop (map, x, y, ph);
		ang += 0.012;
	} else if (mode == 2) {
		x = 1 + rand () % (w - 2);
		y = 1 + rand () % (h - 2);
		drop (map, x, y, rand() % ph);
	}

	if ((rand() % 250) == 42) {
		blob (w, h);
		mode = rand() % 3;
	}

	map ^= 1;
	usleep (2000);
}


void fluid_init (int w, int h)
{
	width = w;
	height = h;

	srand (getpid ());

	U[0] = calloc (sizeof(short), (width + 2) * height);
	U[1] = calloc (sizeof(short), (width + 2) * height);

	if (!U[0] || !U[1]) {
		fprintf (stderr, "insufficient memory for height buffers\n");
		exit (-1);
	}
}
