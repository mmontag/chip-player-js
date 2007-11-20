/*
 * XMP plugin for XMMS/Beep/Audacious
 * Written by Claudio Matsuoka, 2000-04-30
 * Based on J. Nick Koston's MikMod plugin for XMMS
 *
 * $Id: audacious.c,v 1.15 2007-11-20 23:38:19 cmatsuoka Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include <audacious/configdb.h>
#include <audacious/util.h>
#include <audacious/plugin.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "xmp.h"
#include "xmpi.h"
#include "driver.h"
#include "xpanel.h"

static void	init		(void);
static int	is_our_file	(char *);
static void	play_file	(InputPlayback *);
static void	stop		(InputPlayback *);
static void	mod_pause	(InputPlayback *, short);
static void	seek		(InputPlayback *, int);
static int	get_time	(InputPlayback *);
static void	*play_loop	(void *);
static void	aboutbox	(void);
static void	get_song_info	(char *, char **, int *);
static void	configure	(void);
static void	config_ok	(GtkWidget *, gpointer);
static void	file_info_box	(char *);
static void	mseek		(InputPlayback *, gulong);
static void	cleanup		(void);

#if __AUDACIOUS_PLUGIN_API__ >= 2
static Tuple	*get_song_tuple	(char *);
#endif

static GThread *decode_thread;
static GStaticMutex load_mutex = G_STATIC_MUTEX_INIT;


#define FREQ_SAMPLE_44 0
#define FREQ_SAMPLE_22 1
#define FREQ_SAMPLE_11 2

static struct {
	InputPlayback *ipb;
	AFormat fmt;
	int nch;
} play_data;

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
static gboolean xmp_plugin_audio_error = FALSE;

static GtkWidget *Res_16;
static GtkWidget *Res_8;
static GtkWidget *Chan_ST;
static GtkWidget *Chan_MO;
static GtkWidget *Sample_44;
static GtkWidget *Sample_22;
static GtkWidget *Sample_11;
static GtkWidget *Convert_Check;
static GtkWidget *Fixloops_Check;
static GtkWidget *Modrange_Check;
static GtkWidget *Interp_Check;
static GtkWidget *Filter_Check;
static GtkObject *pansep_adj;

static GtkWidget *xmp_conf_window = NULL;
static GtkWidget *about_window = NULL;
static GtkWidget *info_window = NULL;

struct ipc_info _ii, *ii = &_ii;
int skip = 0;
extern volatile int new_module;
static short audio_open = FALSE;


InputPlugin xmp_ip = {
	.description	= "XMP Plugin " VERSION,
	.init		= init,
	.about		= aboutbox,
	.configure	= configure,
	.is_our_file	= is_our_file,
	.play_file	= play_file,
	.stop		= stop,
	.pause		= mod_pause,
	.seek		= seek,
	.get_time	= get_time,
	.get_song_info	= get_song_info,
	.file_info_box	= file_info_box,
	.cleanup	= cleanup,
#if __AUDACIOUS_PLUGIN_API__ >= 2
	.get_song_tuple	= get_song_tuple,
	.mseek		= mseek,
#endif
};


#if __AUDACIOUS_PLUGIN_API__ >= 2

InputPlugin *xmp_iplist[] = { &xmp_ip, NULL };

DECLARE_PLUGIN(xmp, NULL, NULL, xmp_iplist, NULL, NULL, NULL, NULL, NULL);

#endif
 
static void file_info_box_build (void);
static void init_visual (GdkVisual *);

static void strip_vfs(char *s)
{
	int len;
	char *c;

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

static void aboutbox()
{
	GtkWidget *vbox1;
	GtkWidget *label1;
	GtkWidget *about_exit;
	GtkWidget *scroll1;
	GtkWidget *table1;
	GtkWidget *label_fmt, *label_trk;
	struct xmp_fmt_info *f, *fmt;
	int i;

	if (about_window) {
		gdk_window_raise(about_window->window);
		return;
	}

	about_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(about_window),
		"about_window", about_window);
	gtk_window_set_title(GTK_WINDOW(about_window),"About the XMP Plugin");
	gtk_window_set_policy(GTK_WINDOW(about_window), FALSE, FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(about_window), "destroy",
		GTK_SIGNAL_FUNC(gtk_widget_destroyed), &about_window);
	gtk_container_border_width(GTK_CONTAINER(about_window), 10);
	gtk_widget_realize(about_window);

	vbox1 = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(about_window), vbox1);
	gtk_object_set_data(GTK_OBJECT(about_window), "vbox1", vbox1);
	gtk_widget_show(vbox1);
	gtk_container_border_width(GTK_CONTAINER(vbox1), 10);

	label1 = gtk_label_new(
		"Extended Module Player " VERSION "\n"
		"Written by Claudio Matsuoka and Hipolito Carraro Jr.\n"
		"\n"
		"Portions Copyright (C) 1998,2000 Olivier Lapicque,\n"
		"(C) 1998 Tammo Hinrichs, (C) 1998 Sylvain Chipaux,\n"
		"(C) 1997 Bert Jahn, (C) 1999 Tatsuyuki Satoh, (C)\n"
		"1996-1999 Takuya Ooura, (C) 2001-2006 Russell Marks\n"
		"\n"
		"Supported module formats:"
	);
	gtk_object_set_data(GTK_OBJECT(label1), "label1", label1);
	gtk_box_pack_start(GTK_BOX(vbox1), label1, TRUE, TRUE, 0);

	scroll1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll1),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_widget_set_size_request(scroll1, 290, 100);
	gtk_object_set_data(GTK_OBJECT(scroll1), "scroll1", scroll1);
	gtk_widget_set (scroll1, "height", 100, NULL);
	gtk_box_pack_start(GTK_BOX(vbox1), scroll1, TRUE, TRUE, 0);

	xmp_get_fmt_info(&fmt);
	table1 = gtk_table_new(100, 2, FALSE);
	for (i = 0, f = fmt; f; i++, f = f->next) {
		label_fmt = gtk_label_new(f->id);
		label_trk = gtk_label_new(f->tracker);
		gtk_label_set_justify (GTK_LABEL (label_fmt), GTK_JUSTIFY_LEFT);
		gtk_label_set_justify (GTK_LABEL (label_trk), GTK_JUSTIFY_LEFT);
		gtk_table_attach_defaults (GTK_TABLE (table1),
						label_fmt, 0, 1, i, i + 1);
		gtk_table_attach_defaults (GTK_TABLE (table1),
						label_trk, 1, 2, i, i + 1);
	}

	gtk_table_resize (GTK_TABLE (table1), i + 1, 3);
	gtk_object_set_data(GTK_OBJECT(table1), "table1", table1);
	
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll1),
								table1);

	about_exit = gtk_button_new_with_label("Ok");
	gtk_signal_connect_object(GTK_OBJECT(about_exit), "clicked",
		GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(about_window));

	gtk_object_set_data(GTK_OBJECT(about_window), "about_exit", about_exit);
	gtk_box_pack_start(GTK_BOX(vbox1), about_exit, FALSE, FALSE, 0);

	gtk_widget_show_all(about_window);
}

static GdkImage *image;
static GtkWidget *image1;
static GtkWidget *frame1;

static GtkWidget *text1;
static GtkTextBuffer *text1b;

static GdkColor *color_black;
static GdkColor *color_white;
static GdkColormap *colormap;
static XImage *ximage;
static Display *display;
static Window window;

xmp_context ctx;


static void stop(InputPlayback *ipb)
{
	if (!ipb->playing)
		return;

	_D("*** stop!");
	xmp_stop_module(ctx); 
	ii->mode = 0;

	ipb->playing = 0;
	g_thread_join(decode_thread);
	ipb->output->close_audio();
        audio_open = FALSE;
}

static void seek(InputPlayback *ipb, int time)
{
	mseek(ipb, time * 1000);
}

static void mseek(InputPlayback *ipb, unsigned long time)
{
	int i, t;
	struct xmp_player_context *p = &((struct xmp_context *)ctx)->p;

	_D("seek to %d, total %d", time, xmp_cfg.time);

	for (i = 0; i < xmp_cfg.mod_info.len; i++) {
		t = p->m.xxo_info[i].time;

		_D("%2d: %d %d", i, time, t);

		if (t > time) {
			int a;
			if (i > 0)
				i--;
			a = xmp_ord_set(ctx, i);
			xmp_ip.output->flush(p->m.xxo_info[i].time);
			break;
		}
	}
}

static void mod_pause(InputPlayback *ipb, short p)
{
	ii->pause = p;
	xmp_ip.output->pause(p);
}


static int get_time(InputPlayback *ipb)
{
	if (xmp_plugin_audio_error)
		return -2;
	if (!ipb->playing)
		return -1;

	return xmp_ip.output->output_time();
}


#if __AUDACIOUS_PLUGIN_API__ < 2

InputPlugin *get_iplugin_info()
{
	return &xmp_ip;
}

#endif


static void driver_callback(void *b, int i)
{
#if __AUDACIOUS_PLUGIN_API__ >= 2
	play_data.ipb->pass_audio(play_data.ipb, play_data.fmt, play_data.nch,
					i, b, &play_data.ipb->playing);

#else
	xmp_ip.add_vis_pcm(xmp_ip.output->written_time(),
			xmp_cfg.force8bit ? FMT_U8 : FMT_S16_NE,
			xmp_cfg.force_mono ? 1 : 2, i, b);
	
	while (xmp_ip.output->buffer_free() < i && play_data.ipb->playing)
		usleep(10000);

	if (play_data.ipb->playing)
		xmp_ip.output->write_audio(b, i);
#endif
}


static void init(void)
{
	ConfigDb *cfg;

	ctx = xmp_create_context();

	xmp_cfg.mixing_freq = 0;
	xmp_cfg.convert8bit = 0;
	xmp_cfg.fixloops = 0;
	xmp_cfg.modrange = 0;
	xmp_cfg.force8bit = 0;
	xmp_cfg.force_mono = 0;
	xmp_cfg.interpolation = TRUE;
	xmp_cfg.filter = TRUE;
	xmp_cfg.pan_amplitude = 80;

#define CFGREADINT(x) bmp_cfg_db_get_int (cfg, "XMP", #x, &xmp_cfg.x)

	if ((cfg = bmp_cfg_db_open())) {
		CFGREADINT(mixing_freq);
		CFGREADINT(force8bit);
		CFGREADINT(convert8bit);
		CFGREADINT(modrange);
		CFGREADINT(fixloops);
		CFGREADINT(force_mono);
		CFGREADINT(interpolation);
		CFGREADINT(filter);
		CFGREADINT(pan_amplitude);

		bmp_cfg_db_close(cfg);
	}

	file_info_box_build();

	xmp_init_callback(ctx, driver_callback);
	xmp_register_event_callback(x11_event_callback);

	memset(ii, 0, sizeof (ii));
	ii->wresult = 42;
}


static void cleanup()
{
	xmp_free_context(ctx);
}


static int is_our_file(char *filename)
{
	strip_vfs(filename);		/* Sorry, no VFS support */

	if (xmp_test_module(ctx, filename, NULL) == 0)
		return 1;

	return 0;
}


