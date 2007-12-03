/*
 * XMP plugin for WinAmp
 *
 * $Id: winamp.c,v 1.33 2007-12-03 00:01:36 cmatsuoka Exp $
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmreg.h>
#include <msacm.h>
#include <math.h>

#include "xmpi.h"
#include "in2.h"
#include "resource.h"

#define WM_WA_MPEG_EOF (WM_USER + 2)

// avoid CRT. Evil. Big. Bloated.
BOOL WINAPI
_DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

static DWORD WINAPI __stdcall play_loop(void *);

static int stopped;
static int paused;
HANDLE decode_thread = INVALID_HANDLE_VALUE;
HANDLE load_mutex;

#define FREQ_SAMPLE_44 0
#define FREQ_SAMPLE_22 1
#define FREQ_SAMPLE_11 2

/* x2 because mod.dsp_dosamples() may use up to twice as much space */
#define MIX_BUFSIZE (20000 * 2)

typedef struct {
	int mixing_freq;
	int force_mono;
	int interpolation;
	int filter;
	int fixloops;
	int loop;
	int pan_amplitude;
	int time;
	struct xmp_module_info mod_info;
} XMPConfig;

XMPConfig xmp_cfg;
static int xmp_plugin_audio_error = FALSE;
static short audio_open = FALSE;
static xmp_context ctx;
static int playing;
static struct xmp_options *opt = NULL;

static char mix_buffer[MIX_BUFSIZE];

static void	config		(HWND);
static void	about		(HWND);
static void	init		(void);
static void	quit		(void);
static void	get_file_info	(char *, char *, int *);
static int	infoDlg		(char *, HWND);
static int	is_our_file	(char *);
static int	play_file	(char *);
static void	pause		(void);
static void	unpause		(void);
static int	is_paused	(void);
static void	stop		(void);
static int	getlength	(void);
static int	getoutputtime	(void);
static void	setoutputtime	(int);
static void	setvolume	(int);
static void	setpan		(int);
static void	eq_set		(int, char[10], int);

In_Module mod = {
	IN_VER,
	"XMP Plugin " VERSION,
	0,			/* hMainWindow */
	0,			/* hDllInstance */
	"xm;mod;flt;st;m15;it;s3m;stm;stx;mtm;ice;imf;ptm;mdl;"
	"ult;liq;psm;amf;mmd0;mmd1;mmd2;mmd3;med;med3;med4;dmf;"
	"rtm;pt3;tcb;dt;gtk;dtt;mgt;arch;dsym;digi;dbm;emod;okt;"
	"sfx;far;umx;stim;mtp;ims;669;fnk;funk;amd;rad;hsc;alm;"
	"ac1d;fchs;fcm;fuzz;kris;ksm;mp;p18a;p10c;pru1;pru2;pha;"
	"wn;unic;tp3;xann;di;eu;p4x;p40;p41;p60a;np1;np2;np3;zen;"
	"crb;tdd;gmc;gdm\0"
	"Module formats\0",	/* file extensions */
	1,			/* is_seekable */
	1,			/* uses output */
	config,
	about,
	init,
	quit,
	get_file_info,
	infoDlg,
	is_our_file,
	play_file,
	pause,
	unpause,
	is_paused,
	stop,
	getlength,
	getoutputtime,
	setoutputtime,
	setvolume,
	setpan,
	0, 0, 0, 0, 0, 0, 0, 0, 0,	// vis stuff
	0, 0,			// dsp
	eq_set, NULL,		// setinfo
	0			// out_mod
};

__declspec(dllexport) In_Module *winampGetInModule2()
{
	return &mod;
}


static char *get_inifile(char *s)
{
	char *c;

	if (GetModuleFileName(GetModuleHandle("in_xmp.dll"), s, MAX_PATH)) {
		if ((c = strrchr(s, '\\')) != NULL)
			*++c = 0;
                strncat(s, "plugin.ini", MAX_PATH);
        }

	return s;
}


