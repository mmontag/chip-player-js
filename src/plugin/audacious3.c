/*
 * XMP plugin for XMMS/Beep/Audacious
 * Written by Claudio Matsuoka, 2000-04-30
 * Based on J. Nick Koston's MikMod plugin for XMMS
 */

/* Audacious 3.0 port/rewrite for Fedora by Michael Schwendt
 * TODO: list of supported formats missing in 'About' dialog
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>
#include <libaudgui/libaudgui-gtk.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "xmp.h"
#include "common.h"
#include "driver.h"

static GMutex *seek_mutex;
static GCond *seek_cond;
static gint jumpToTime = -1;
static gboolean stop_flag = FALSE;

static xmp_context ctx;

/* config values used for the Preferences UI */
struct {
	gboolean bits16, bits8;
	gboolean stereo, mono;
	gboolean freq44, freq22, freq11;
	gboolean fixloops, modrange, convert8bit, interpolation, filter;
	gfloat panamp;
} guicfg;

static void configure_init(void);

#define FREQ_SAMPLE_44 0
#define FREQ_SAMPLE_22 1
#define FREQ_SAMPLE_11 2

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

XMPConfig xmp_cfg;

extern struct xmp_drv_info drv_smix;


static void strip_vfs(char *s) {
	int len;
	char *c;

	if (!s) {
		return;
	}
	_D("%s", s);
	if (!memcmp(s, "file://", 7)) {
		len = strlen(s);
		memmove(s, s + 7, len - 6);
	}

	for (c = s; *c; c++) {
		if (*c == '%' && isxdigit(*(c + 1)) && isxdigit(*(c + 2))) {
			char val[3];
			val[0] = *(c + 1);
			val[1] = *(c + 2);
			val[2] = 0;
			*c++ = strtoul(val, NULL, 16);
			len = strlen(c);
			memmove(c, c + 2, len - 1);
		}
	}
}


static void stop(InputPlayback *playback)
{
	_D("*** stop!");
	g_mutex_lock(seek_mutex);
	if (!stop_flag) {
		xmp_stop_module(ctx); 
		stop_flag = TRUE;
		playback->output->abort_write();
		g_cond_signal(seek_cond);
	}
	g_mutex_unlock(seek_mutex);
}


static void mseek(InputPlayback *playback, gint msec)
{
	g_mutex_lock(seek_mutex);
	if (!stop_flag) {
		jumpToTime = msec;
		playback->output->abort_write();
		g_cond_signal(seek_cond);
		g_cond_wait(seek_cond, seek_mutex);
	}
	g_mutex_unlock(seek_mutex);
}


static void seek_ctx(gint time)
{
	int i, t;
	struct player_data *p = &((struct context_data *)ctx)->p;

	_D("seek to %ld, total %d", time, xmp_cfg.time);

	for (i = 0; i < xmp_cfg.mod_info.len; i++) {
		t = p->m.xxo_info[i].time;

		_D("%2d: %ld %d", i, time, t);

		if (t > time) {
			int a;
			if (i > 0)
				i--;
			a = xmp_ord_set(ctx, i);
			break;
		}
	}
}


static void mod_pause(InputPlayback *playback, gboolean p)
{
	g_mutex_lock(seek_mutex);
	if (!stop_flag) {
		playback->output->pause(p);
	}
	g_mutex_unlock(seek_mutex);
}


static gboolean init(void)
{
	mcs_handle_t *cfg;

	_D("Plugin init");
	xmp_drv_register(&drv_smix);
	ctx = xmp_create_context();

	jumpToTime = -1;
	seek_mutex = g_mutex_new();
	seek_cond = g_cond_new();

	xmp_cfg.mixing_freq = 0;
	xmp_cfg.convert8bit = 0;
	xmp_cfg.fixloops = 0;
	xmp_cfg.modrange = 0;
	xmp_cfg.force8bit = 0;
	xmp_cfg.force_mono = 0;
	xmp_cfg.interpolation = TRUE;
	xmp_cfg.filter = TRUE;
	xmp_cfg.pan_amplitude = 80;

#define CFGREADINT(x) aud_cfg_db_get_int (cfg, "XMP", #x, &xmp_cfg.x)

	if ((cfg = aud_cfg_db_open())) {
		CFGREADINT(mixing_freq);
		CFGREADINT(force8bit);
		CFGREADINT(convert8bit);
		CFGREADINT(modrange);
		CFGREADINT(fixloops);
		CFGREADINT(force_mono);
		CFGREADINT(interpolation);
		CFGREADINT(filter);
		CFGREADINT(pan_amplitude);

		aud_cfg_db_close(cfg);
	}

	configure_init();

	xmp_init(ctx, 0, NULL);

	return TRUE;
}