static void get_song_info(char *filename, char **title, int *length)
{
	xmp_context ctx2;
	int lret;
	struct xmp_module_info mi;
	struct xmp_options *opt;

	strip_vfs(filename);		/* Sorry, no VFS support */

	/* Create new context to load a file and get the length */

	ctx2 = xmp_create_context();
	opt = xmp_get_options(ctx2);
	opt->skipsmp = 1;	/* don't load samples */

	g_static_mutex_lock(&load_mutex);
	lret = xmp_load_module(ctx2, filename);
	g_static_mutex_unlock(&load_mutex);

	if (lret < 0) {
		xmp_free_context(ctx2);
		return;
	}

	*length = lret;
	xmp_get_module_info(ctx2, &mi);
	*title = g_strdup(mi.name);

	xmp_release_module(ctx2);
	xmp_free_context(ctx2);
}


#if __AUDACIOUS_PLUGIN_API__ >= 2

static Tuple *get_song_tuple(char *filename)
{
	Tuple *tuple;
	xmp_context ctx2;
	int lret;
	struct xmp_module_info mi;
	struct xmp_options *opt;

	strip_vfs(filename);		/* Sorry, no VFS support */

	tuple = aud_tuple_new_from_filename(filename);

	/* Create new context to load a file and get the length */

	ctx2 = xmp_create_context();
	opt = xmp_get_options(ctx2);
	opt->skipsmp = 1;	/* don't load samples */

	g_static_mutex_lock(&load_mutex);
	lret = xmp_load_module(ctx2, filename);
	g_static_mutex_unlock(&load_mutex);

	if (lret < 0) {
		xmp_free_context(ctx2);
		return NULL;
	}

	xmp_get_module_info(ctx2, &mi);

	aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, mi.name);
	aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, mi.type);
	aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, lret);

	xmp_release_module(ctx2);
	xmp_free_context(ctx2);

	return tuple;
}

