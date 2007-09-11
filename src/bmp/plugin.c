/*
 * XMP plugin for XMMS
 * Written by Claudio Matsuoka, 2000-04-30
 * Based on J. Nick Koston's MikMod plugin
 *
 * $Id: plugin.c,v 1.10 2007-09-11 19:35:42 cmatsuoka Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>


#ifdef BMP_PLUGIN
#include <bmp/configfile.h>
#include <bmp/util.h>
#include <bmp/plugin.h>
#else
#include <xmms/configfile.h>
#include <xmms/util.h>
#include <xmms/plugin.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "xmp.h"
#include "xmpi.h"
#include "driver.h"
#include "formats.h"
#include "xpanel.h"


static void	init		(void);
static int	is_our_file	(char *);
static void	play_file	(char *);
static int	get_time	(void);
static void	stop		(void);
static void	*play_loop	(void *);
static void	mod_pause	(short);
static void	seek		(int time);
static void	aboutbox	(void);
static void	get_song_info	(char *, char **, int *);
static void	configure	(void);
static void	config_ok	(GtkWidget *, gpointer);
static void	file_info_box	(char *);

static struct xmp_control ctl;

static pthread_t decode_thread;
static pthread_mutex_t load_mutex = PTHREAD_MUTEX_INITIALIZER;



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



XMPConfig xmp_cfg;
static gboolean xmp_xmms_audio_error = FALSE;
extern InputPlugin xmp_ip;
extern struct xmp_ord_info xxo_info[XMP_DEF_MAXORD];


/* module parameters */
gboolean xmp_going = 0;

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
//static gint expose_event (GtkWidget *, GdkEventExpose *);

struct ipc_info _ii, *ii = &_ii;
int skip = 0;
extern volatile int new_module;
static short audio_open = FALSE;

InputPlugin xmp_ip = {
	NULL,			/* handle */
	NULL,			/* filename */
	"XMP Player " VERSION,	/* description */
	init,			/* init */
	aboutbox,		/* about */
	configure,		/* configure */
	is_our_file,		/* is_our_file */
	NULL,			/* scan_dir */
	play_file,		/* play_file */
	stop,			/* stop */
	mod_pause,		/* pause */
	seek,			/* seek */
	NULL,			/* set_eq */
	get_time,		/* get_time */
	NULL,			/* get_volume */
	NULL,			/* set_volume */
	NULL,			/* add_vis -- obsolete */
	NULL,			/* get_vis_type -- obsolete */
	NULL,			/* add_vis_pcm */
	NULL,			/* set_info */
	NULL,			/* set_info_text */
	get_song_info,		/* get_song_info */
	file_info_box,		/* file_info_box */
	NULL			/* output */
};


static void file_info_box_build (void);
static void init_visual (GdkVisual *);