static void stop()
{
	if (!playing)
		return;

	_D(_D_CRIT "stop!");

	stopped = 1;
	xmp_stop_module(ctx);

	if (decode_thread != INVALID_HANDLE_VALUE) {
		if (WaitForSingleObject(decode_thread, INFINITE) ==
		    WAIT_TIMEOUT) {
			MessageBox(mod.hMainWindow,
				   "error asking thread to die!\n",
				   "error killing decode thread", 0);
			TerminateThread(decode_thread, 0);
		}
		CloseHandle(decode_thread);
		decode_thread = INVALID_HANDLE_VALUE;
	}
	mod.outMod->Close();
	mod.SAVSADeInit();
}

static void driver_callback(void *b, int i)
{
	int dsp = mod.dsp_isactive();
	int n = i * (dsp ? 2 : 1);
	int numch = opt->outfmt & XMP_FMT_MONO ? 1 : 2;
	int ssize = opt->resol / 8;
	int t;

	while (mod.outMod->CanWrite() < n)
		Sleep(50);

	t = mod.outMod->GetWrittenTime();
	mod.SAAddPCMData(b, numch, opt->resol, t);
	mod.VSAAddPCMData(b, numch, opt->resol, t);

	if (dsp) {
		memcpy(mix_buffer, b, i);
		n = mod.dsp_dosamples((short *)mix_buffer, i / numch / ssize,
				opt->resol, numch, opt->freq) * numch * ssize;
		b = mix_buffer;
	}

	mod.outMod->Write(b, n);
}

static BOOL CALLBACK config_dialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char inifile[MAX_PATH];
	HANDLE sld, cmb;
	char *freq[] = { "44100", "22050", "11025", 0 };
	int i, fidx;
	char buffer[20];

#define CFGWRITESTR(x) do { \
	char buf[16]; \
	snprintf(buf, 16, "%d", xmp_cfg.x); \
	WritePrivateProfileString("XMP", #x, buf, inifile); \
} while (0)

	switch (uMsg) {
	case WM_CLOSE:
		EndDialog(hDlg,TRUE);
		return 0;
	case WM_INITDIALOG:
		if (xmp_cfg.loop)
			CheckDlgButton(hDlg, IDC_LOOP, BST_CHECKED);
		if (xmp_cfg.force_mono)
			CheckDlgButton(hDlg, IDC_FORCE_MONO, BST_CHECKED);
		if (xmp_cfg.interpolation)
			CheckDlgButton(hDlg, IDC_INTERPOLATION, BST_CHECKED);
		if (xmp_cfg.filter)
			CheckDlgButton(hDlg, IDC_FILTER, BST_CHECKED);

		sld = GetDlgItem(hDlg, IDC_PAN_AMPLITUDE);
		SendMessage(sld, TBM_SETRANGE, (WPARAM)TRUE,
					(LPARAM)MAKELONG(0, 10));
		SendMessage(sld, TBM_SETPOS, (WPARAM)TRUE,
					(LPARAM)xmp_cfg.pan_amplitude / 10);

		cmb = GetDlgItem(hDlg, IDC_MIXING_FREQ);
		SendMessage(cmb, CB_RESETCONTENT, 0, 0);

		for (i = 0; freq[i]; i++)
			SendMessage(cmb, CB_ADDSTRING, i, (LPARAM)freq[i]);
		snprintf(buffer, 20, "%d", xmp_cfg.mixing_freq);
		fidx = SendMessage(cmb, CB_FINDSTRINGEXACT, -1, (LPARAM)buffer);
		if (fidx == CB_ERR)
			fidx = 0;
		SendMessage(cmb, CB_SETCURSEL, fidx, 0);
		xmp_cfg.mixing_freq = atoi(freq[fidx]);
		break;
	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, lParam)) {
		case IDOK:
			xmp_cfg.mixing_freq = atoi(freq[SendMessage(GetDlgItem(
				hDlg, IDC_MIXING_FREQ), CB_GETCURSEL, 0, 0)]);
			xmp_cfg.loop = (IsDlgButtonChecked(hDlg,
				IDC_LOOP) == BST_CHECKED);
			xmp_cfg.force_mono = (IsDlgButtonChecked(hDlg,
				IDC_FORCE_MONO) == BST_CHECKED);
			xmp_cfg.interpolation = (IsDlgButtonChecked(hDlg,
				IDC_INTERPOLATION) == BST_CHECKED);
			xmp_cfg.filter = (IsDlgButtonChecked(hDlg,
				IDC_FILTER) == BST_CHECKED);
			xmp_cfg.pan_amplitude = SendMessage(GetDlgItem(hDlg,
				IDC_PAN_AMPLITUDE), TBM_GETPOS, 0, 0) * 10;
				
			get_inifile(inifile);
			CFGWRITESTR(mixing_freq);
			CFGWRITESTR(loop);
			CFGWRITESTR(force_mono);
			CFGWRITESTR(interpolation);
			CFGWRITESTR(filter);
			CFGWRITESTR(pan_amplitude);

			if (opt) {
				if (xmp_cfg.loop)
					opt->flags |= XMP_CTL_LOOP;
				else
					opt->flags &= ~XMP_CTL_LOOP;
			}

			/* fall thru */
		case IDCANCEL:
			EndDialog(hDlg,TRUE);
			break;
		}
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

