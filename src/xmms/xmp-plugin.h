/*
 *  $Id: xmp-plugin.h,v 1.1 2001-06-02 20:28:33 cmatsuoka Exp $
 */
 
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <xmms/plugin.h>
#include "xmp.h"
#ifdef __EMX__
#define PATH_MAX _POSIX_PATH_MAX
#endif

#define FREQ_SAMPLE_44 0
#define FREQ_SAMPLE_22 1
#define FREQ_SAMPLE_11 2

extern InputPlugin xmp_ip;

typedef struct {
	gint mixing_freq;
	gint force8bit;
	gint force_mono;
	gint interpolation;
	gint filter;
	gint convert8bit;
	gint fixloops;
	gint loop;
	gint modrange;
	gint pan_amplitude;
	gint time;
	struct xmp_module_info mod_info;
} XMPConfig;

extern XMPConfig xmp_cfg;

extern struct xmp_drv_info drv_xmms;