#endif


static int fd_old2, fd_info[2];
static GThread *catch_thread;

static gpointer catch_info(gpointer arg)
{
	FILE *f;
	char buf[100];
	GtkTextIter end;

	f = fdopen(fd_info[0], "r");

	while (!feof (f)) {
		fgets (buf, 100, f);
		gtk_text_buffer_get_end_iter(text1b, &end);
		gtk_text_buffer_insert(text1b, &end, buf, -1);

		if (!strncmp(buf, "Estimated time :", 16))
			break;
	}

	fclose (f);

	return NULL;
}


static void play_file(InputPlayback *ipb)
{
	char *filename = ipb->filename;
	int channelcnt = 1;
	FILE *f;
	struct xmp_options *opt;
	int lret;
	GtkTextIter start, end;
	
	opt = xmp_get_options(ctx);

	/* Sorry, no VFS support */
	if (memcmp(filename, "file://", 7) == 0)	/* Audacious 1.4.0 */
		filename += 7;

	_D("play_file: %s", filename);

	stop(ipb);	/* sanity check */

	if ((f = fopen(filename,"rb")) == 0) {
		ipb->playing = 0;
		return;
	}
	fclose(f);

	gtk_text_buffer_get_start_iter(text1b, &start);
	gtk_text_buffer_get_end_iter(text1b, &end);
	gtk_text_buffer_delete(text1b, &start, &end);

	xmp_plugin_audio_error = FALSE;
	ipb->playing = 1;

	opt->resol = 8;
	opt->verbosity = 3;
	opt->drv_id = "callback";

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

	play_data.ipb = ipb;
	play_data.fmt = opt->resol == 16 ? FMT_S16_NE : FMT_U8;
	play_data.nch = opt->outfmt & XMP_FMT_MONO ? 1 : 2;
	
	if (audio_open)
	    ipb->output->close_audio();
	
	if (!ipb->output->open_audio(play_data.fmt, opt->freq, play_data.nch)) {
	    ipb->error = TRUE;
	    xmp_plugin_audio_error = TRUE;
	    return;
	}
	
	audio_open = TRUE;

	xmp_open_audio(ctx);

	pipe(fd_info);
	fd_old2 = dup (fileno (stderr));
	dup2(fd_info[1], fileno (stderr));
	fflush(stderr);
	catch_thread = g_thread_create(catch_info, NULL, TRUE, NULL);

	_D("*** loading: %s", filename);
	g_static_mutex_lock(&load_mutex);
	lret =  xmp_load_module(ctx, filename);
	g_static_mutex_unlock(&load_mutex);

	if (lret < 0) {
		xmp_ip.set_info_text("Error loading mod");
		ipb->playing = 0;
		return;
	}

	_D("joining catch thread");
	g_thread_join(catch_thread);
	_D("joined");
	dup2(fileno(stderr), fd_old2);

	gtk_adjustment_set_value(GTK_TEXT_VIEW(text1)->vadjustment, 0.0);

	close(fd_info[0]);
	close(fd_info[1]);

	_D ("before panel update");

	xmp_cfg.time = lret; //xmpi_scan_module((struct xmp_context *)ctx);
	xmp_get_module_info(ctx, &ii->mi);
	strcpy(ii->filename, "");

	new_module = 1;

	_D("after panel update");

	memcpy(&xmp_cfg.mod_info, &ii->mi, sizeof (ii->mi));

#if __AUDACIOUS_PLUGIN_API__ >= 2
	ipb->set_params(ipb, ii->mi.name, lret, 0, opt->freq, channelcnt);
	ipb->playing = 1;
	ipb->eof = 0;
	ipb->error = FALSE;

	decode_thread = g_thread_self();
	ipb->set_pb_ready(ipb);
	play_loop(ipb);
#else
	xmp_ip.set_info(ii->mi.name, lret, 0, opt->freq, channelcnt);
	decode_thread = g_thread_create(play_loop, ipb, TRUE, NULL);
#endif
}


