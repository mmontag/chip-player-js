#ifdef _WIN32
//#define _WIN32_WINNT	0x500	// for GetConsoleWindow()
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <string>

#ifdef _WIN32
extern "C" int __cdecl _getch(void);	// from conio.h
#else
#include <unistd.h>		// for STDIN_FILENO and usleep()
#include <termios.h>
#include <sys/time.h>	// for struct timeval in _kbhit()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

#include <common_def.h>
#include "player/s98player.hpp"
#include "audio/AudioStream.h"
#include "audio/AudioStream_SpcDrvFuns.h"
#include "emu/Resampler.h"


int main(int argc, char* argv[]);
static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* Data);
static UINT8 FilePlayCallback(void* userParam, UINT8 evtType, void* evtParam);
#ifndef _WIN32
static void changemode(UINT8 noEcho);
static int _kbhit(void);
#define	_getch	getchar
#endif


static UINT32 smplSize;
static void* audDrv;
static void* audDrvLog;
static UINT32 smplAlloc;
static WAVE_32BS* smplData;
static volatile bool canRender;

static UINT32 sampleRate;
static UINT32 maxLoops = 2;
static UINT8 playState;

int main(int argc, char* argv[])
{
	UINT8 retVal;
	UINT32 drvCount;
	UINT32 idWavOut;
	UINT32 idWavOutDev;
	UINT32 idWavWrt;
	AUDDRV_INFO* drvInfo;
	AUDIO_OPTS* opts;
	S98Player s98play;
	
	if (argc < 2)
	{
		printf("Usage: %s inputfile\n", argv[0]);
		return 0;
	}
	
	s98play.SetCallback(&FilePlayCallback, NULL);
	
	printf("Loading S98 ...\n");
	retVal = s98play.LoadFile(argv[1]);
	if (retVal)
	{
		printf("Error 0x%02X loading S98 file!\n", retVal);
		return 2;
	}
	printf("S98 v%u, Tick Rate: %u/%u\n", s98play.GetFileHeader()->fileVer,
		s98play.GetFileHeader()->tickMult, s98play.GetFileHeader()->tickDiv);
	
	printf("Opening Audio Device ...\n");
	Audio_Init();
	drvCount = Audio_GetDriverCount();
	if (! drvCount)
		goto Exit_AudDeinit;
	
	idWavOut = 2;
	idWavOutDev = 0;
	idWavWrt = (UINT32)-1;
	//idWavWrt = 0;
	
	Audio_GetDriverInfo(idWavOut, &drvInfo);
	printf("Using driver %s.\n", drvInfo->drvName);
	retVal = AudioDrv_Init(idWavOut, &audDrv);
	if (retVal)
	{
		printf("WaveOut: Drv Init Error: %02X\n", retVal);
		goto Exit_AudDeinit;
	}
	
	sampleRate = 44100;
	
	opts = AudioDrv_GetOptions(audDrv);
	opts->sampleRate = sampleRate;
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	
	canRender = false;
	AudioDrv_SetCallback(audDrv, FillBuffer, &s98play);
	printf("Opening Device %u ...\n", idWavOutDev);
	retVal = AudioDrv_Start(audDrv, idWavOutDev);
	if (retVal)
	{
		printf("Dev Init Error: %02X\n", retVal);
		goto Exit_SndDrvDeinit;
	}
	
	smplAlloc = AudioDrv_GetBufferSize(audDrv) / smplSize;
	smplData = (WAVE_32BS*)malloc(smplAlloc * sizeof(WAVE_32BS));
	
	audDrvLog = NULL;
	if (idWavWrt != (UINT32)-1)
	{
		retVal = AudioDrv_Init(idWavWrt, &audDrvLog);
		if (retVal)
			audDrvLog = NULL;
	}
	if (audDrvLog != NULL)
	{
		void* aDrv = AudioDrv_GetDrvData(audDrvLog);
		WavWrt_SetFileName(aDrv, "waveOut.wav");
		retVal = AudioDrv_Start(audDrvLog, 0);
		if (retVal)
			AudioDrv_Deinit(&audDrvLog);
	}
	if (audDrvLog != NULL)
		AudioDrv_DataForward_Add(audDrv, audDrvLog);
	
	s98play.SetSampleRate(sampleRate);
	s98play.Start();
	
	canRender = true;
	printf("Song Title: %s\n", s98play.GetSongTitle());
#ifndef _WIN32
	changemode(1);
#endif
	playState = 0x00;
	while(! (playState & PLAYSTATE_END))
	{
		printf("Playing %.2f ...   \r", s98play.GetCurSample() / (double)s98play.GetSampleRate());
		fflush(stdout);
		Sleep(50);
		
		if (_kbhit())
		{
			int inkey = _getch();
			int letter = toupper(inkey);
			
			if (letter == ' ' || letter == 'P')
			{
				playState ^= PLAYSTATE_PAUSE;
				if (playState & PLAYSTATE_PAUSE)
					AudioDrv_Pause(audDrv);
				else
					AudioDrv_Resume(audDrv);
			}
			else if (inkey == 0x1B || letter == 'Q')
			{
				playState |= PLAYSTATE_END;
			}
		}
	}
#ifndef _WIN32
	changemode(0);
#endif
	canRender = false;
	
	retVal = AudioDrv_Stop(audDrv);
	if (audDrvLog != NULL)
		retVal = AudioDrv_Stop(audDrvLog);
	free(smplData);	smplData = NULL;
	
Exit_SndDrvDeinit:
	s98play.Stop();
	s98play.UnloadFile();
//Exit_AudDrvDeinit:
	AudioDrv_Deinit(&audDrv);
	if (audDrvLog != NULL)
		AudioDrv_Deinit(&audDrvLog);
Exit_AudDeinit:
	Audio_Deinit();
	printf("Done.\n");
	
#if defined(_DEBUG) && (_MSC_VER >= 1400)
	// doesn't work well with C++ containers
	//if (_CrtDumpMemoryLeaks())
	//	_getch();
#endif
	
	return 0;
}

