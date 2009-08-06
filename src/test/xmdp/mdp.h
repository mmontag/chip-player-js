/* mdp.h
 * Copyright (C) 1997 Claudio Matsuoka and Hipolito Carraro Jr
 */

#include "rawkey.h"

#ifdef USE_X11
#undef RAW_KEYS
#define MDP "xmdp"
#define KEY_LEFT 100
#define KEY_RIGHT 102
#define KEY_QUIT 76
#define KEY_MINUS 20
#define KEY_MINUS2 1
#define KEY_PLUS 21
#define KEY_PLUS2 0
#else /* SVGAlib */
#include <vga.h>
#define MDP "smdp"
#ifdef RAW_KEYS
#define GET_KEY() get_scancode()
#define KEY_LEFT CURSOR_LEFT
#define KEY_RIGHT CURSOR_RIGHT
#define KEY_QUIT FUNC_KEY(10)
#define KEY_MINUS 12
#define KEY_MINUS2 GRAY_MINUS
#define KEY_PLUS 10
#define KEY_PLUS2 GRAY_PLUS
#else /* SVGAlib, no raw keys */
#define GET_KEY() vga_getkey()
#define KEY_LEFT 'k'
#define KEY_RIGHT 'j'
#define KEY_QUIT 'q'
#define KEY_MINUS '-'
#define KEY_PLUS '+'
#endif
#endif

#define VGAMODE G640x480x16
#define MAX_TIMER 1600		/* Expire time */

#ifdef USE_X11
#define drawpixel(x,y) XPutPixel(ximage,x,y,__color)
#define getpixel(x,y) XGetPixel(ximage,x,y)
#define setcolor(x) __color=color[x].pixel
#else
#define drawpixel(x,y) vga_drawpixel(x,y)
#define getpixel(x,y) vga_getpixel(x,y)
#define setcolor(x) vga_setcolor(x)
#define drawhline(x,y,w) vga_drawline(x,y,x+w-1,y)
#define drawvline(x,y,h) vga_drawline(x,y,x,y+h-1)
#endif

struct channel_info {
    int timer;
    int vol;
    int note;
    int bend;
    int y2, y3;
};