static void aboutbox ()
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

	scroll1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll1),
		GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_widget_set_size_request(scroll1, 290, 100);
	gtk_object_set_data(GTK_OBJECT(scroll1), "scroll1", scroll1);
	gtk_widget_set (scroll1, "height", 100, NULL);
	gtk_box_pack_start(GTK_BOX(vbox1), scroll1, TRUE, TRUE, 0);

	xmp_get_fmt_info (&fmt);
	table1 = gtk_table_new (100, 2, FALSE);
	for (i = 0, f = fmt; f; i++, f = f->next) {
		label_fmt = gtk_label_new(f->suffix);
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

//static GdkGC *gdkgc;
static GdkImage *image;
static GtkWidget *image1;
static GtkWidget *frame1;
#ifdef BMP_PLUGIN
static GtkWidget *text1v;
static GtkTextBuffer *text1b;
#else
static GtkWidget *text1;
#endif
static GdkFont *font;
static GdkColor *color_black;
static GdkColor *color_white;
static GdkColormap *colormap;
static XImage *ximage;
static Display *display;
static Window window;

static void stop (void)
{
	if (!xmp_going)
		return;

	_D("*** stop!");
	xmp_stop_module (); 
	ii->mode = 0;

	pthread_join (decode_thread, NULL);

	if (audio_open) {
        	xmp_ip.output->close_audio();
        	audio_open = FALSE;
    	}
}


static void seek (int time)
{
	int i, t;
	_D("seek to %d, total %d", time, xmp_cfg.time);

	time *= 1000;
	for (i = 0; i < xmp_cfg.mod_info.len; i++) {
		t = xxo_info[i].time;

		_D("%2d: %d %d", i, time, t);

		if (t > time) {
			int a;
			if (i > 0)
				i--;
			a = xmp_ord_set (i);
			xmp_ip.output->flush (xxo_info[i].time);
			break;
		}
	}
}


static void mod_pause (short p)
{
	ii->pause = p;
	xmp_ip.output->pause(p);
}


static int get_time (void)
{
	if (xmp_xmms_audio_error)
		return -2;
	if (!xmp_going)
		return -1;

	return xmp_ip.output->output_time();
}


InputPlugin *get_iplugin_info()
{
	return &xmp_ip;
}


static void driver_callback(void *b, int i)
{
	xmp_ip.add_vis_pcm (xmp_ip.output->written_time(),
			xmp_cfg.force8bit ? FMT_U8 : FMT_S16_NE,
			xmp_cfg.force_mono ? 1 : 2, i, b);
	
	while (xmp_ip.output->buffer_free() < i && xmp_going)
		xmms_usleep(10000);

	if(xmp_going)
		xmp_ip.output->write_audio(b, i);
}


static void init(void)
{
	ConfigFile *cfg;
	gchar *filename;

	xmp_cfg.mixing_freq = 0;
	xmp_cfg.convert8bit = 0;
	xmp_cfg.fixloops = 0;
	xmp_cfg.modrange = 0;
	xmp_cfg.force8bit = 0;
	xmp_cfg.force_mono = 0;
	xmp_cfg.interpolation = TRUE;
	xmp_cfg.filter = TRUE;
	xmp_cfg.pan_amplitude = 80;

#define CFGREADINT(x) xmms_cfg_read_int (cfg, "XMP", #x, &xmp_cfg.x)

#ifdef BMP_PLUGIN
	filename = g_strconcat(g_get_home_dir(), "/.bmp/config", NULL);
#else
	filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
#endif

	if ((cfg = xmms_cfg_open_file(filename))) {
		CFGREADINT (mixing_freq);
		CFGREADINT (force8bit);
		CFGREADINT (convert8bit);
		CFGREADINT (modrange);
		CFGREADINT (fixloops);
		CFGREADINT (force_mono);
		CFGREADINT (interpolation);
		CFGREADINT (filter);
		CFGREADINT (pan_amplitude);

		xmms_cfg_free(cfg);
	}

	file_info_box_build ();

	//xmp_drv_register (&drv_xmms);
	//xmp_init_formats ();

	memset (&ctl, 0, sizeof (struct xmp_control));
	xmp_init_callback (&ctl, driver_callback);
	xmp_register_event_callback (x11_event_callback);

	//xmp_drv_mutelloc (64);

	memset (ii, 0, sizeof (ii));
	ii->wresult = 42;
}


static int check_common_files(char *filename)
{
	char buf[4096], *x;
	FILE *f;

	if ((f = fopen(filename, "rb")) == NULL)
		return 0;

	fread(buf, 4096, 1, f);

	/* Check Protracker files */
	x = buf + 1080;
	if (!memcmp(x, "M.K.", 4))
		return 1;
	if (!memcmp(x, "M!K!", 4))
		return 1;
	if (!memcmp(x, "M&K!", 4))
		return 1;
	if (!memcmp(x, "N.T.", 4))
		return 1;
	if (!memcmp(x, "CD81", 4))
		return 1;

	/* Check FastTracker files */
	if (isdigit(*x) && !memcmp(x + 1, "CHN", 4))
		return 1;
	if (isdigit(*x) && isdigit(*(x + 1)) && !memcmp(x + 2, "CH", 4))
		return 1;

	/* Check Fasttracker II files */
	if (!memcmp(buf, "Extended module: ", 17))
		return 1;

	/* Check Scream Tracker 2 files */
	if (!memcmp(buf + 20, "!Scream!", 8))
		return 1;

	/* Check Scream Tracker 3 files */
	if (!memcmp(buf + 44, "SCRM", 4))
		return 1;

	/* Check Impulse tracker files */
	if (!memcmp(buf, "IMPM", 4))
		return 1;

	/* Check MTM files */
	if (!memcmp(buf, "MTM", 3))
		return 1;

	fclose(f);

	return 0;
}


static int is_our_file(char *filename)
{
	int i;

	/* Check some common module files by file magic. If not detected,
	 * try to load and discard file. But xmp can't load a module while
	 * another module is playing, so it can't check if the module is
	 * valid or not. In this case, we'll blindly accept the file.
	 */

	if (check_common_files(filename))
		return 1;

	if (xmp_going)
		return 1;

	pthread_mutex_lock (&load_mutex);

	/* xmp needs the audio device to be set before loading a file */
	ctl.maxvoc = 16;
	ctl.verbose = 0;
	ctl.memavl = 0;
	xmp_drv_set(&ctl);

	_D("checking: %s", filename);
	if ((i = xmp_load_module(filename)) >= 0)
		xmp_release_module();		/* Fixes memory leak */
	_D("  returned %d", i);

	pthread_mutex_unlock (&load_mutex);

	return i;
}


static void get_song_info(char *filename, char **title, int *length)
{
	char *x;

	/* Ugh. xmp doesn't allow me to load and scan a module while
	 * playing. Must fix this.
	 */
	_D("song_info: %s", filename);
	if ((x = strrchr (filename, '/')))
		filename = ++x;
	*title = g_strdup (filename);
}

static int fd_old2, fd_info[2];
static pthread_t catch_thread;

void *catch_info (void *arg)
{
	FILE *f;
	char buf[100];
	GtkTextIter end;
	GtkTextTag *tag;

	f = fdopen(fd_info[0], "r");

	while (!feof (f)) {
		fgets (buf, 100, f);
#ifdef BMP_PLUGIN
		gtk_text_buffer_get_end_iter(text1b, &end);
		tag = gtk_text_buffer_create_tag(text1b, NULL,
						"foreground", "black", NULL);
		gtk_text_buffer_insert_with_tags(text1b, &end, buf, -1,
								tag, NULL);
#else
		gtk_text_insert(GTK_TEXT(text1), font,
			color_black, color_white, buf, strlen(buf));
#endif
		if (!strncmp(buf, "Estimated time :", 16))
			break;
	}

	fclose (f);

	pthread_exit (NULL);
}


static void play_file (char *filename)
{
	int channelcnt = 1;
	int format = FMT_U8;
	FILE *f;
	char *info;
	
	_D("play_file: %s", filename);

	stop();		/* sanity check */

	if ((f = fopen(filename,"rb")) == 0) {
		xmp_going = 0;
		return;
	}
	fclose(f);

#ifdef BMP_PLUGIN
	gtk_text_buffer_set_text(text1b, 0, -1);
#else
	gtk_text_set_point (GTK_TEXT(text1), 0);
	gtk_text_forward_delete (GTK_TEXT(text1),
		gtk_text_get_length (GTK_TEXT(text1)));
#endif
	
	xmp_xmms_audio_error = FALSE;
	xmp_going = 1;

	pthread_mutex_lock (&load_mutex);
	ctl.resol = 8;
	ctl.verbose = 3;
	ctl.drv_id = "callback";

	switch (xmp_cfg.mixing_freq) {
	case 1:
		ctl.freq = 22050;	/* 1:2 mixing freq */
		break;
	case 2:
		ctl.freq = 11025;	/* 1:4 mixing freq */
		break;
	default:
		ctl.freq = 44100;	/* standard mixing freq */
		break;
	}

	if ((xmp_cfg.force8bit == 0)) {
		format = FMT_S16_NE;
		ctl.resol = 16;
	}

	if ((xmp_cfg.force_mono == 0)) {
		channelcnt = 2;
	} else {
		ctl.outfmt |= XMP_FMT_MONO;
	}

	if ((xmp_cfg.interpolation == 1))
		ctl.flags |= XMP_CTL_ITPT;

	if ((xmp_cfg.filter == 1))
		ctl.flags |= XMP_CTL_FILTER;

	ctl.mix = xmp_cfg.pan_amplitude;

	{
	    AFormat fmt;
	    int nch;
	
	    fmt = ctl.resol == 16 ? FMT_S16_NE : FMT_U8;
	    nch = ctl.outfmt & XMP_FMT_MONO ? 1 : 2;
	
	    if (audio_open)
		xmp_ip.output->close_audio();
	
	    if (!xmp_ip.output->open_audio(fmt, ctl.freq, nch)) {
		xmp_xmms_audio_error = TRUE;
		return;
	    }
	
	    audio_open = TRUE;
	
	    //return xmp_smix_on (ctl);
	}

	xmp_open_audio (&ctl);

	pipe (fd_info);
	fd_old2 = dup (fileno (stderr));
	dup2 (fd_info[1], fileno (stderr));
	fflush (stderr);
	pthread_create (&catch_thread, NULL, catch_info, NULL);

	_D("*** loading: %s", filename);
	if (xmp_load_module (filename) < 0) {
		xmp_ip.set_info_text("Error loading mod");
		xmp_going = 0;
		return;
	}

	_D("joining catch thread");
	pthread_join (catch_thread, NULL);
	_D("joined");
	dup2 (fileno (stderr), fd_old2);

#ifdef BMP_PLUGIN
	gtk_adjustment_set_value(GTK_TEXT_VIEW(text1v)->vadjustment, 0.0);
#else
	gtk_adjustment_set_value(GTK_TEXT(text1)->vadj, 0.0);
#endif

	close (fd_info[0]);
	close (fd_info[1]);

	_D ("before panel update");

	xmp_cfg.time = xmpi_scan_module ();
	xmp_get_module_info (&ii->mi);
	strcpy (ii->filename, "");

	new_module = 1;

	_D("after panel update");

	memcpy (&xmp_cfg.mod_info, &ii->mi, sizeof (ii->mi));

	pthread_mutex_unlock (&load_mutex);

	info = malloc (strlen (ii->mi.name) + strlen (ii->mi.type) + 20);
	sprintf (info, "%s [%s, %d ch]", ii->mi.name, ii->mi.type, ii->mi.chn);

	xmp_ip.set_info(info, xmp_cfg.time, 128 * 1000, ctl.freq, channelcnt);
	free (info);

	_D("--- pthread_create");
	pthread_create(&decode_thread, NULL, play_loop, NULL);

	return;
}


static void *play_loop (void *arg)
{
	xmp_play_module();
	xmp_release_module();
	xmp_close_audio();
	xmp_going = 0;

	_D("--- pthread_exit");

	pthread_exit(NULL);
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
#ifdef BMP_PLUGIN
	xmp_conf_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#else
	xmp_conf_window = gtk_window_new(GTK_WINDOW_DIALOG);
#endif

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

	Sample_22 = gtk_radio_button_new_with_label(sample_group, "22 kHz)");
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
	ConfigFile *cfg;
	gchar *filename;

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
        ctl.mix = xmp_cfg.pan_amplitude;

	filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
	cfg = xmms_cfg_open_file(filename);
	if (!cfg)
		cfg = xmms_cfg_new();

#define CFGWRITEINT(x) xmms_cfg_write_int (cfg, "XMP", #x, xmp_cfg.x)

	CFGWRITEINT (mixing_freq);
	CFGWRITEINT (force8bit);
	CFGWRITEINT (convert8bit);
	CFGWRITEINT (modrange);
	CFGWRITEINT (fixloops);
	CFGWRITEINT (force_mono);
	CFGWRITEINT (interpolation);
	CFGWRITEINT (filter);
	CFGWRITEINT (pan_amplitude);

	xmms_cfg_write_file(cfg, filename);
	xmms_cfg_free(cfg);
	g_free(filename);
	gtk_widget_destroy(xmp_conf_window);
}


static void button_cycle (GtkWidget *widget, GdkEvent *event)
{
     ii->mode++;
     ii->mode %= 3;
}

static void button_mute (GtkWidget *widget, GdkEvent *event)
{
	int i;

	if (!xmp_going)
		return;

	xmp_channel_mute (0, 64, 1);
	for (i = 0; i < ii->mi.chn; i++)
		ii->mute[i] = 1;
}

static void button_unmute (GtkWidget *widget, GdkEvent *event)
{
	int i;

	if (!xmp_going)
		return;

	xmp_channel_mute (0, 64, 0);
	for (i = 0; i < ii->mi.chn; i++)
		ii->mute[i] = 0;
}

static int image1_clicked_x = 0;
static int image1_clicked_y = 0;
static int image1_clicked_ok = 0;

static void image1_clicked(GtkWidget *widget, GdkEventButton *event)
{
	if (!xmp_going || image1_clicked_ok)
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

	visual = gdk_visual_get_system ();

	/*
	 * Image
	 */

	frame1 = gtk_event_box_new();
	gtk_object_set_data(GTK_OBJECT(frame1), "frame1", frame1);
	gtk_widget_set_size_request(frame1, 300, 128);
	gtk_box_pack_start(GTK_BOX(vbox1), frame1, FALSE, FALSE, 0);

	image = gdk_image_new(GDK_IMAGE_FASTEST, visual, 300, 128);
	ximage = GDK_IMAGE_XIMAGE(image);
#ifdef BMP_PLUGIN
	image1 = gtk_image_new_from_image(image, NULL);
#else
	image1 = gtk_image_new(image, NULL);
#endif
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

	scrw1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_object_set_data(GTK_OBJECT(scrw1), "scrw1", scrw1);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrw1), GTK_POLICY_ALWAYS, GTK_POLICY_AUTOMATIC);

	gtk_widget_set_size_request(scrw1, 290, 250);

	gtk_container_add (GTK_CONTAINER(expander), scrw1);

	gtk_box_pack_start(GTK_BOX(vbox1), expander, TRUE, TRUE, 0);