static gpointer play_loop(gpointer arg)
{
	InputPlayback *ipb = arg;

	xmp_play_module(ctx);
	xmp_release_module(ctx);
	xmp_close_audio(ctx);

	ipb->eof = 1;
	ipb->playing = 0;

	return NULL;
}


static void configure()
{
	GtkWidget *notebook1;
	GtkWidget *vbox;
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *Resolution_Frame;
	GtkWidget *vbox4;
	GSList *resolution_group = NULL;
	GtkWidget *Channels_Frame;
	GtkWidget *vbox5;
	GSList *vbox5_group = NULL;
	GtkWidget *Downsample_Frame;
	GtkWidget *vbox3;
	GSList *sample_group = NULL;
	GtkWidget *vbox6;
	GtkWidget *Quality_Label;
	GtkWidget *Options_Label;
	GtkWidget *pansep_label, *pansep_hscale;
	GtkWidget *bbox;
	GtkWidget *ok;
	GtkWidget *cancel;

	if (xmp_conf_window) {
		gdk_window_raise(xmp_conf_window->window);
		return;
	}

	xmp_conf_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),
		"xmp_conf_window", xmp_conf_window);
	gtk_window_set_title(GTK_WINDOW(xmp_conf_window), "XMP Configuration");
	gtk_window_set_policy(GTK_WINDOW(xmp_conf_window), FALSE, FALSE, FALSE);
	gtk_window_set_position(GTK_WINDOW(xmp_conf_window), GTK_WIN_POS_MOUSE);
	gtk_signal_connect(GTK_OBJECT(xmp_conf_window), "destroy",
		GTK_SIGNAL_FUNC(gtk_widget_destroyed),&xmp_conf_window);
	gtk_container_border_width(GTK_CONTAINER(xmp_conf_window), 10);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(xmp_conf_window), vbox);

	notebook1 = gtk_notebook_new();
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),"notebook1", notebook1);
	gtk_widget_show(notebook1);
	gtk_box_pack_start(GTK_BOX(vbox), notebook1, TRUE, TRUE, 0);
	gtk_container_border_width(GTK_CONTAINER(notebook1), 3);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),"vbox1", vbox1);
	gtk_widget_show(vbox1);

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),"hbox1", hbox1);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 0);

	Resolution_Frame = gtk_frame_new("Resolution");
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),
		"Resolution_Frame", Resolution_Frame);
	gtk_widget_show(Resolution_Frame);
	gtk_box_pack_start(GTK_BOX(hbox1), Resolution_Frame, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER (Resolution_Frame), 5);

	vbox4 = gtk_vbox_new(FALSE, 0);
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),"vbox4", vbox4);
	gtk_widget_show(vbox4);
	gtk_container_add(GTK_CONTAINER(Resolution_Frame), vbox4);

	Res_16 = gtk_radio_button_new_with_label(resolution_group, "16 bit");
	resolution_group = gtk_radio_button_group(GTK_RADIO_BUTTON(Res_16));
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "Res_16", Res_16);
	gtk_widget_show(Res_16);
	gtk_box_pack_start(GTK_BOX(vbox4), Res_16, TRUE, TRUE, 0);
	if (xmp_cfg.force8bit == 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Res_16), TRUE);

	Res_8 = gtk_radio_button_new_with_label(resolution_group, "8 bit");
	resolution_group = gtk_radio_button_group(GTK_RADIO_BUTTON(Res_8));
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "Res_8", Res_8);
	gtk_widget_show(Res_8);
	gtk_box_pack_start(GTK_BOX(vbox4), Res_8, TRUE, TRUE, 0);
	if (xmp_cfg.force8bit == 1)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Res_8), TRUE);

	Channels_Frame = gtk_frame_new("Channels");
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),
		"Channels_Frame", Channels_Frame);
	gtk_widget_show(Channels_Frame);
	gtk_box_pack_start(GTK_BOX(hbox1), Channels_Frame, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(Channels_Frame), 5);

	vbox5 = gtk_vbox_new(FALSE, 0);
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "vbox5", vbox5);
	gtk_widget_show(vbox5);
	gtk_container_add(GTK_CONTAINER(Channels_Frame), vbox5);

	Chan_ST = gtk_radio_button_new_with_label(vbox5_group, "Stereo");
	vbox5_group = gtk_radio_button_group(GTK_RADIO_BUTTON(Chan_ST));
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "Chan_ST", Chan_ST);
	gtk_widget_show(Chan_ST);
	gtk_box_pack_start(GTK_BOX(vbox5), Chan_ST, TRUE, TRUE, 0);
	if (xmp_cfg.force_mono == 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Chan_ST), TRUE);

	Chan_MO = gtk_radio_button_new_with_label(vbox5_group, "Mono");
	vbox5_group = gtk_radio_button_group(GTK_RADIO_BUTTON(Chan_MO));
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "Chan_MO", Chan_MO);
	gtk_widget_show(Chan_MO);
	gtk_box_pack_start(GTK_BOX(vbox5), Chan_MO, TRUE, TRUE, 0);
	if (xmp_cfg.force_mono == 1)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Chan_MO), TRUE);

	Downsample_Frame = gtk_frame_new("Sampling rate");
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),
		"Downsample_Frame", Downsample_Frame);
	gtk_widget_show(Downsample_Frame);
	gtk_box_pack_start(GTK_BOX(vbox1), Downsample_Frame, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(Downsample_Frame), 5);

	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "vbox3", vbox3);
	gtk_widget_show(vbox3);
	gtk_container_add(GTK_CONTAINER(Downsample_Frame), vbox3);

	Sample_44 = gtk_radio_button_new_with_label(sample_group, "44 kHz");
	sample_group = gtk_radio_button_group(GTK_RADIO_BUTTON(Sample_44));
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),"Sample_44", Sample_44);
	gtk_widget_show(Sample_44);
	gtk_box_pack_start(GTK_BOX(vbox3), Sample_44, TRUE, TRUE, 0);
	if (xmp_cfg.mixing_freq == FREQ_SAMPLE_44)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Sample_44),TRUE);

	Sample_22 = gtk_radio_button_new_with_label(sample_group, "22 kHz");
	sample_group = gtk_radio_button_group(GTK_RADIO_BUTTON(Sample_22));
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "Sample_22",Sample_22);
	gtk_widget_show(Sample_22);
	gtk_box_pack_start(GTK_BOX(vbox3), Sample_22, TRUE, TRUE, 0);
	if (xmp_cfg.mixing_freq == FREQ_SAMPLE_22)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Sample_22),TRUE);

	Sample_11 = gtk_radio_button_new_with_label(sample_group, "11 kHz");
	sample_group = gtk_radio_button_group(GTK_RADIO_BUTTON(Sample_11));
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "Sample_11",Sample_11);
	gtk_widget_show(Sample_11);
	gtk_box_pack_start(GTK_BOX(vbox3), Sample_11, TRUE, TRUE, 0);
	if (xmp_cfg.mixing_freq == FREQ_SAMPLE_11)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Sample_11),TRUE);

	vbox6 = gtk_vbox_new(FALSE, 0);
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), "vbox6", vbox6);
	gtk_widget_show(vbox6);

	/* Options */