static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* data)
{
	S98Player* s98play = (S98Player*)userParam;
	UINT32 smplCount;
	UINT32 smplRendered;
	INT16* SmplPtr16;
	UINT32 curSmpl;
	WAVE_32BS fnlSmpl;
	
	smplCount = bufSize / smplSize;
	if (! smplCount)
		return 0;
	
	if (! canRender)
	{
		memset(data, 0x00, smplCount * smplSize);
		return smplCount * smplSize;
	}
	
	if (smplCount > smplAlloc)
		smplCount = smplAlloc;
	memset(smplData, 0, smplCount * sizeof(WAVE_32BS));
	smplRendered = s98play->Render(smplCount, smplData);
	smplCount = smplRendered;
	
	SmplPtr16 = (INT16*)data;
	for (curSmpl = 0; curSmpl < smplCount; curSmpl ++, SmplPtr16 += 2)
	{
		fnlSmpl.L = smplData[curSmpl].L >> 8;
		fnlSmpl.R = smplData[curSmpl].R >> 8;
		if (fnlSmpl.L < -0x8000)
			fnlSmpl.L = -0x8000;
		else if (fnlSmpl.L > +0x7FFF)
			fnlSmpl.L = +0x7FFF;
		if (fnlSmpl.R < -0x8000)
			fnlSmpl.R = -0x8000;
		else if (fnlSmpl.R > +0x7FFF)
			fnlSmpl.R = +0x7FFF;
		SmplPtr16[0] = (INT16)fnlSmpl.L;
		SmplPtr16[1] = (INT16)fnlSmpl.R;
	}
	
	return curSmpl * smplSize;
}

static UINT8 FilePlayCallback(void* userParam, UINT8 evtType, void* evtParam)
{
	switch(evtType)
	{
	case PLREVT_START:
		printf("Playback started.\n");
		break;
	case PLREVT_STOP:
		printf("Playback stopped.\n");
		break;
	case PLREVT_LOOP:
		{
			UINT32* curLoop = (UINT32*)evtParam;
			printf("Loop %u.\n", *curLoop);
			if (*curLoop >= maxLoops)
			{
				playState |= PLAYSTATE_END;
				return 0x01;
			}
		}
		break;
	case PLREVT_END:
		playState |= PLAYSTATE_END;
		printf("Song End.\n");
		break;
	}
	return 0x00;
}



#ifndef _WIN32
static struct termios oldterm;
static UINT8 termmode = 0xFF;

static void changemode(UINT8 noEcho)
{
	if (termmode == 0xFF)
	{
		tcgetattr(STDIN_FILENO, &oldterm);
		termmode = 0;
	}
	if (termmode == noEcho)
		return;
	
	if (noEcho)
	{
		struct termios newterm;
		newterm = oldterm;
		newterm.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newterm);
		termmode = 1;
	}
	else
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
		termmode = 0;
	}
	
	return;
}

static int _kbhit(void)
{
	struct timeval tv;
	fd_set rdfs;
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO, &rdfs);
	select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
	
	return FD_ISSET(STDIN_FILENO, &rdfs);;
}
#endif

