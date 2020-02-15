// DRO/S98/VGM player test program
// -------------------------------
// Warning: This just serves as a tool for testing features.
// Thus, the user interface can be a complicated mess and a pain to use.

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
extern "C" int __cdecl _kbhit(void);
#else
#include <unistd.h>		// for STDIN_FILENO and usleep()
#include <termios.h>
#include <sys/time.h>	// for struct timeval in _kbhit()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

#include "common_def.h"
#include "utils/DataLoader.h"
#include "utils/FileLoader.h"
#include "utils/MemoryLoader.h"
#include "player/playerbase.hpp"
#include "player/s98player.hpp"
#include "player/droplayer.hpp"
#include "player/vgmplayer.hpp"
#include "audio/AudioStream.h"
#include "audio/AudioStream_SpcDrvFuns.h"
#include "emu/Resampler.h"
#include "utils/OSMutex.h"

//#define USE_MEMORY_LOADER 1	// define to use the in-memory loader

int main(int argc, char* argv[]);
static void DoChipControlMode(PlayerBase* player);
static void StripNewline(char* str);
static std::string FCC2Str(UINT32 fcc);
static UINT8 *SlurpFile(const char *fileName, UINT32 *fileSize);
static UINT8 GetPlayerForFile(DATA_LOADER *dLoad, PlayerBase** retPlayer);
static const char* GetFileTitle(const char* filePath);
static UINT32 CalcCurrentVolume(UINT32 playbackSmpl);
static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* Data);
static UINT8 FilePlayCallback(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam);
static UINT32 GetNthAudioDriver(UINT8 adrvType, INT32 drvNumber);
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
static UINT32 localAudBufSize;
static void* localAudBuffer;
static OS_MUTEX* renderMtx;	// render thread mutex

static UINT32 sampleRate = 44100;
static UINT32 maxLoops = 2;
static bool manualRenderLoop = false;
static volatile UINT8 playState;

static UINT32 idWavOut;
static UINT32 idWavOutDev;
static UINT32 idWavWrt;

static INT32 AudioOutDrv = 0;
static INT32 WaveWrtDrv = -1;

static UINT32 masterVol = 0x10000;	// fixed point 16.16
static UINT32 fadeSmplStart;
static UINT32 fadeSmplTime;

static bool showTags = false;
static bool showFileInfo = false;

static DROPlayer* droPlr;
static S98Player* s98Plr;
static VGMPlayer* vgmPlr;