static void config(HWND hwndParent)
{
	DialogBox(mod.hDllInstance, (const char *)IDD_CONFIG, hwndParent,
							config_dialog);
}

static void about(HWND hwndParent)
{
	MessageBox(hwndParent,
		"Extended Module Player " VERSION "\n"
		"Written by Claudio Matsuoka and Hipolito Carraro Jr.\n"
		"\n"
		"Portions Copyright (C) 1998,2000 Olivier Lapicque,\n"
		"(C) 1998 Tammo Hinrichs, (C) 1998 Sylvain Chipaux,\n"
		"(C) 1997 Bert Jahn, (C) 1999 Tatsuyuki Satoh, (C)\n"
		"2001-2006 Russell Marks\n"
		, "About XMP", MB_OK);
}


static void init()
{
	static char inifile[MAX_PATH];

	ctx = xmp_create_context();

#define CFGREADINT(x,y) do { \
	xmp_cfg.x = GetPrivateProfileInt("XMP", #x, y, inifile); \
} while (0)

	get_inifile(inifile);
	
	CFGREADINT(mixing_freq, 44100);
	CFGREADINT(loop, 0);
	CFGREADINT(fixloops, 0);
	CFGREADINT(force_mono, 0);
	CFGREADINT(interpolation, 1);
	CFGREADINT(filter, 1);
	CFGREADINT(pan_amplitude, 80);

	xmp_init_callback(ctx, driver_callback);
	//xmp_register_event_callback(x11_event_callback);
}

static void quit()
{
}

static int is_our_file(char *fn)
{
	_D(_D_WARN "fn = %s", fn);
	if (xmp_test_module(ctx, fn, NULL) == 0)
		return 1;

	return 0;
}

