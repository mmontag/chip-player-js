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
static const char* GetFileTitle(const char* filePath);
static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* Data);
static UINT8 FilePlayCallback(void* userParam, UINT8 evtType, void* evtParam);
static UINT8 InitAudioSystem(void);
static UINT8 DeinitAudioSystem(void);
static UINT8 StartAudioDevice(void);
static UINT8 StopAudioDevice(void);
static UINT8 StartDiskWriter(const char* fileName);
static UINT8 StopDiskWriter(void);
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
static volatile bool isRendering;

static UINT32 sampleRate = 44100;
static UINT32 maxLoops = 2;
static UINT8 playState;

static UINT32 idWavOut;
static UINT32 idWavOutDev;
static UINT32 idWavWrt;

static INT32 AudioOutDrv = 1;
static INT32 WaveWrtDrv = -1;

int main(int argc, char* argv[])
{
	int argbase;
	UINT8 retVal;
	S98Player s98play;
	const S98_HEADER* s98hdr;
	const char* s98Title;
	int curSong;
	
	if (argc < 2)
	{
		printf("Usage: %s inputfile\n", argv[0]);
		return 0;
	}
	argbase = 1;
	
	retVal = InitAudioSystem();
	if (retVal)
		return 1;
	retVal = StartAudioDevice();
	if (retVal)
	{
		DeinitAudioSystem();
		return 1;
	}
	playState = 0x00;
	
	for (curSong = argbase; curSong < argc; curSong ++)
	{
	
	s98play.SetCallback(&FilePlayCallback, NULL);
	
	printf("Loading %s ...  ", GetFileTitle(argv[curSong]));
	fflush(stdout);
	retVal = s98play.LoadFile(argv[curSong]);
	if (retVal)
	{
		printf("Error 0x%02X loading S98 file!\n", retVal);
		continue;
	}
	s98hdr = s98play.GetFileHeader();
	s98Title = s98play.GetSongTitle();
	printf("S98 v%u, Tick Rate: %u/%u\n", s98hdr->fileVer, s98hdr->tickMult, s98hdr->tickDiv);
	if (s98Title != NULL)
		printf("Song Title: %s\n", s98Title);
	
	isRendering = false;
	AudioDrv_SetCallback(audDrv, FillBuffer, &s98play);
	s98play.SetSampleRate(sampleRate);
	s98play.Start();
	
	StartDiskWriter("waveOut.wav");
	canRender = true;
#ifndef _WIN32
	changemode(1);
#endif
	playState &= ~PLAYSTATE_END;
	while(! (playState & PLAYSTATE_END))
	{
		if (! (playState & PLAYSTATE_PAUSE))
		{
			printf("Playing %.2f ...   \r", s98play.GetCurSample() / (double)s98play.GetSampleRate());
			fflush(stdout);
		}
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
			else if (letter == 'R')	// restart
			{
				s98play.Reset();
			}
			else if (letter == 'B')	// previous file
			{
				if (curSong > argbase)
				{
					playState |= PLAYSTATE_END;
					curSong -= 2;
				}
			}
			else if (letter == 'N')	// next file
			{
				if (curSong + 1 < argc)
					playState |= PLAYSTATE_END;
			}
			else if (inkey == 0x1B || letter == 'Q')	// quit
			{
				playState |= PLAYSTATE_END;
				curSong = argc - 1;
			}
		}
	}
#ifndef _WIN32
	changemode(0);
#endif
	canRender = false;
	while(isRendering)
		Sleep(1);	// wait for render thread to finish
	StopDiskWriter();
	
	s98play.Stop();
	s98play.UnloadFile();
	
	}	// end for(curSong)
	
	StopAudioDevice();
	DeinitAudioSystem();
	printf("Done.\n");
	
#if defined(_DEBUG) && (_MSC_VER >= 1400)
	// doesn't work well with C++ containers
	//if (_CrtDumpMemoryLeaks())
	//	_getch();
#endif
	
	return 0;
}

static const char* GetFileTitle(const char* filePath)
{
	const char* dirSep1;
	const char* dirSep2;
	
	dirSep1 = strrchr(filePath, '/');
	dirSep2 = strrchr(filePath, '\\');
	if (dirSep2 > dirSep1)
		dirSep1 = dirSep2;

	return (dirSep1 == NULL) ? filePath : (dirSep1 + 1);
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
	
	isRendering = true;
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
	isRendering = false;
	
	return curSmpl * smplSize;
}