int main(int argc, char* argv[])
{
	int argbase;
	UINT8 retVal;
	DATA_LOADER *dLoad;
	PlayerBase* player;
	int curSong;
	bool needRefresh;
	
	if (argc < 2)
	{
		printf("Usage: %s inputfile\n", argv[0]);
		return 0;
	}
	argbase = 1;
#ifdef _WIN32
	SetConsoleOutputCP(65001);	// set UTF-8 codepage
#endif
	
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
	
	// I'll keep the instances of the players for the program's life time.
	// This way player/chip options are kept between track changes.
	droPlr = new DROPlayer;
	s98Plr = new S98Player;
	vgmPlr = new VGMPlayer;
	
	for (curSong = argbase; curSong < argc; curSong ++)
	{
	
	printf("Loading %s ...  ", GetFileTitle(argv[curSong]));
	fflush(stdout);
	player = NULL;

#ifdef USE_MEMORY_LOADER
	UINT32 fileSize;
	UINT8 *fileData = SlurpFile(argv[curSong],&fileSize);
	dLoad = MemoryLoader_Init(fileData, fileSize);
#else
	dLoad = FileLoader_Init(argv[curSong]);
#endif

	if(dLoad == NULL) continue;
	DataLoader_SetPreloadBytes(dLoad,0x100);
	retVal = DataLoader_Load(dLoad);
	if (retVal)
	{
		DataLoader_CancelLoading(dLoad);
		fprintf(stderr, "Error 0x%02X loading file!\n", retVal);
		continue;
	}
	retVal = GetPlayerForFile(dLoad, &player);
	if (retVal)
	{
		DataLoader_CancelLoading(dLoad);
		player = NULL;
		fprintf(stderr, "Error 0x%02X loading file!\n", retVal);
		continue;
	}
	player->SetCallback(&FilePlayCallback, NULL);
	
	if (player->GetPlayerType() == FCC_S98)
	{
		S98Player* s98play = dynamic_cast<S98Player*>(player);
		const S98_HEADER* s98hdr = s98play->GetFileHeader();
		
		printf("S98 v%u, Total Length: %.2f s, Loop Length: %.2f s, Tick Rate: %u/%u", s98hdr->fileVer,
				player->Tick2Second(player->GetTotalTicks()), player->Tick2Second(player->GetLoopTicks()),
				s98hdr->tickMult, s98hdr->tickDiv);
	}
	else if (player->GetPlayerType() == FCC_VGM)
	{
		VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(player);
		const VGM_HEADER* vgmhdr = vgmplay->GetFileHeader();
		
		printf("VGM v%3X, Total Length: %.2f s, Loop Length: %.2f s", vgmhdr->fileVer,
				player->Tick2Second(player->GetTotalTicks()), player->Tick2Second(player->GetLoopTicks()));
	}
	else if (player->GetPlayerType() == FCC_DRO)
	{
		DROPlayer* droplay = dynamic_cast<DROPlayer*>(player);
		const DRO_HEADER* drohdr = droplay->GetFileHeader();
		const char* hwType;
		
		if (drohdr->hwType == 0)
			hwType = "OPL2";
		else if (drohdr->hwType == 1)
			hwType = "DualOPL2";
		else if (drohdr->hwType == 2)
			hwType = "OPL3";
		else
			hwType = "unknown";
		printf("DRO v%u, Total Length: %.2f s, HW Type: %s", drohdr->verMajor,
				player->Tick2Second(player->GetTotalTicks()), hwType);
	}
	
	if (showTags)
	{
		const char* songTitle = NULL;
		const char* songAuthor = NULL;
		const char* songGame = NULL;
		const char* songSystem = NULL;
		const char* songDate = NULL;
		const char* songComment = NULL;
	
		const char* const* tagList = player->GetTags();
		for (const char* const* t = tagList; *t; t += 2)
		{
			if (!strcmp(t[0], "TITLE"))
				songTitle = t[1];
			else if (!strcmp(t[0], "ARTIST"))
				songAuthor = t[1];
			else if (!strcmp(t[0], "GAME"))
				songGame = t[1];
			else if (!strcmp(t[0], "SYSTEM"))
				songSystem = t[1];
			else if (!strcmp(t[0], "DATE"))
				songDate = t[1];
			else if (!strcmp(t[0], "COMMENT"))
				songComment = t[1];
		}
	
		if (songTitle != NULL && songTitle[0] != '\0')
			printf("\nSong Title: %s", songTitle);
		if (songAuthor != NULL && songAuthor[0] != '\0')
			printf("\nSong Author: %s", songAuthor);
		if (songGame != NULL && songGame[0] != '\0')
			printf("\nSong Game: %s", songGame);
		if (songSystem != NULL && songSystem[0] != '\0')
			printf("\nSong System: %s", songSystem);
		if (songDate != NULL && songDate[0] != '\0')
			printf("\nSong Date: %s", songDate);
		if (songComment != NULL && songComment[0] != '\0')
			printf("\nSong Comment: %s", songComment);
	}
	
	putchar('\n');
	
	player->SetSampleRate(sampleRate);
	player->Start();
	fadeSmplTime = player->GetSampleRate() * 4;
	fadeSmplStart = (UINT32)-1;
	
	if (showFileInfo)
	{
		PLR_SONG_INFO sInf;
		std::vector<PLR_DEV_INFO> diList;
		size_t curDev;
		
		player->GetSongInfo(sInf);
		player->GetSongDeviceInfo(diList);
		printf("SongInfo: %s v%X.%X, Rate %u/%u, Len %u, Loop at %d, devices: %u\n",
			FCC2Str(sInf.format).c_str(), sInf.fileVerMaj, sInf.fileVerMin,
			sInf.tickRateMul, sInf.tickRateDiv, sInf.songLen, sInf.loopTick, sInf.deviceCnt);
		for (curDev = 0; curDev < diList.size(); curDev ++)
		{
			const PLR_DEV_INFO& pdi = diList[curDev];
			printf(" Dev %d: Type 0x%02X #%d, Core %s, Clock %u, Rate %u, Volume 0x%X\n",
				(int)pdi.id, pdi.type, (INT8)pdi.instance, FCC2Str(pdi.core).c_str(), pdi.clock, pdi.smplRate, pdi.volume);
			if (pdi.cParams != 0)
				printf("        CfgParams: 0x%08X\n", pdi.cParams);
		}
	}
	
	StartDiskWriter("waveOut.wav");
	
	if (audDrv != NULL)
		retVal = AudioDrv_SetCallback(audDrv, FillBuffer, &player);
	else
		retVal = 0xFF;
	manualRenderLoop = (retVal != 0x00);
#ifndef _WIN32
	changemode(1);
#endif
	playState &= ~PLAYSTATE_END;
	needRefresh = true;
	while(! (playState & PLAYSTATE_END))
	{
		if (! (playState & PLAYSTATE_PAUSE))
			needRefresh = true;	// always update when playing
		if (needRefresh)
		{
			const char* pState;
			
			if (playState & PLAYSTATE_PAUSE)
				pState = "Paused";
			else if (fadeSmplStart != (UINT32)-1)
				pState = "Fading";
			else
				pState = "Playing";
			printf("%s %.2f / %.2f ...   \r", pState, player->Sample2Second(player->GetCurPos(PLAYPOS_SAMPLE)),
					player->Tick2Second(player->GetTotalPlayTicks(maxLoops)));
			fflush(stdout);
			needRefresh = false;
		}
		
		if (manualRenderLoop && ! (playState & PLAYSTATE_PAUSE))
		{
			UINT32 wrtBytes = FillBuffer(audDrvLog, &player, localAudBufSize, localAudBuffer);
			AudioDrv_WriteData(audDrvLog, wrtBytes, localAudBuffer);
		}
		else
		{
			Sleep(50);
		}
		
		if (_kbhit())
		{
			int inkey = _getch();
			int letter = toupper(inkey);
			
			if (letter == ' ' || letter == 'P')
			{
				playState ^= PLAYSTATE_PAUSE;
				if (audDrv != NULL)
				{
					if (playState & PLAYSTATE_PAUSE)
						AudioDrv_Pause(audDrv);
					else
						AudioDrv_Resume(audDrv);
				}
			}
			else if (letter == 'R')	// restart
			{
				OSMutex_Lock(renderMtx);
				player->Reset();
				fadeSmplStart = (UINT32)-1;
				OSMutex_Unlock(renderMtx);
			}
			else if (letter >= '0' && letter <= '9')
			{
				UINT32 maxPos;
				UINT8 pbPos10;
				UINT32 destPos;
				
				OSMutex_Lock(renderMtx);
				maxPos = player->GetTotalPlayTicks(maxLoops);
				pbPos10 = letter - '0';
				destPos = maxPos * pbPos10 / 10;
				player->Seek(PLAYPOS_TICK, destPos);
				if (player->GetCurPos(PLAYPOS_SAMPLE) < fadeSmplStart)
					fadeSmplStart = (UINT32)-1;
				OSMutex_Unlock(renderMtx);
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
			else if (letter == 'F')	// fade out
			{
				fadeSmplStart = player->GetCurPos(PLAYPOS_SAMPLE);
			}
			else if (letter == 'C')	// chip control
			{
				DoChipControlMode(player);
			}
			needRefresh = true;
		}
	}
#ifndef _WIN32
	changemode(0);
#endif
	// remove callback to prevent further rendering
	// also waits for render thread to finish its work
	if (audDrv != NULL)
		AudioDrv_SetCallback(audDrv, NULL, NULL);
	
	StopDiskWriter();
	
	player->Stop();
	player->UnloadFile();
	DataLoader_Deinit(dLoad);
	player = NULL; dLoad = NULL;
#ifdef USE_MEMORY_LOADER
	free(fileData);
#endif
	
	}	// end for(curSong)
	
	delete droPlr;
	delete s98Plr;
	delete vgmPlr;
	
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

/*
Command Mode overview

Sound Chip ID:
	D - display configuration
		T param - show tags (0/D/OFF - off, 1/E/ON - on)
		FI param - show file information (see above)
		Q - quit
	P - player configuration
		[DRO]
			OPL3 param - DualOPL2 -> OPL3 patch, (0/1/2, see DRO_V2OPL3_*)
		[VGM]
			PHZ param - set playback rate in Hz (set to 50 to play NTSC VGMs in PAL speed)
			HSO param - hard-stop old VGMs (<1.50) when they finish
	[number] - sound chip # configuration (active sound chip)
			Note: The number is the ID of the active sound chip (as shown by device info).
			      All sound chips can be controlled with 0x800I00NN (NN = libvgm device ID, I = instance, i.e. 0 or 1)
		C param - set emulation core to param (four-character code, case sensitive, empty = use default)
		O param - set sound core options (core-specific)
		SRM param - set sample rate mode (0/1/2, see DEVRI_SRMODE_*)
		SR param - set emulated sample rate (0 = use rate of output stream)
		RSM param - set resampling mode [not working]
		M param,param,... - set mute options
			This is a list of channels to be toggled. (0 = first channel)
			Additional valid letters:
				E - enable sound chip
				D - disable sound chip
				O - all channels on
				X - mute all channels
		Q - quit
*/
static void DoChipControlMode(PlayerBase* player)
{
	int letter;
	int mode;
	char line[0x80];
	char* endPtr;
	int chipID;
	UINT8 retVal;
	
	printf("Command Mode. ");
	mode = 0;	// start
	chipID = -1;
	while(mode >= 0)
	{
		if (mode == 0)
		{
			PLR_DEV_OPTS devOpts;
			
			// number (sound chip ID) / D (display) / P (player options)
			printf("Sound Chip ID: ");
			fgets(line, 0x80, stdin);
			StripNewline(line);
			if (line[0] == '\0')
				return;
			
			// Note: In MSVC 2010, strtol returns 0x7FFFFFFF for the string "0x8000000C".
			// strtoul returns the correct value and also properly returns -1 for "-1".
			chipID = (int)strtoul(line, &endPtr, 0);
			if (endPtr > line)
			{
				retVal = player->GetDeviceOptions((UINT32)chipID, devOpts);
				if (retVal & 0x80)
				{
					printf("Invalid sound chip ID.\n");
					continue;
				}
				
				printf("Cfg: Core %s, Opts 0x%X, srMode 0x%02X, sRate %u, resampleMode 0x%02X\n",
					FCC2Str(devOpts.emuCore).c_str(), devOpts.coreOpts, devOpts.srMode,
					devOpts.smplRate, devOpts.resmplMode);
				printf("Muting: Chip %s [0x%02X], Channel Mask: 0x%02X\n",
					(devOpts.muteOpts.disable & 0x01) ? "Off" : "On", devOpts.muteOpts.disable,
					devOpts.muteOpts.chnMute[0]);
				mode = 1;
			}
			else
			{
				letter = toupper((unsigned char)*line);
				if (letter == 'D')
				{
					mode = 10;
				}
				else if (letter == 'P')
				{
					switch(player->GetPlayerType())
					{
					case FCC_DRO:
					{
						DROPlayer* droplay = dynamic_cast<DROPlayer*>(player);
						DRO_PLAY_OPTIONS playOpts;
						droplay->GetPlayerOptions(playOpts);
						printf("Opts: OPL3Mode %u\n", playOpts.v2opl3Mode);
						mode = 2;
					}
						break;
					case FCC_VGM:
					{
						VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(player);
						VGM_PLAY_OPTIONS playOpts;
						vgmplay->GetPlayerOptions(playOpts);
						printf("Opts: PlaybkHz %u, HardStopOld %u\n",
							playOpts.playbackHz, playOpts.hardStopOld);
						mode = 2;
					}
						break;
					}
				}
			}
		}
		else if (mode == 1)	// sound chip command mode
		{
			char* tokenStr;
			PLR_DEV_OPTS devOpts;
			
			retVal = player->GetDeviceOptions((UINT32)chipID, devOpts);
			if (retVal & 0x80)
			{
				mode = 0;
				continue;
			}
			
			// Core / Opts / SRMode / SampleRate / ReSampleMode / Muting
			printf("Command [C/O/SRM/SR/RSM/M data]: ");
			fgets(line, 0x80, stdin);
			StripNewline(line);
			
			tokenStr = strtok(line, " ");
			for (endPtr = line; *endPtr != '\0'; endPtr ++)
				*endPtr = (char)toupper((unsigned char)*endPtr);
			tokenStr = endPtr + 1;
			
			if (! strcmp(line, "C"))
			{
				std::string fccStr(tokenStr);
				fccStr.resize(4, 0x00);
				devOpts.emuCore =
					(fccStr[0] << 24) | (fccStr[1] << 16) |
					(fccStr[2] <<  8) | (fccStr[3] <<  0);
				player->SetDeviceOptions((UINT32)chipID, devOpts);
			}
			else if (! strcmp(line, "O"))
			{
				devOpts.coreOpts = (UINT32)strtoul(tokenStr, &endPtr, 0);
				if (endPtr > tokenStr)
					player->SetDeviceOptions((UINT32)chipID, devOpts);
			}
			else if (! strcmp(line, "SRM"))
			{
				devOpts.srMode = (UINT8)strtoul(tokenStr, &endPtr, 0);
				if (endPtr > tokenStr)
					player->SetDeviceOptions((UINT32)chipID, devOpts);
			}
			else if (! strcmp(line, "SR"))
			{
				devOpts.smplRate = (UINT32)strtoul(tokenStr, &endPtr, 0);
				if (endPtr > tokenStr)
					player->SetDeviceOptions((UINT32)chipID, devOpts);
			}
			else if (! strcmp(line, "RSM"))
			{
				devOpts.resmplMode = (UINT8)strtoul(tokenStr, &endPtr, 0);
				if (endPtr > tokenStr)
					player->SetDeviceOptions((UINT32)chipID, devOpts);
			}
			else if (! strcmp(line, "M"))
			{
				PLR_MUTE_OPTS& muteOpts = devOpts.muteOpts;
				
				letter = '\0';
				tokenStr = strtok(NULL, ",");
				while(tokenStr != NULL)
				{
					letter = toupper((unsigned char)*tokenStr);
					if (letter == 'E')
						muteOpts.disable = 0x00;
					else if (letter == 'D')
						muteOpts.disable = 0xFF;
					else if (letter == 'O')
						muteOpts.chnMute[0] = 0;
					else if (letter == 'X')
						muteOpts.chnMute[0] = ~0;
					else if (isalnum(letter))
					{
						long chnID = strtol(tokenStr, &endPtr, 0);
						if (endPtr > tokenStr)
							muteOpts.chnMute[0] ^= (1 << chnID);
					}
					
					tokenStr = strtok(NULL, ",");
				}
				
				player->SetDeviceMuting((UINT32)chipID, muteOpts);
				printf("-> Chip %s [0x%02X], Channel Mask: 0x%02X\n",
					(muteOpts.disable & 0x01) ? "Off" : "On", muteOpts.disable, muteOpts.chnMute[0]);
			}
			else if (! strcmp(line, "Q"))
				mode = -1;
			else
				mode = 0;
		}
		else if (mode == 2)	// player configuration mode
		{
			switch(player->GetPlayerType())
			{
			case FCC_DRO:
			{
				DROPlayer* droplay = dynamic_cast<DROPlayer*>(player);
				DRO_PLAY_OPTIONS playOpts;
				char* tokenStr;
				
				droplay->GetPlayerOptions(playOpts);
				
				printf("Command [OPL3 data]: ");
				fgets(line, 0x80, stdin);
				StripNewline(line);
			
				tokenStr = strtok(line, " ");
				for (endPtr = line; *endPtr != '\0'; endPtr ++)
					*endPtr = (char)toupper((unsigned char)*endPtr);
				tokenStr = endPtr + 1;
			
				if (! strcmp(line, "OPL3"))
				{
					playOpts.v2opl3Mode = (UINT8)strtoul(tokenStr, &endPtr, 0);
					if (endPtr > tokenStr)
						droplay->SetPlayerOptions(playOpts);
				}
				else if (! strcmp(line, "Q"))
					mode = -1;
				else
					mode = 0;
			}
				break;
			case FCC_VGM:
			{
				VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(player);
				VGM_PLAY_OPTIONS playOpts;
				char* tokenStr;
				
				vgmplay->GetPlayerOptions(playOpts);
				
				printf("Command [PHZ/HSO data]: ");
				fgets(line, 0x80, stdin);
				StripNewline(line);
			
				tokenStr = strtok(line, " ");
				for (endPtr = line; *endPtr != '\0'; endPtr ++)
					*endPtr = (char)toupper((unsigned char)*endPtr);
				tokenStr = endPtr + 1;
			
				if (! strcmp(line, "PHZ"))
				{
					playOpts.playbackHz = (UINT32)strtoul(tokenStr, &endPtr, 0);
					if (endPtr > tokenStr)
						vgmplay->SetPlayerOptions(playOpts);
				}
				else if (! strcmp(line, "HSO"))
				{
					playOpts.hardStopOld = (UINT8)strtoul(tokenStr, &endPtr, 0);
					if (endPtr > tokenStr)
						vgmplay->SetPlayerOptions(playOpts);
				}
				else if (! strcmp(line, "Q"))
					mode = -1;
				else
					mode = 0;
			}
				break;
			default:
				printf("No player-specific configuration available.\n");
				mode = 0;
				break;
			}
		}
		else if (mode == 10)	// display configuration mode
		{
			char* tokenStr;
			
			// Tags / FileInfo
			printf("Command [T/FI data]: ");
			fgets(line, 0x80, stdin);
			StripNewline(line);
			
			tokenStr = strtok(line, " ");
			for (endPtr = line; *endPtr != '\0'; endPtr ++)
				*endPtr = (char)toupper((unsigned char)*endPtr);
			tokenStr = endPtr + 1;
			
			if (! strcmp(line, "T") || ! strcmp(line, "FI"))
			{
				int val = (int)strtol(tokenStr, &endPtr, 0);
				if (endPtr == tokenStr)
				{
					for (endPtr = tokenStr; *endPtr != '\0'; endPtr ++)
						*endPtr = (char)toupper((unsigned char)*endPtr);
					letter = *tokenStr;
					if (letter == 'E' || ! strcmp(tokenStr, "ON"))
						val = 1;
					else if (letter == 'D' || ! strcmp(tokenStr, "OFF"))
						val = 0;
					else
						val = -1;
				}
				if (val >= 0)
				{
					if (! strcmp(line, "T"))
						showTags = !!val;
					else if (! strcmp(line, "FI"))
						showFileInfo = !!val;
				}
			}
			else if (! strcmp(line, "Q"))
				mode = -1;
			else
				mode = 0;
		}
	}
	
	return;
}

static void StripNewline(char* str)
{
	char* strPtr;
	
	strPtr = str;
	while(*strPtr != '\0')
		strPtr ++;
	
	while(strPtr > str && iscntrl((unsigned char)strPtr[-1]))
		strPtr --;
	*strPtr = '\0';
	
	return;
}

static std::string FCC2Str(UINT32 fcc)
{
	std::string result(4, '\0');
	result[0] = (char)((fcc >> 24) & 0xFF);
	result[1] = (char)((fcc >> 16) & 0xFF);
	result[2] = (char)((fcc >>  8) & 0xFF);
	result[3] = (char)((fcc >>  0) & 0xFF);
	return result;
}

static UINT8 *SlurpFile(const char *fileName, UINT32 *fileSize)
{
	*fileSize = 0;
	FILE *hFile = fopen(fileName,"rb");
	UINT32 hFileSize;
	UINT8 *fileData;
	if(hFile == NULL) return NULL;
	fseek(hFile,0,SEEK_END);
	hFileSize = ftell(hFile);
	rewind(hFile);
	fileData = (UINT8 *)malloc(hFileSize);
	if(fileData == NULL) return NULL;
	if(fread(fileData,1,hFileSize,hFile) != hFileSize)
	{
		free(fileData);
		return NULL;
	}
	*fileSize = hFileSize;
	return fileData;
}

static UINT8 GetPlayerForFile(DATA_LOADER *dLoad, PlayerBase** retPlayer)
{
	UINT8 retVal;
	PlayerBase* player;
	
	if (! VGMPlayer::IsMyFile(dLoad))
		player = vgmPlr;
	else if (! S98Player::IsMyFile(dLoad))
		player = s98Plr;
	else if (! DROPlayer::IsMyFile(dLoad))
		player = droPlr;
	else
		return 0xFF;
	
	retVal = player->LoadFile(dLoad);
	if (retVal < 0x80)
		*retPlayer = player;
	return retVal;
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

#if 1
#define VOLCALC64
#define VOL_BITS	16	// use .X fixed point for working volume
#else
#define VOL_BITS	8	// use .X fixed point for working volume
#endif
#define VOL_SHIFT	(16 - VOL_BITS)	// shift for master volume -> working volume

// Pre- and post-shifts are used to make the calculations as accurate as possible
// without causing the sample data (likely 24 bits) to overflow while applying the volume gain.
// Smaller values for VOL_PRESH are more accurate, but have a higher risk of overflows during calculations.
// (24 + VOL_POSTSH) must NOT be larger than 31
#define VOL_PRESH	4	// sample data pre-shift
#define VOL_POSTSH	(VOL_BITS - VOL_PRESH)	// post-shift after volume multiplication

static UINT32 CalcCurrentVolume(UINT32 playbackSmpl)
{
	UINT32 curVol;	// 16.16 fixed point
	
	// 1. master volume
	curVol = masterVol;
	
	// 2. apply fade-out factor
	if (playbackSmpl >= fadeSmplStart)
	{
		UINT32 fadeSmpls;
		UINT64 fadeVol;	// 64 bit for less type casts when doing multiplications with .16 fixed point
		
		fadeSmpls = playbackSmpl - fadeSmplStart;
		if (fadeSmpls >= fadeSmplTime)
			return 0x0000;	// going beyond fade time -> volume 0
		
		fadeVol = (UINT64)fadeSmpls * 0x10000 / fadeSmplTime;
		fadeVol = 0x10000 - fadeVol;	// fade from full volume to silence
		fadeVol = fadeVol * fadeVol;	// logarithmic fading sounds nicer
		curVol = (UINT32)((fadeVol * curVol) >> 32);
	}
	
	return curVol;
}

static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* data)
{
	PlayerBase* player;
	UINT32 basePbSmpl;
	UINT32 smplCount;
	UINT32 smplRendered;
	INT16* SmplPtr16;
	UINT32 curSmpl;
	WAVE_32BS fnlSmpl;	// final sample value
	INT32 curVolume;
	
	smplCount = bufSize / smplSize;
	if (! smplCount)
		return 0;
	
	player = *(PlayerBase**)userParam;
	if (player == NULL)
	{
		memset(data, 0x00, smplCount * smplSize);
		return smplCount * smplSize;
	}
	if (! (player->GetState() & PLAYSTATE_PLAY))
	{
		fprintf(stderr, "Player Warning: calling Render while not playing! playState = 0x%02X\n", player->GetState());
		memset(data, 0x00, smplCount * smplSize);
		return smplCount * smplSize;
	}
	
	OSMutex_Lock(renderMtx);
	if (smplCount > smplAlloc)
		smplCount = smplAlloc;
	memset(smplData, 0, smplCount * sizeof(WAVE_32BS));
	basePbSmpl = player->GetCurPos(PLAYPOS_SAMPLE);
	smplRendered = player->Render(smplCount, smplData);
	smplCount = smplRendered;
	
	curVolume = (INT32)CalcCurrentVolume(basePbSmpl) >> VOL_SHIFT;
	SmplPtr16 = (INT16*)data;
	for (curSmpl = 0; curSmpl < smplCount; curSmpl ++, basePbSmpl ++, SmplPtr16 += 2)
	{
		if (basePbSmpl >= fadeSmplStart)
		{
			UINT32 fadeSmpls;
			
			fadeSmpls = basePbSmpl - fadeSmplStart;
			if (fadeSmpls >= fadeSmplTime && ! (playState & PLAYSTATE_END))
			{
				playState |= PLAYSTATE_END;
				break;
			}
			
			curVolume = (INT32)CalcCurrentVolume(basePbSmpl) >> VOL_SHIFT;
		}
		
		// Input is about 24 bits (some cores might output a bit more)
		fnlSmpl = smplData[curSmpl];
		
#ifdef VOLCALC64
		fnlSmpl.L = (INT32)( ((INT64)fnlSmpl.L * curVolume) >> VOL_BITS );
		fnlSmpl.R = (INT32)( ((INT64)fnlSmpl.R * curVolume) >> VOL_BITS );
#else
		fnlSmpl.L = ((fnlSmpl.L >> VOL_PRESH) * curVolume) >> VOL_POSTSH;
		fnlSmpl.R = ((fnlSmpl.R >> VOL_PRESH) * curVolume) >> VOL_POSTSH;
#endif
		
		fnlSmpl.L >>= 8;	// 24 bit -> 16 bit
		fnlSmpl.R >>= 8;
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
	OSMutex_Unlock(renderMtx);
	
	return curSmpl * smplSize;
}

static UINT8 FilePlayCallback(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam)
{
	switch(evtType)
	{
	case PLREVT_START:
		//printf("Playback started.\n");
		break;
	case PLREVT_STOP:
		//printf("Playback stopped.\n");
		break;
	case PLREVT_LOOP:
		{
			UINT32* curLoop = (UINT32*)evtParam;
			if (*curLoop >= maxLoops)
			{
				if (fadeSmplTime)
				{
					if (fadeSmplStart == (UINT32)-1)
						fadeSmplStart = player->GetCurPos(PLAYPOS_SAMPLE);
				}
				else
				{
					printf("Loop End.\n");
					playState |= PLAYSTATE_END;
					return 0x01;
				}
			}
			if (player->GetState() & PLAYSTATE_SEEK)
				break;
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

static UINT32 GetNthAudioDriver(UINT8 adrvType, INT32 drvNumber)
{
	if (drvNumber == -1)
		return (UINT32)-1;
	
	UINT32 drvCount;
	UINT32 curDrv;
	INT32 typedDrv;
	AUDDRV_INFO* drvInfo;
	
	// go through all audio drivers get the ID of the requested Output/Disk Writer driver
	drvCount = Audio_GetDriverCount();
	for (typedDrv = 0, curDrv = 0; curDrv < drvCount; curDrv ++)
	{
		Audio_GetDriverInfo(curDrv, &drvInfo);
		if (drvInfo->drvType == adrvType)
		{
			if (typedDrv == drvNumber)
				return curDrv;
			typedDrv ++;
		}
	}
	
	return (UINT32)-1;
}

// initialize audio system and search for requested audio drivers
static UINT8 InitAudioSystem(void)
{
	AUDDRV_INFO* drvInfo;
	UINT8 retVal;
	
	retVal = OSMutex_Init(&renderMtx, 0);
	
	printf("Opening Audio Device ...\n");
	retVal = Audio_Init();
	if (retVal == AERR_NODRVS)
		return retVal;
	
	idWavOut = GetNthAudioDriver(ADRVTYPE_OUT, AudioOutDrv);
	idWavOutDev = 0;	// default device
	if (AudioOutDrv != -1 && idWavOut == (UINT32)-1)
	{
		fprintf(stderr, "Requested Audio Output driver not found!\n");
		Audio_Deinit();
		return AERR_NODRVS;
	}
	idWavWrt = GetNthAudioDriver(ADRVTYPE_DISK, WaveWrtDrv);
	
	audDrv = NULL;
	if (idWavOut != (UINT32)-1)
	{
		Audio_GetDriverInfo(idWavOut, &drvInfo);
		printf("Using driver %s.\n", drvInfo->drvName);
		retVal = AudioDrv_Init(idWavOut, &audDrv);
		if (retVal)
		{
			fprintf(stderr, "WaveOut: Driver Init Error: %02X\n", retVal);
			Audio_Deinit();
			return retVal;
		}
#ifdef AUDDRV_DSOUND
		if (drvInfo->drvSig == ADRVSIG_DSOUND)
			DSound_SetHWnd(AudioDrv_GetDrvData(audDrv), GetDesktopWindow());
#endif
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
	
	retVal = 0x00;
	if (audDrv != NULL)
	{
		retVal = AudioDrv_Deinit(&audDrv);	audDrv = NULL;
	}
	if (audDrvLog != NULL)
	{
		AudioDrv_Deinit(&audDrvLog);	audDrvLog = NULL;
	}
	Audio_Deinit();
	
	OSMutex_Deinit(renderMtx);	renderMtx = NULL;
	
	return retVal;
}

static UINT8 StartAudioDevice(void)
{
	AUDIO_OPTS* opts;
	UINT8 retVal;
	
	opts = NULL;
	smplAlloc = 0x00;
	smplData = NULL;
	
	if (audDrv != NULL)
		opts = AudioDrv_GetOptions(audDrv);
	else if (audDrvLog != NULL)
		opts = AudioDrv_GetOptions(audDrvLog);
	if (opts == NULL)
		return 0xFF;
	opts->sampleRate = sampleRate;
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	
	if (audDrv != NULL)
	{
		printf("Opening Device %u ...\n", idWavOutDev);
		retVal = AudioDrv_Start(audDrv, idWavOutDev);
		if (retVal)
		{
			fprintf(stderr, "Device Init Error: %02X\n", retVal);
			return retVal;
		}
		
		smplAlloc = AudioDrv_GetBufferSize(audDrv) / smplSize;
		localAudBufSize = 0;
	}
	else
	{
		smplAlloc = opts->sampleRate / 4;
		localAudBufSize = smplAlloc * smplSize;
	}
	
	smplData = (WAVE_32BS*)malloc(smplAlloc * sizeof(WAVE_32BS));
	localAudBuffer = localAudBufSize ? malloc(localAudBufSize) : NULL;
	
	return 0x00;
}

static UINT8 StopAudioDevice(void)
{
	UINT8 retVal;
	
	retVal = 0x00;
	if (audDrv != NULL)
		retVal = AudioDrv_Stop(audDrv);
	free(smplData);	smplData = NULL;
	free(localAudBuffer);	localAudBuffer = NULL;
	
	return retVal;
}

static UINT8 StartDiskWriter(const char* fileName)
{
	AUDIO_OPTS* opts;
	UINT8 retVal;
	
	if (audDrvLog == NULL)
		return 0x00;
	
	opts = AudioDrv_GetOptions(audDrvLog);
	if (audDrv != NULL)
		*opts = *AudioDrv_GetOptions(audDrv);
	
	WavWrt_SetFileName(AudioDrv_GetDrvData(audDrvLog), fileName);
	retVal = AudioDrv_Start(audDrvLog, 0);
	if (! retVal && audDrv != NULL)
		AudioDrv_DataForward_Add(audDrv, audDrvLog);
	return retVal;
}

static UINT8 StopDiskWriter(void)
{
	if (audDrvLog == NULL)
		return 0x00;
	
	if (audDrv != NULL)
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