#define OPTCHECK(w,l,o) {						\
	w = gtk_check_button_new_with_label(l);				\
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window), #w, w);	\
	gtk_widget_show(w);						\
	gtk_box_pack_start(GTK_BOX(vbox6), w, TRUE, TRUE, 0);		\
	if (xmp_cfg.o == 1)						\
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE); \
}

	OPTCHECK(Convert_Check, "Convert 16 bit samples to 8 bit", convert8bit);
	OPTCHECK(Fixloops_Check, "Fix sample loops", fixloops);
	OPTCHECK(Modrange_Check, "Force 3 octave range in standard MOD files",
		modrange);
	OPTCHECK(Interp_Check, "Enable 32-bit linear interpolation",
		interpolation);
	OPTCHECK(Filter_Check, "Enable IT filters", filter);

	pansep_label = gtk_label_new("Pan amplitude (%)");
	gtk_widget_show(pansep_label);
	gtk_box_pack_start(GTK_BOX(vbox6), pansep_label, TRUE, TRUE, 0);
	pansep_adj = gtk_adjustment_new(xmp_cfg.pan_amplitude,
		0.0, 100.0, 1.0, 10.0, 1.0);
	pansep_hscale = gtk_hscale_new(GTK_ADJUSTMENT(pansep_adj));
	gtk_scale_set_digits(GTK_SCALE(pansep_hscale), 0);
	gtk_scale_set_draw_value(GTK_SCALE(pansep_hscale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(pansep_hscale), GTK_POS_BOTTOM);
	gtk_widget_show(pansep_hscale);
	gtk_box_pack_start(GTK_BOX(vbox6), pansep_hscale, TRUE, TRUE, 0);

	Quality_Label = gtk_label_new("Quality");
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),
		"Quality_Label", Quality_Label);
	gtk_widget_show(Quality_Label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), vbox1, Quality_Label);

	Options_Label = gtk_label_new("Options");
	gtk_object_set_data(GTK_OBJECT(xmp_conf_window),
		"Options_Label", Options_Label);
	gtk_widget_show(Options_Label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook1), vbox6, Options_Label);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	ok = gtk_button_new_with_label("Ok");
	gtk_signal_connect(GTK_OBJECT(ok), "clicked",
		GTK_SIGNAL_FUNC(config_ok), NULL);
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
	gtk_widget_show(ok);
	gtk_widget_grab_default(ok);

	cancel = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked",
		GTK_SIGNAL_FUNC(gtk_widget_destroy),
		GTK_OBJECT(xmp_conf_window));
	GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
	gtk_widget_show(cancel);

	gtk_widget_show(bbox);

	gtk_widget_show(vbox);
	gtk_widget_show(xmp_conf_window);
}