static void cleanup()
{
	g_cond_free(seek_cond);
	g_mutex_free(seek_mutex);
	xmp_free_context(ctx);
}


static int is_our_file_from_vfs(const char* _filename, VFSFile *vfsfile)
{
	gchar *filename = g_strdup(_filename);
	gboolean ret;

	_D("filename = %s", filename);
	strip_vfs(filename);		/* Sorry, no VFS support */

	ret = (xmp_test_module(ctx, filename, NULL) == 0);

	g_free(filename);
	return ret;
}


Tuple *probe_for_tuple(const gchar *_filename, VFSFile *fd)
{
	gchar *filename = g_strdup(_filename);
	xmp_context ctx;
	int len;
	Tuple *tuple;
	struct xmp_module_info mi;
	struct xmp_options *opt;

	_D("filename = %s", filename);
	strip_vfs(filename);		/* Sorry, no VFS support */

	ctx = xmp_create_context();
	opt = xmp_get_options(ctx);
	opt->skipsmp = 1;	/* don't load samples */
	len = xmp_load_module(ctx, filename);
	g_free(filename);

	if (len < 0) {
		xmp_free_context(ctx);
		return NULL;
	}

	xmp_get_module_info(ctx, &mi);

	tuple = tuple_new_from_filename(filename);
	tuple_associate_string(tuple, FIELD_TITLE, NULL, mi.name);
	tuple_associate_string(tuple, FIELD_CODEC, NULL, mi.type);
	tuple_associate_int(tuple, FIELD_LENGTH, NULL, len);

	xmp_release_module(ctx);
	xmp_free_context(ctx);

	return tuple;
}