static int play_file(char *fn)
{
	int maxlatency;
	DWORD tmp;
	FILE *f;
	int lret, numch;

	_D("fn = %s", fn);

	opt = xmp_get_options(ctx);

	stop();				/* sanity check */

	if ((f = fopen(fn, "rb")) == NULL) {
		playing = 0;
		return -1;
	}
	fclose(f);

	xmp_plugin_audio_error = FALSE;
	playing = 1;

	opt->resol = 16;
	opt->verbosity = 0;
	opt->drv_id = "callback";

	opt->freq = xmp_cfg.mixing_freq;

	if (xmp_cfg.loop)
		opt->flags |= XMP_CTL_LOOP;
	else
		opt->flags &= ~XMP_CTL_LOOP;

	if (xmp_cfg.force_mono)
		opt->outfmt |= XMP_FMT_MONO;
	else
		opt->outfmt &= ~XMP_FMT_MONO;

	if (xmp_cfg.interpolation)
		opt->flags |= XMP_CTL_ITPT;
	else
		opt->flags &= ~XMP_CTL_ITPT;

	if (xmp_cfg.filter)
		opt->flags |= XMP_CTL_FILTER;
	else
		opt->flags &= ~XMP_CTL_FILTER;

	opt->mix = xmp_cfg.pan_amplitude;

	numch = opt->outfmt & XMP_FMT_MONO ? 1 : 2;

	if (audio_open)
		mod.outMod->Close();
	audio_open = TRUE;

	paused = 0;

	maxlatency = mod.outMod->Open(opt->freq, numch, opt->resol, -1, -1);
	if (maxlatency < 0)
		return 1;
	mod.SetInfo(0, opt->freq / 1000, 1, 1);
	mod.SAVSAInit(maxlatency, opt->freq);
	mod.VSASetInfo(numch, opt->freq);
	mod.outMod->SetVolume(-666);

	xmp_open_audio(ctx);

	load_mutex = CreateMutex(NULL, TRUE, "load_mutex");
	lret = xmp_load_module(ctx, fn);
	ReleaseMutex(load_mutex);

	xmp_cfg.time = lret;
	xmp_get_module_info(ctx, &xmp_cfg.mod_info);

	stopped = 0;
	decode_thread = (HANDLE)CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)play_loop,
			NULL, 0, &tmp);
	return 0;
}

static DWORD WINAPI __stdcall play_loop(void *b)
{
	_D(_D_CRIT "play");
	xmp_play_module(ctx);
	_D(_D_CRIT "end play");
	xmp_release_module(ctx);
	xmp_close_audio(ctx);
	playing = 0;

	if (!stopped)
		PostMessage(mod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);

	return 0;
}

static void pause()
{
	paused = 1;
	mod.outMod->Pause(1);
}

static void unpause()
{
	paused = 0;
	mod.outMod->Pause(0);
}

static int is_paused()
{
	return paused;
}

static int getlength()
{
	return xmp_cfg.time;
}

static int getoutputtime()
{
	if (!playing)
		return -1;

	return mod.outMod->GetOutputTime();
}

static void setoutputtime(int time)
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
			mod.outMod->Flush(p->m.xxo_info[i].time);
			break;
		}
	}
}

static void setvolume(int volume)
{
	mod.outMod->SetVolume(volume);
}

static void setpan(int pan)
{
	mod.outMod->SetPan(pan);
}

static int infoDlg(char *fn, HWND hwnd)
{
	return 0;
}

static void get_file_info(char *filename, char *title, int *length_in_ms)
{
	xmp_context ctx2;
	int lret;
	struct xmp_module_info mi;
	struct xmp_options *opt;

	_D(_D_WARN "filename = %s", filename);

	/* Winamp docs say "if filename == NULL, current playing is used"
	 * but it seems to pass an empty filename. I'll test for both
	 */
	if (filename == NULL || strlen(filename) == 0) {
		xmp_get_module_info(ctx, &mi);
		wsprintf(title, "%s", mi.name);
		return;
	}

	/* Create new context to load a file and get the length */
	
	ctx2 = xmp_create_context();
	opt = xmp_get_options(ctx2);
	opt->skipsmp = 1;	/* don't load samples */

	load_mutex = CreateMutex(NULL, TRUE, "load_mutex");
	lret = xmp_load_module(ctx2, filename);
	ReleaseMutex(load_mutex);

	if (lret < 0) {
		_D("free context");
		xmp_free_context(ctx2);
		return;
        }

	*length_in_ms = lret;
	xmp_get_module_info(ctx2, &mi);
	wsprintf(title, "%s", mi.name);

        xmp_release_module(ctx2);
        xmp_free_context(ctx2);
}

static void eq_set(int on, char data[10], int preamp)
{
}