static void config_ok(GtkWidget *widget, gpointer data)
{
	ConfigDb *cfg;
	struct xmp_options *opt;

	opt = xmp_get_options(ctx);

	if (GTK_TOGGLE_BUTTON(Res_16)->active)
		xmp_cfg.force8bit = 0;
	if (GTK_TOGGLE_BUTTON(Res_8)->active)
		xmp_cfg.force8bit = 1;

	if (GTK_TOGGLE_BUTTON(Chan_ST)->active)
		xmp_cfg.force_mono = 0;
	if (GTK_TOGGLE_BUTTON(Chan_MO)->active)
		xmp_cfg.force_mono = 1;

	if (GTK_TOGGLE_BUTTON(Sample_44)->active)
		xmp_cfg.mixing_freq = 0;
	if (GTK_TOGGLE_BUTTON(Sample_22)->active)
		xmp_cfg.mixing_freq = 1;
	if (GTK_TOGGLE_BUTTON(Sample_11)->active)
		xmp_cfg.mixing_freq = 2;

	xmp_cfg.interpolation = !!GTK_TOGGLE_BUTTON(Interp_Check)->active;
	xmp_cfg.filter = !!GTK_TOGGLE_BUTTON(Filter_Check)->active;
	xmp_cfg.convert8bit = !!GTK_TOGGLE_BUTTON(Convert_Check)->active;
	xmp_cfg.modrange = !!GTK_TOGGLE_BUTTON(Modrange_Check)->active;
	xmp_cfg.fixloops = !!GTK_TOGGLE_BUTTON(Fixloops_Check)->active;

	xmp_cfg.pan_amplitude = (guchar)GTK_ADJUSTMENT(pansep_adj)->value;
        opt->mix = xmp_cfg.pan_amplitude;

	cfg = bmp_cfg_db_open();

#define CFGWRITEINT(x) bmp_cfg_db_set_int (cfg, "XMP", #x, xmp_cfg.x)

	CFGWRITEINT (mixing_freq);
	CFGWRITEINT (force8bit);
	CFGWRITEINT (convert8bit);
	CFGWRITEINT (modrange);
	CFGWRITEINT (fixloops);
	CFGWRITEINT (force_mono);
	CFGWRITEINT (interpolation);
	CFGWRITEINT (filter);
	CFGWRITEINT (pan_amplitude);

	bmp_cfg_db_close(cfg);

	gtk_widget_destroy(xmp_conf_window);
}