static gboolean play(InputPlayback *ipb, const gchar *_filename, VFSFile *file, gint start_time, gint stop_time, gboolean pause)
{
	int channelcnt = 1;
	FILE *f;
	struct xmp_options *opt;
	int lret, fmt, nch;
	void *data;
	int size;
	gchar *filename = g_strdup(_filename);
	Tuple *tuple;
	
	_D("play: %s\n", filename);
	if (file == NULL) {
		return FALSE;
	}
	opt = xmp_get_options(ctx);

	strip_vfs(filename);  /* Sorry, no VFS support */

	_D("play_file: %s", filename);

	jumpToTime = (start_time > 0) ? start_time : -1;
	stop_flag = FALSE;

	if ((f = fopen(filename, "rb")) == 0) {
		goto PLAY_ERROR_1;
	}
	fclose(f);

	opt->resol = 8;
	opt->verbosity = 0;
	opt->drv_id = "smix";

	switch (xmp_cfg.mixing_freq) {
	case 1:
		opt->freq = 22050;	/* 1:2 mixing freq */
		break;
	case 2:
		opt->freq = 11025;	/* 1:4 mixing freq */
		break;
	default:
		opt->freq = 44100;	/* standard mixing freq */
		break;
	}

	if (xmp_cfg.force8bit == 0)
		opt->resol = 16;

	if (xmp_cfg.force_mono == 0) {
		channelcnt = 2;
		opt->outfmt &= ~XMP_FMT_MONO;
	} else {
		opt->outfmt |= XMP_FMT_MONO;
	}

	if (xmp_cfg.interpolation == 1)
		opt->flags |= XMP_CTL_ITPT;
	else
		opt->flags &= ~XMP_CTL_ITPT;

	if (xmp_cfg.filter == 1)
		opt->flags |= XMP_CTL_FILTER;
	else
		opt->flags &= ~XMP_CTL_FILTER;

	opt->mix = xmp_cfg.pan_amplitude;

	fmt = opt->resol == 16 ? FMT_S16_NE : FMT_U8;
	nch = opt->outfmt & XMP_FMT_MONO ? 1 : 2;
	
	if (!ipb->output->open_audio(fmt, opt->freq, nch)) {
		goto PLAY_ERROR_1;
	}

	xmp_open_audio(ctx);

	_D("*** loading: %s", filename);

	lret =  xmp_load_module(ctx, filename);
	g_free(filename);

	if (lret < 0) {
		xmp_close_audio(ctx);
		goto PLAY_ERROR_1;
	}

	xmp_cfg.time = lret;
	xmp_get_module_info(ctx, &xmp_cfg.mod_info);

	tuple = tuple_new_from_filename(filename);
	tuple_associate_string(tuple, FIELD_TITLE, NULL, xmp_cfg.mod_info.name);
	tuple_associate_string(tuple, FIELD_CODEC, NULL, xmp_cfg.mod_info.type);
	tuple_associate_int(tuple, FIELD_LENGTH, NULL, lret);
	ipb->set_tuple(ipb, tuple);

	ipb->set_params(ipb, xmp_cfg.mod_info.chn * 1000, opt->freq, channelcnt);
	ipb->set_pb_ready(ipb);

	stop_flag = FALSE;
	xmp_player_start(ctx);

	while (!stop_flag) {
		if (stop_time >= 0 && ipb->output->written_time() >= stop_time) {
			goto DRAIN;
        	}

		g_mutex_lock(seek_mutex);
		if (jumpToTime != -1) {
			seek_ctx(jumpToTime);
			ipb->output->flush(jumpToTime);
			jumpToTime = -1;
			g_cond_signal(seek_cond);
		}
		g_mutex_unlock(seek_mutex);

		xmp_get_buffer(ctx, &data, &size);

        	if (!stop_flag && jumpToTime < 0) {
			ipb->output->write_audio(data, size);
        	}

		if ((xmp_player_frame(ctx) != 0) && jumpToTime < 0) {
			stop_flag = TRUE;
 DRAIN:
			while (!stop_flag && ipb->output->buffer_playing()) {
				g_usleep(20000);
			}
			break;
		}
	}

	g_mutex_lock(seek_mutex);
	stop_flag = TRUE;
	g_cond_signal(seek_cond);  /* wake up any waiting request */
	g_mutex_unlock(seek_mutex);

	ipb->output->close_audio();
	xmp_player_end(ctx);
	xmp_release_module(ctx);
	xmp_close_audio(ctx);
	return TRUE;

 PLAY_ERROR_1:
	g_free(filename);
	return FALSE;
}


static void configure_apply()
{
	mcs_handle_t *cfg;
	struct xmp_options *opt;

	/* transfer Preferences UI config values back into XMPConfig */
	if (guicfg.freq11) {
		xmp_cfg.mixing_freq = FREQ_SAMPLE_11;
	} else if (guicfg.freq22) {
		xmp_cfg.mixing_freq = FREQ_SAMPLE_22;
	} else {  /* if (guicfg.freq44) { */
		xmp_cfg.mixing_freq = FREQ_SAMPLE_44;
	}

	xmp_cfg.convert8bit = guicfg.bits8;
	xmp_cfg.force_mono = guicfg.mono;
	xmp_cfg.fixloops = guicfg.fixloops;
	xmp_cfg.modrange = guicfg.modrange;
	xmp_cfg.interpolation = guicfg.interpolation;
	xmp_cfg.filter = guicfg.filter;
	xmp_cfg.pan_amplitude = (gint)guicfg.panamp;

	opt = xmp_get_options(ctx);
	opt->mix = xmp_cfg.pan_amplitude;

	cfg = aud_cfg_db_open();

#define CFGWRITEINT(x) aud_cfg_db_set_int (cfg, "XMP", #x, xmp_cfg.x)

	CFGWRITEINT(mixing_freq);
	CFGWRITEINT(force8bit);
	CFGWRITEINT(convert8bit);
	CFGWRITEINT(modrange);
	CFGWRITEINT(fixloops);
	CFGWRITEINT(force_mono);
	CFGWRITEINT(interpolation);
	CFGWRITEINT(filter);
	CFGWRITEINT(pan_amplitude);

	aud_cfg_db_close(cfg);
}