#ifdef BMP_PLUGIN
	text1b = gtk_text_buffer_new(NULL);
	text1v = gtk_text_view_new_with_buffer(text1b);
	//gtk_object_set_data(GTK_OBJECT(text1b), "text1b", text1b);
	gtk_object_set_data(GTK_OBJECT(text1v), "text1v", text1v);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text1v), GTK_WRAP_NONE);
	gtk_container_add (GTK_CONTAINER(scrw1), text1v);
	gtk_widget_realize (text1v);
#else
	text1 = gtk_text_new(NULL, NULL);
	gtk_object_set_data(GTK_OBJECT(text1), "text1", text1);
	gtk_text_set_line_wrap (GTK_TEXT(text1), FALSE);
	gtk_widget_set (text1, "height", 160, "width", 290, NULL);
	gtk_container_add (GTK_CONTAINER(scrw1), text1);
	gtk_widget_realize (text1);
#endif

	gtk_widget_realize (image1);

	display = GDK_WINDOW_XDISPLAY (info_window->window);
	window = GDK_WINDOW_XWINDOW (info_window->window);
    	colormap = gdk_colormap_get_system ();

	font = gdk_font_load ("fixed");
	gdk_color_black (colormap, color_black);
	gdk_color_white (colormap, color_white);


	init_visual (visual);


	set_palette ();
	clear_screen ();

	ii->wresult = 0;

	panel_setup ();
	gtk_timeout_add (50, (GtkFunction)panel_loop, NULL);
}