static void button_cycle(GtkWidget *widget, GdkEvent *event)
{
     ii->mode++;
     ii->mode %= 3;
}

static void button_mute(GtkWidget *widget, GdkEvent *event)
{
	int i;

	if (!play_data.ipb->playing)
		return;

	xmp_channel_mute(ctx, 0, 64, 1);
	for (i = 0; i < ii->mi.chn; i++)
		ii->mute[i] = 1;
}

static void button_unmute(GtkWidget *widget, GdkEvent *event)
{
	int i;

	if (!play_data.ipb->playing)
		return;

	xmp_channel_mute(ctx, 0, 64, 0);
	for (i = 0; i < ii->mi.chn; i++)
		ii->mute[i] = 0;
}

static int image1_clicked_x = 0;
static int image1_clicked_y = 0;
static int image1_clicked_ok = 0;

static void image1_clicked(GtkWidget *widget, GdkEventButton *event)
{
	if (!play_data.ipb->playing || image1_clicked_ok)
		return;

	image1_clicked_x = event->x - (frame1->allocation.width - 300) / 2;
	image1_clicked_y = event->y;
	image1_clicked_ok = 1;
}

static void file_info_box_build()
{
	GtkWidget *hbox1, *vbox1;
	GtkWidget *info_exit, *info_cycle, *info_mute;
	GtkWidget *info_unmute, *info_about;
	GtkWidget *scrw1;
	GtkWidget *expander;
	GdkVisual *visual;
	PangoFontDescription *desc;

	info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(info_window),
					"info_window", info_window);
	gtk_window_set_title(GTK_WINDOW(info_window),"Extended Module Player");
	gtk_window_set_policy(GTK_WINDOW(info_window), FALSE, FALSE, TRUE);
	gtk_signal_connect(GTK_OBJECT(info_window), "destroy",
		GTK_SIGNAL_FUNC(gtk_widget_destroyed), &info_window);
	gtk_container_border_width(GTK_CONTAINER(info_window), 0);
	gtk_widget_realize (info_window);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(info_window), vbox1);
	gtk_object_set_data(GTK_OBJECT(vbox1), "vbox1", vbox1);
	gtk_container_border_width(GTK_CONTAINER(vbox1), 4);

	visual = gdk_visual_get_system();

	/*
	 * Image
	 */

	frame1 = gtk_event_box_new();
	gtk_object_set_data(GTK_OBJECT(frame1), "frame1", frame1);
	gtk_widget_set_size_request(frame1, 300, 128);
	gtk_box_pack_start(GTK_BOX(vbox1), frame1, FALSE, FALSE, 0);

	image = gdk_image_new(GDK_IMAGE_FASTEST, visual, 300, 128);
	ximage = GDK_IMAGE_XIMAGE(image);
	image1 = gtk_image_new_from_image(image, NULL);

	gtk_object_set_data(GTK_OBJECT(image1), "image1", image1);
	gtk_container_add (GTK_CONTAINER(frame1), image1);
	gtk_widget_set_events (frame1, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
	gtk_signal_connect (GTK_OBJECT (frame1), "button_press_event",
			(GtkSignalFunc)image1_clicked, NULL);

	/*
	 * Buttons
	 */

	hbox1 = gtk_hbox_new (TRUE, 0);
	gtk_object_set_data(GTK_OBJECT(hbox1), "hbox1", hbox1);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, FALSE, 0);

	info_cycle = gtk_button_new_with_label("Mode");
	gtk_signal_connect (GTK_OBJECT (info_cycle), "clicked",
                            (GtkSignalFunc) button_cycle, NULL);
	gtk_object_set_data(GTK_OBJECT(info_cycle), "info_cycle", info_cycle);
	gtk_box_pack_start(GTK_BOX(hbox1), info_cycle, TRUE, TRUE, 0);

	info_mute = gtk_button_new_with_label("Mute");
	gtk_signal_connect (GTK_OBJECT (info_mute), "clicked",
                            (GtkSignalFunc) button_mute, NULL);
	gtk_object_set_data(GTK_OBJECT(info_mute), "info_mute", info_mute);
	gtk_box_pack_start(GTK_BOX(hbox1), info_mute, TRUE, TRUE, 0);

	info_unmute = gtk_button_new_with_label("Unmute");
	gtk_signal_connect (GTK_OBJECT (info_unmute), "clicked",
                            (GtkSignalFunc) button_unmute, NULL);
	gtk_object_set_data(GTK_OBJECT(info_unmute), "info_unmute", info_unmute);
	gtk_box_pack_start(GTK_BOX(hbox1), info_unmute, TRUE, TRUE, 0);

	info_about = gtk_button_new_with_label("About");
	gtk_signal_connect_object(GTK_OBJECT(info_about), "clicked",
			(GtkSignalFunc) aboutbox, NULL);
	gtk_object_set_data(GTK_OBJECT(info_about), "info_about", info_about);
	gtk_box_pack_start(GTK_BOX(hbox1), info_about, TRUE, TRUE, 0);

	info_exit = gtk_button_new_with_label("Close");
	gtk_signal_connect_object(GTK_OBJECT(info_exit), "clicked",
		GTK_SIGNAL_FUNC(gtk_widget_hide), GTK_OBJECT(info_window));
	gtk_object_set_data(GTK_OBJECT(info_exit), "info_exit", info_exit);
	gtk_box_pack_start(GTK_BOX(hbox1), info_exit, TRUE, TRUE, 0);

	/*
	 * Info area
	 */

	expander = gtk_expander_new("Module information");

	scrw1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_object_set_data(GTK_OBJECT(scrw1), "scrw1", scrw1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrw1),
				GTK_POLICY_ALWAYS, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(expander), scrw1);
	gtk_box_pack_start(GTK_BOX(vbox1), expander, TRUE, TRUE, 0);

	gtk_widget_set_size_request(scrw1, 290, 200);
	text1b = gtk_text_buffer_new(NULL);
	text1 = gtk_text_view_new_with_buffer(text1b);
	desc = pango_font_description_new();
	pango_font_description_set_family(desc, "Monospace");
	gtk_widget_modify_font(text1, desc);
	pango_font_description_free(desc);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text1), GTK_WRAP_NONE);

	gtk_object_set_data(GTK_OBJECT(text1), "text1", text1);
	gtk_container_add(GTK_CONTAINER(scrw1), text1);
	gtk_widget_realize(text1);

	gtk_widget_realize(image1);

	display = GDK_WINDOW_XDISPLAY(info_window->window);
	window = GDK_WINDOW_XWINDOW(info_window->window);
    	colormap = gdk_colormap_get_system();

	gdk_color_black(colormap, color_black);
	gdk_color_white(colormap, color_white);

	init_visual(visual);

	set_palette();
	clear_screen();

	ii->wresult = 0;

	panel_setup();
	gtk_timeout_add(50, (GtkFunction)panel_loop, NULL);
}