static void configure_init(void)
{
	/* transfer XMPConfig into Preferences UI config */
	/* keeping compatibility with older releases */
	guicfg.freq11 = (xmp_cfg.mixing_freq == FREQ_SAMPLE_11);
	guicfg.freq22 = (xmp_cfg.mixing_freq == FREQ_SAMPLE_22);
	guicfg.freq44 = (xmp_cfg.mixing_freq == FREQ_SAMPLE_44);
	guicfg.mono = xmp_cfg.force_mono;
	guicfg.stereo = !xmp_cfg.force_mono;
	guicfg.bits8 = xmp_cfg.convert8bit;
	guicfg.bits16 = !xmp_cfg.convert8bit;
	guicfg.convert8bit = xmp_cfg.convert8bit;
	guicfg.fixloops = xmp_cfg.fixloops;
	guicfg.modrange = xmp_cfg.modrange;
	guicfg.interpolation = xmp_cfg.interpolation;
	guicfg.filter = xmp_cfg.filter;
	guicfg.panamp = xmp_cfg.pan_amplitude;
}

void xmp_aud_about()
{
	static GtkWidget *about_window = NULL;

	audgui_simple_message(&about_window, GTK_MESSAGE_INFO,
                          g_strdup_printf(
                "Extended Module Player %s", VERSION),
                "Written by Claudio Matsuoka and Hipolito Carraro Jr.\n"
                "\n"
		"Audacious 3 plugin by Michael Schwendt\n"
                "\n"
                "Portions Copyright (C) 1998,2000 Olivier Lapicque,\n"
                "(C) 1998 Tammo Hinrichs, (C) 1998 Sylvain Chipaux,\n"
                "(C) 1997 Bert Jahn, (C) 1999 Tatsuyuki Satoh, (C)\n"
                "1995-1999 Arnaud Carre, (C) 2001-2006 Russell Marks,\n"
		"(C) 2005-2006 Michael Kohn\n"
                "\n"
		/* TODO: list */
		/* "Supported module formats:" */
	);
}