static void file_info_box (char *filename)
{
	_D ("Info box");

	gtk_widget_show_all(info_window);
	gdk_window_raise(info_window->window);
}

/*----------------------------------------------------------------------*/

#define alloc_color(d,c,x) \
gdk_colormap_alloc_color(colormap, (GdkColor *)x, TRUE, TRUE)

#include "xstuff.c"


static void init_visual (GdkVisual *visual)
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


void putimage (int x, int y, int w, int h)
{
    //gdk_draw_image (image1->parent->window, gdkgc, image, x, y, x, y, w, h);
}


#if 0
static gint expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	return 0;
}
#endif

void update_display ()
{
#ifdef BMP_PLUGIN
	GdkRectangle area;

	area.x = (frame1->allocation.width - 300) / 2;
	area.y = 0;
	area.width = 300;
	area.height = 128;

	gdk_window_invalidate_rect(image1->window, &area, FALSE);
#else
	GdkEventExpose e;

	e.type = GDK_EXPOSE;
	e.window = image1->window;
	e.send_event = TRUE;
	e.area.x = 10;
	e.area.y = 10;
	e.area.width = 10;
	e.area.height = 10;
	e.count = 0;

	gdk_event_put((GdkEvent *)&e);
	//XSync (display, False);
#endif

}


