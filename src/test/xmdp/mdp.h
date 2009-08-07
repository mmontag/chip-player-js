/* mdp.h
 * Copyright (C) 1997 Claudio Matsuoka and Hipolito Carraro Jr
 */

#define MAX_TIMER 1600		/* Expire time */

struct channel_info {
    int timer;
    int vol;
    int note;
    int bend;
    int y2, y3;
};