static PreferencesWidget prefs_precision[] = {
	{ WIDGET_RADIO_BTN, "16 bit", &guicfg.bits16,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
	{ WIDGET_RADIO_BTN, "8 bit", &guicfg.bits8,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
};

static PreferencesWidget prefs_channels[] = {
	{ WIDGET_RADIO_BTN, "Stereo", &guicfg.stereo,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
	{ WIDGET_RADIO_BTN, "Mono", &guicfg.mono,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
};

static PreferencesWidget prefs_frequency[] = {
	{ WIDGET_RADIO_BTN, "44 kHz", &guicfg.freq44,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
	{ WIDGET_RADIO_BTN, "22 kHz", &guicfg.freq22,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
	{ WIDGET_RADIO_BTN, "11 kHz", &guicfg.freq11,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
};

static PreferencesWidget prefs_opts[] = {
	{ WIDGET_CHK_BTN, "Convert 16 bit samples to 8 bit",
		&guicfg.convert8bit, NULL, NULL, FALSE,
		.cfg_type = VALUE_BOOLEAN },
	{ WIDGET_CHK_BTN, "Fix sample loops", &guicfg.fixloops,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN },
	{ WIDGET_CHK_BTN, "Force 3 octave range in standard MOD files",
		&guicfg.modrange, NULL, NULL, FALSE,
		.cfg_type = VALUE_BOOLEAN },
	{ WIDGET_CHK_BTN, "Enable 32-bit linear interpolation",
		&guicfg.interpolation, NULL, NULL, FALSE,
		.cfg_type = VALUE_BOOLEAN },
	{ WIDGET_CHK_BTN, "Enable IT filters", &guicfg.filter,
		NULL, NULL, FALSE, .cfg_type = VALUE_BOOLEAN},
	{ WIDGET_LABEL, "Pan amplitude (%)", NULL, NULL, NULL, FALSE },
	{ WIDGET_SPIN_BTN, "", &guicfg.panamp, NULL, NULL, FALSE,
		{ .spin_btn = { 0.0, 100.0, 1.0, "" } },
		.cfg_type = VALUE_FLOAT},
};

static PreferencesWidget prefs_opts_tab[] = {
	{ WIDGET_BOX, NULL, NULL, NULL, NULL, FALSE,
		{.box = { prefs_opts, G_N_ELEMENTS(prefs_opts), FALSE, FALSE}}},
};

static PreferencesWidget prefs_qual_row1[] = {
	{ WIDGET_BOX, "Resolution", NULL, NULL, NULL, FALSE,
		{ .box = { prefs_precision, G_N_ELEMENTS(prefs_precision),
			FALSE, TRUE }
		}
	},
	{ WIDGET_BOX, "Channels", NULL, NULL, NULL, FALSE,
		{ .box = { prefs_channels, G_N_ELEMENTS(prefs_channels),
			FALSE, TRUE }
		}
	},
};

static PreferencesWidget prefs_qual_row2[] = {
	{ WIDGET_BOX, "Sampling rate", NULL, NULL, NULL, FALSE,
		{ .box = { prefs_frequency, G_N_ELEMENTS(prefs_frequency),
			FALSE, TRUE }
		}
	},
};

static PreferencesWidget prefs_qual_box1[] = {
	{ WIDGET_BOX, NULL, NULL, NULL, NULL, FALSE,
		{ .box = { prefs_qual_row1, G_N_ELEMENTS(prefs_qual_row1),
			TRUE, TRUE }
		}
	},
	{ WIDGET_BOX, NULL, NULL, NULL, NULL, FALSE,
		{ .box = { prefs_qual_row2, G_N_ELEMENTS(prefs_qual_row2),
			FALSE, TRUE }
		}
	},
};

static PreferencesWidget prefs_qual_tab[] = {
	{ WIDGET_BOX, NULL, NULL, NULL, NULL, FALSE,
		{ .box = { prefs_qual_box1, G_N_ELEMENTS(prefs_qual_box1),
			FALSE, TRUE }
		}
	},
};

static NotebookTab prefs_tabs[] = {
	{ "Quality", prefs_qual_tab, G_N_ELEMENTS(prefs_qual_tab) },
	{ "Options", prefs_opts_tab, G_N_ELEMENTS(prefs_opts_tab) },
};

static PreferencesWidget prefs[] = {
	{WIDGET_NOTEBOOK, NULL, NULL, NULL, NULL, FALSE,
		{ .notebook = { prefs_tabs, G_N_ELEMENTS(prefs_tabs) } },
	},
};

PluginPreferences xmp_aud_preferences = {
#if _AUD_PLUGIN_VERSION > 20
	.domain = "xmpaudplugin",
#endif
	.title = "Extended Module Player Configuration",
	.prefs = prefs,
	.n_prefs = G_N_ELEMENTS(prefs),
	.type = PREFERENCES_WINDOW,
	.init = configure_init,
	.apply = configure_apply,
};

/* Filtering files by suffix is really stupid. */
const gchar* const fmts[] = {
	"xm", "mod", "m15", "it", "s2m", "s3m", "stm", "stx", "med", "dmf",
	"mtm", "ice", "imf", "ptm", "mdl", "ult", "liq", "psm", "amf",
        "rtm", "pt3", "tcb", "dt", "gtk", "dtt", "mgt", "digi", "dbm",
	"emod", "okt", "sfx", "far", "umx", "stim", "mtp", "ims", "669",
	"fnk", "funk", "amd", "rad", "hsc", "alm", "kris", "ksm", "unic",
	"zen", "crb", "tdd", "gmc", "gdm", "mdz", "xmz", "s3z", "j2b", NULL
};

#ifndef AUD_INPUT_PLUGIN
#define AUD_INPUT_PLUGIN(x...) InputPlugin xmp_ip = { x };
#endif

AUD_INPUT_PLUGIN (
#if _AUD_PLUGIN_VERSION > 20
	.name		= "XMP Plugin " VERSION,
#else
	.description	= "XMP Plugin " VERSION,
#endif
	.init		= init,
	.about		= xmp_aud_about,
	.settings	= &xmp_aud_preferences,
	.play		= play,
	.stop		= stop,
	.pause		= mod_pause,
	.probe_for_tuple = probe_for_tuple,
	.is_our_file_from_vfs = is_our_file_from_vfs,
	.cleanup	= cleanup,
	.mseek		= mseek,
#if _AUD_PLUGIN_VERSION > 20
	.extensions	= fmts,
#else
	.vfs_extensions	= fmts,
#endif
)

#if _AUD_PLUGIN_VERSION <= 20
static InputPlugin *xmp_iplist[] = { &xmp_ip, NULL };

SIMPLE_INPUT_PLUGIN(xmp, xmp_iplist);
#endif