#if 0
void setpalette (char **bg)
{
    int i;
    unsigned long rgb;

    _D ("setting image palette");

	mask_r = 0xf00000;
	mask_g = 0x00f800;
	mask_b = 0x0000f0;
	draw_rectangle = draw_rectangle_rgb16;
	erase_rectangle = erase_rectangle_rgb16;
	//indexed = 0;

    color[0].red = color[0].green = color[0].blue = 0x02;
    color[1].red = color[1].green = color[1].blue = 0xfe;
    color[2].red = color[2].green = color[2].blue = 0xd0;

    for (i = 4; i < 12; i++) {
	rgb = strtoul (&bg[i - 3][5], NULL, 16);
	color[i].red = (rgb & mask_r) >> 16;
	color[i].green = (rgb & mask_g) >> 8;
	color[i].blue = rgb & mask_b;
	color[i + 8].red = color[i].red >> 1;
	color[i + 8].green = color[i].green >> 1;
	color[i + 8].blue = color[i].blue >> 1;
    }

    for (i = 0; i < 20; i++) {
	color[i].red <<= 8;
	color[i].green <<= 8;
	color[i].blue <<= 8;
	//if (!XAllocColor (display, colormap, &color[i]))
	 //   fprintf (stderr, "cannot allocte color cell\n");
	if (!gdk_colormap_alloc_color (colormap, (GdkColor *)&color[i], TRUE, TRUE))
	    fprintf (stderr, "cannot allocte color cell\n");
    }

#if 0
    if (indexed) {
	for (i = 0; i < 3; i++)
	    pmap[color[i].pixel] = color[i].pixel;
	for (i = 4; i < 12; i++)
	    pmap[color[i].pixel] = color[i + 8].pixel;
	for (i = 12; i < 20; i++)
	    pmap[color[i].pixel] = color[i - 8].pixel;
    }
#endif
}
#endif



int process_events (int *x, int *y)
{
	if (image1_clicked_ok) {
		*x = image1_clicked_x;
		*y = image1_clicked_y;
		image1_clicked_ok = 0;
		return -1;
	}

	return 0;
}


void settitle (char *title)
{
}


