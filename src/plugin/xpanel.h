/* Extended Module Player
 * Copyright (C) 1996, 1997 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include "xmp.h"

#define RES_X 300
#define RES_Y 128

struct font_header {
    int h;
    int *index;
    char **map;
};

struct ipc_info {
    struct xmp_module_info mi;
    char filename[80];
    int vol[32];
    int mute[32];
    int progress;
    int pat;
    int row;
    int wresult;
    int pause;
    int cmd;
    int mode;
    int buffer[256];
};

int	setcolor	(int);
void	update_display	(void);
void	prepare_screen	(void);
void	clear_screen	(void);
void	close_window	(void);
void	get_rectangle	(int, int, int, int, int *);
void	put_rectangle	(int, int, int, int, int *);
void	setpalette	(char **);
void	set_palette	(void);
void	display_loop	(void);
void	panel_setup	(void);
int	panel_loop	(void);
int	writemsg	(struct font_header *, int, int, char *, int, int);
void	draw_xpm	(char **, int, int);
int	process_events	(int *, int *);
int	create_window	(char *, char *, int, int, int, char **);
void	x11_event_callback (unsigned long);


extern void	(*draw_rectangle)	(int, int, int, int);
extern void	(*erase_rectangle)	(int, int, int, int);

