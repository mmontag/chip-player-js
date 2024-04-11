#ifndef __RF5C68_H__
#define __RF5C68_H__

#include "../EmuStructs.h"

extern DEV_DEF devDef_RF5C68_MAME;

typedef void (*SAMPLE_END_CB)(void* param,int channel);

void rf5c68_set_sample_end_callback(void *info, SAMPLE_END_CB callback, void* param);

#endif	// __RF5C68_H__
