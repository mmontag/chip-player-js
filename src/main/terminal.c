#include <stdio.h>
#include <termios.h>
#include "terminal.h"

static int background = 0;
static struct termios term;

int set_tty()
{
	struct termios t;

	if (background)
		return -1;

	if (tcgetattr(0, &term) < 0)
		return -1;

	t = term;
	t.c_lflag &= ~(ECHO | ICANON | TOSTOP);
	t.c_cc[VMIN] = t.c_cc[VTIME] = 0;

	if (tcsetattr(0, TCSAFLUSH, &t) < 0)
		return -1;

	return 0;
}

int reset_tty()
{
	if (background)
		return -1;

	if (tcsetattr(0, TCSAFLUSH, &term) < 0) {
		fprintf(stderr, "can't reset terminal!\n");
		return -1;
	}

	return 0;
}