static void file_info_box(char *filename)
{
	_D ("Info box");

	gtk_widget_show_all(info_window);
	gdk_window_raise(info_window->window);
}


/*----------------------------------------------------------------------*/

#define alloc_color(d,c,x) \
gdk_colormap_alloc_color(colormap, (GdkColor *)x, TRUE, TRUE)

#include "xstuff.c"


static void init_visual(GdkVisual *visual)
{
    if (visual->type == GDK_VISUAL_PSEUDO_COLOR && visual->depth == 8) {
	draw_rectangle = draw_rectangle_indexed;
	erase_rectangle = erase_rectangle_indexed;
	indexed = 1;
    } else if (visual->type == GDK_VISUAL_TRUE_COLOR && visual->depth == 24) {
	draw_rectangle = draw_rectangle_rgb24;
	erase_rectangle = erase_rectangle_rgb24;
	indexed = 0;
    } else if (visual->type == GDK_VISUAL_TRUE_COLOR && visual->depth == 16) {
	mask_r = 0xf00000;
	mask_g = 0x00f800;
	mask_b = 0x0000f0;
	draw_rectangle = draw_rectangle_rgb16;
	erase_rectangle = erase_rectangle_rgb16;
	indexed = 0;
    } else if (visual->type == GDK_VISUAL_TRUE_COLOR && visual->depth == 15) {
	mask_r = 0xf00000;
	mask_g = 0x00f000;
	mask_b = 0x0000f0;
	draw_rectangle = draw_rectangle_rgb15;
	erase_rectangle = erase_rectangle_rgb15;
	indexed = 0;
    } else {
	fprintf (stderr, "Visual class and depth not supported, aborting\n");
	exit (-1);
    }
}

void update_display()
{
	GdkRectangle area;

	area.x = (frame1->allocation.width - 300) / 2;
	area.y = 0;
	area.width = 300;
	area.height = 128;

	gdk_window_invalidate_rect(image1->window, &area, FALSE);
}

int process_events(int *x, int *y)
{
	if (image1_clicked_ok) {
		*x = image1_clicked_x;
		*y = image1_clicked_y;
		image1_clicked_ok = 0;
		return -1;
	}

	return 0;
}