static UINT8 FilePlayCallback(void* userParam, UINT8 evtType, void* evtParam)
{
	switch(evtType)
	{
	case PLREVT_START:
		//printf("S98 playback started.\n");
		break;
	case PLREVT_STOP:
		//printf("S98 playback stopped.\n");
		break;
	case PLREVT_LOOP:
		{
			UINT32* curLoop = (UINT32*)evtParam;
			if (*curLoop >= maxLoops)
			{
				printf("Loop End.\n");
				playState |= PLAYSTATE_END;
				return 0x01;
			}
			printf("Loop %u.\n", 1 + *curLoop);
		}
		break;
	case PLREVT_END:
		playState |= PLAYSTATE_END;
		printf("Song End.\n");
		break;
	}
	return 0x00;
}

// initialize audio system and search for requested audio drivers
static UINT8 InitAudioSystem(void)
{
	UINT32 drvCount;
	UINT32 curDrv;
	INT32 outDrv;
	INT32 wrtDrv;
	AUDDRV_INFO* drvInfo;
	UINT8 retVal;
	
	printf("Opening Audio Device ...\n");
	Audio_Init();
	drvCount = Audio_GetDriverCount();
	if (! drvCount)
	{
		Audio_Deinit();
		return 0xF0;
	}
	
	idWavOut = (UINT32)-1;
	idWavOutDev = 0;
	idWavWrt = (UINT32)-1;
	
	// go through all audio drivers store the IDs of the requested Output and Disk Writer drivers
	outDrv = wrtDrv = 0;
	for (curDrv = 0; curDrv < drvCount; curDrv ++)
	{
		Audio_GetDriverInfo(curDrv, &drvInfo);
		if (drvInfo->drvType == ADRVTYPE_OUT)
		{
			if (outDrv == AudioOutDrv)
				idWavOut = curDrv;
			outDrv ++;
		}
		else if (drvInfo->drvType == ADRVTYPE_DISK)
		{
			if (wrtDrv == WaveWrtDrv)
				idWavWrt = curDrv;
			wrtDrv ++;
		}
	}
	if (idWavOut == (UINT32)-1)
	{
		printf("Requested Audio Output driver not found!\n");
		Audio_Deinit();
		return 0x81;
	}
	
	Audio_GetDriverInfo(idWavOut, &drvInfo);
	printf("Using driver %s.\n", drvInfo->drvName);
	retVal = AudioDrv_Init(idWavOut, &audDrv);
	if (retVal)
	{
		printf("WaveOut: Drv Init Error: %02X\n", retVal);
		Audio_Deinit();
		return retVal;
	}
	
	audDrvLog = NULL;
	if (idWavWrt != (UINT32)-1)
	{
		retVal = AudioDrv_Init(idWavWrt, &audDrvLog);
		if (retVal)
			audDrvLog = NULL;
	}
	
	return 0x00;
}

static UINT8 DeinitAudioSystem(void)
{
	UINT8 retVal;
	
	retVal = AudioDrv_Deinit(&audDrv);
	if (audDrvLog != NULL)
		AudioDrv_Deinit(&audDrvLog);
	Audio_Deinit();
	
	return retVal;
}

static UINT8 StartAudioDevice(void)
{
	AUDIO_OPTS* opts;
	UINT8 retVal;
	
	opts = AudioDrv_GetOptions(audDrv);
	opts->sampleRate = sampleRate;
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	
	canRender = false;
	printf("Opening Device %u ...\n", idWavOutDev);
	retVal = AudioDrv_Start(audDrv, idWavOutDev);
	if (retVal)
	{
		printf("Dev Init Error: %02X\n", retVal);
		return retVal;
	}
	
	smplAlloc = AudioDrv_GetBufferSize(audDrv) / smplSize;
	smplData = (WAVE_32BS*)malloc(smplAlloc * sizeof(WAVE_32BS));
	
	return 0x00;
}

static UINT8 StopAudioDevice(void)
{
	UINT8 retVal;
	
	retVal = AudioDrv_Stop(audDrv);
	free(smplData);	smplData = NULL;
	
	return retVal;
}

static UINT8 StartDiskWriter(const char* fileName)
{
	UINT8 retVal;
	
	if (audDrvLog == NULL)
		return 0x00;
	
	WavWrt_SetFileName(AudioDrv_GetDrvData(audDrvLog), fileName);
	retVal = AudioDrv_Start(audDrvLog, 0);
	if (! retVal)
		AudioDrv_DataForward_Add(audDrv, audDrvLog);
	return retVal;
}

static UINT8 StopDiskWriter(void)
{
	if (audDrvLog == NULL)
		return 0x00;
	
	AudioDrv_DataForward_Remove(audDrv, audDrvLog);
	return AudioDrv_Stop(audDrvLog);
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
