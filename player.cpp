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
#include "player/gymplayer.hpp"
#include "player/playera.hpp"
#include "audio/AudioStream.h"
#include "audio/AudioStream_SpcDrvFuns.h"
#include "emu/Resampler.h"
#include "emu/SoundDevs.h"	// for DEVID_*
#include "emu/EmuCores.h"
#include "utils/OSMutex.h"

//#define USE_MEMORY_LOADER 1	// define to use the in-memory loader

int main(int argc, char* argv[]);
static void DoChipControlMode(PlayerBase* player);
static void StripNewline(char* str);
static std::string FCC2Str(UINT32 fcc);
static UINT8 *SlurpFile(const char *fileName, UINT32 *fileSize);
static const char* GetFileTitle(const char* filePath);
static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* Data);
static UINT8 FilePlayCallback(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam);
static DATA_LOADER* RequestFileCallback(void* userParam, PlayerBase* player, const char* fileName);
static const char* LogLevel2Str(UINT8 level);
static void PlayerLogCallback(void* userParam, PlayerBase* player, UINT8 level, UINT8 srcType,
	const char* srcTag, const char* message);
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


static void* audDrv;
static void* audDrvLog;
static std::vector<UINT8> locAudBuf;	// local audio buffer (for WAV dumping)
static OS_MUTEX* renderMtx;	// render thread mutex

static UINT32 sampleRate = 44100;
static UINT32 maxLoops = 2;
static bool manualRenderLoop = false;
static volatile UINT8 playState;

static UINT32 idWavOut;
static UINT32 idWavOutDev;
static UINT32 idWavWrt;

static INT32 AudioOutDrv = -2;
static INT32 WaveWrtDrv = -1;

static UINT32 masterVol = 0x10000;	// fixed point 16.16

static UINT8 showTags = 1;
static bool showFileInfo = false;
static UINT8 logLevel = DEVLOG_INFO;

static PlayerA mainPlr;

int main(int argc, char* argv[])
{
	int argbase;
	UINT8 retVal;
	DATA_LOADER *dLoad;
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
	mainPlr.RegisterPlayerEngine(new VGMPlayer);
	mainPlr.RegisterPlayerEngine(new S98Player);
	mainPlr.RegisterPlayerEngine(new DROPlayer);
	mainPlr.RegisterPlayerEngine(new GYMPlayer);
	mainPlr.SetEventCallback(FilePlayCallback, NULL);
	mainPlr.SetFileReqCallback(RequestFileCallback, NULL);
	mainPlr.SetLogCallback(PlayerLogCallback, NULL);
	//mainPlr.SetOutputSettings() is done in StartAudioDevice()
	{
		PlayerA::Config pCfg = mainPlr.GetConfiguration();
		pCfg.masterVol = masterVol;
		pCfg.loopCount = maxLoops;
		pCfg.fadeSmpls = sampleRate * 4;	// fade over 4 seconds
		pCfg.endSilenceSmpls = sampleRate / 2;	// 0.5 seconds of silence at the end
		pCfg.pbSpeed = 1.0;
		mainPlr.SetConfiguration(pCfg);
	}

	for (curSong = argbase; curSong < argc; curSong ++)
	{
	
	printf("Loading %s ...  ", GetFileTitle(argv[curSong]));
	fflush(stdout);

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
		DataLoader_Deinit(dLoad);
		fprintf(stderr, "Error 0x%02X loading file!\n", retVal);
		continue;
	}
	retVal = mainPlr.LoadFile(dLoad);
	if (retVal)
	{
		DataLoader_Deinit(dLoad);
		fprintf(stderr, "Error 0x%02X loading file!\n", retVal);
		continue;
	}
	
	PlayerBase* player = mainPlr.GetPlayer();
	mainPlr.SetLoopCount(maxLoops);
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
		mainPlr.SetLoopCount(vgmplay->GetModifiedLoopCount(maxLoops));
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
	
	if (showTags > 0)
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
		if (showTags >= 2)
		{
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
	}
	
	putchar('\n');
	
	{
		PLR_DEV_OPTS devOpts;
		UINT32 devOptID;
		
		devOptID = PLR_DEV_ID(DEVID_SN76496, 0);
		retVal = player->GetDeviceOptions(devOptID, devOpts);
		if (! (retVal & 0x80))
		{
			static const INT16 panPos[4] = {0x00, -0x80, +0x80, 0x00};
			if (! devOpts.emuCore[0])
				devOpts.emuCore[0] = FCC_MAXM;
			memcpy(devOpts.panOpts.chnPan, panPos, sizeof(panPos));
			player->SetDeviceOptions(devOptID, devOpts);
		}
		
		devOptID = PLR_DEV_ID(DEVID_YM2413, 0);
		retVal = player->GetDeviceOptions(devOptID, devOpts);
		if (! (retVal & 0x80))
		{
			static const INT16 panPos[14] = {
				-0x100, +0x100, -0x80, +0x80, -0x40, +0x40, -0xC0, +0xC0, 0x00,
				-0x60, +0x60, 0x00, -0xC0, +0xC0};
			memcpy(devOpts.panOpts.chnPan, panPos, sizeof(panPos));
			player->SetDeviceOptions(devOptID, devOpts);
		}
		
		devOptID = PLR_DEV_ID(DEVID_AY8910, 0);
		retVal = player->GetDeviceOptions(devOptID, devOpts);
		if (! (retVal & 0x80))
		{
			static const INT16 panPos[3] = {-0x80, +0x80, 0x00};
			memcpy(devOpts.panOpts.chnPan, panPos, sizeof(panPos));
			player->SetDeviceOptions(devOptID, devOpts);
		}

		devOptID = PLR_DEV_ID(DEVID_C6280, 0);
		retVal = player->GetDeviceOptions(devOptID, devOpts);
		if (! (retVal & 0x80))
		{
			if (! devOpts.emuCore[0])
				devOpts.emuCore[0] = FCC_MAME;
			player->SetDeviceOptions(devOptID, devOpts);
		}
	}
	
	mainPlr.Start();
	
	if (showFileInfo)
	{
		// only after calling PlayerA::Start() we can obtain info about the currently used sound cores
		PLR_SONG_INFO sInf;
		std::vector<PLR_DEV_INFO> diList;
		size_t curDev;
		
		player->GetSongInfo(sInf);
		player->GetSongDeviceInfo(diList);
		printf("SongInfo: %s v%X.%02X, Rate %u/%u, Len %u, Loop at %d, devices: %u\n",
			FCC2Str(sInf.format).c_str(), sInf.fileVerMaj, sInf.fileVerMin,
			sInf.tickRateMul, sInf.tickRateDiv, sInf.songLen, sInf.loopTick, sInf.deviceCnt);
		for (curDev = 0; curDev < diList.size(); curDev ++)
		{
			const PLR_DEV_INFO& pdi = diList[curDev];
			printf(" Dev %d: Type 0x%02X #%d, Core %s, Clock %u, Rate %u, Volume 0x%X\n",
				(int)pdi.id, pdi.type, (INT8)pdi.instance, FCC2Str(pdi.core).c_str(), pdi.devCfg->clock, pdi.smplRate, pdi.volume);
		}
	}
	const std::vector<VGMPlayer::DACSTRM_DEV>* vgmPcmStrms = NULL;
	if (player->GetPlayerType() == FCC_VGM)
	{
		VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(player);
		vgmPcmStrms = &vgmplay->GetStreamDevInfo();
	}
	
	StartDiskWriter("waveOut.wav");
	
	if (audDrv != NULL)
		retVal = AudioDrv_SetCallback(audDrv, FillBuffer, &mainPlr);
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
			else if (mainPlr.GetState() & PLAYSTATE_END)
				pState = "Finish ";
			else if (mainPlr.GetState() & PLAYSTATE_FADE)
				pState = "Fading ";
			else
				pState = "Playing";
			if (vgmPcmStrms == NULL || vgmPcmStrms->empty())
			{
				printf("%s %.2f / %.2f ...   \r", pState, mainPlr.GetCurTime(1), mainPlr.GetTotalTime(1));
			}
			else
			{
				const VGMPlayer::DACSTRM_DEV* strmDev = &(*vgmPcmStrms)[0];
				std::string pbMode = " ";
				if (strmDev->pbMode & 0x10)
					pbMode += 'R';	// reverse playback
				if (strmDev->pbMode & 0x80)
					pbMode += 'L';	// looping
				if (pbMode.length() == 1)
					pbMode = "";
				printf("%s %.2f / %.2f [%02X / %02X at %4.1f KHz%s] ...     \r",
					pState, mainPlr.GetCurTime(1), mainPlr.GetTotalTime(1),
					1 + strmDev->lastItem, strmDev->maxItems, strmDev->freq / 1000.0,
					pbMode.c_str());
			}
			fflush(stdout);
			needRefresh = false;
		}
		
		if (manualRenderLoop && ! (playState & PLAYSTATE_PAUSE))
		{
			UINT32 wrtBytes = FillBuffer(audDrvLog, &mainPlr, (UINT32)locAudBuf.size(), &locAudBuf[0]);
			AudioDrv_WriteData(audDrvLog, wrtBytes, &locAudBuf[0]);
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
				mainPlr.Reset();
				OSMutex_Unlock(renderMtx);
			}
			else if (letter >= '0' && letter <= '9')
			{
				UINT32 maxPos;
				UINT8 pbPos10;
				UINT32 destPos;
				
				OSMutex_Lock(renderMtx);
				maxPos = mainPlr.GetPlayer()->GetTotalPlayTicks(maxLoops);
				pbPos10 = letter - '0';
				destPos = maxPos * pbPos10 / 10;
				mainPlr.Seek(PLAYPOS_TICK, destPos);
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
				OSMutex_Lock(renderMtx);
				mainPlr.FadeOut();
				OSMutex_Unlock(renderMtx);
			}
			else if (letter == 'C')	// chip control
			{
#ifndef _WIN32
				changemode(0);	// make sure entered charactered are echoed
#endif
				DoChipControlMode(mainPlr.GetPlayer());
#ifndef _WIN32
				changemode(1);
#endif
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
	
	mainPlr.Stop();
	mainPlr.UnloadFile();
	DataLoader_Deinit(dLoad);	dLoad = NULL;
#ifdef USE_MEMORY_LOADER
	free(fileData);
#endif
	
	}	// end for(curSong)
	
	mainPlr.UnregisterAllPlayers();
	
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
		LL param - set log level (0..5 = off/error/warn/info/debug/trace, see emu/EmuStructs.h)
		Q - quit
	P - player configuration
		SPD param - set playback speed (1.0 = 100%)
		[DRO]
			OPL3 param - DualOPL2 -> OPL3 patch, (0/1/2, see DRO_V2OPL3_*)
		[VGM]
			PHZ param - set playback rate in Hz (set to 50 to play NTSC VGMs in PAL speed)
			HSO param - hard-stop old VGMs (<1.50) when they finish
	[number] - sound chip # configuration (active sound chip)
			Note: The number is the ID of the active sound chip (as shown by device info).
			      All sound chips can be controlled with 0x800I00NN (NN = libvgm device ID, I = instance, i.e. 0 or 1)
		C param - set emulation core to param (four-character code, case sensitive, empty = use default)
		LC param - set emulation core of *linked device* (OPN SSG/OPL4 FM) to param (four-character code, case sensitive, empty = use default)
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
		P param,param,... - set channel panning
			This is a list of stereo positions, one for each channel.
			-1.0 (left) .. 0.0 (centre) .. +1.0 (right)
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
					FCC2Str(devOpts.emuCore[0]).c_str(), devOpts.coreOpts, devOpts.srMode,
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
						double spd = playOpts.genOpts.pbSpeed / (double)0x10000;
						printf("Opts: Speed %.3f, OPL3Mode %u\n", spd, playOpts.v2opl3Mode);
						mode = 2;
					}
						break;
					case FCC_VGM:
					{
						VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(player);
						VGM_PLAY_OPTIONS playOpts;
						vgmplay->GetPlayerOptions(playOpts);
						double spd = playOpts.genOpts.pbSpeed / (double)0x10000;
						printf("Opts: Speed %.3f, PlaybkHz %u, HardStopOld %u\n",
							spd, playOpts.playbackHz, playOpts.hardStopOld);
						mode = 2;
					}
						break;
					case FCC_S98:
					{
						S98Player* s98play = dynamic_cast<S98Player*>(player);
						S98_PLAY_OPTIONS playOpts;
						s98play->GetPlayerOptions(playOpts);
						double spd = playOpts.genOpts.pbSpeed / (double)0x10000;
						printf("Opts: Speed %.3f\n", spd);
						mode = 2;
					}
						break;
					case FCC_GYM:
					{
						GYMPlayer* gymplay = dynamic_cast<GYMPlayer*>(player);
						GYM_PLAY_OPTIONS playOpts;
						gymplay->GetPlayerOptions(playOpts);
						double spd = playOpts.genOpts.pbSpeed / (double)0x10000;
						printf("Opts: Speed %.3f\n", spd);
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
			
			// Core / Linked Core / Opts / SRMode / SampleRate / ReSampleMode / Muting
			printf("Command [C/LC/O/SRM/SR/RSM/M data]: ");
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
				devOpts.emuCore[0] =
					(fccStr[0] << 24) | (fccStr[1] << 16) |
					(fccStr[2] <<  8) | (fccStr[3] <<  0);
				player->SetDeviceOptions((UINT32)chipID, devOpts);
			}
			else if (! strcmp(line, "LC"))
			{
				std::string fccStr(tokenStr);
				fccStr.resize(4, 0x00);
				devOpts.emuCore[1] =
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
			else if (! strcmp(line, "P"))
			{
				PLR_PAN_OPTS& panOpts = devOpts.panOpts;
				UINT32 chnID;
				UINT32 curChn;
				
				letter = '\0';
				tokenStr = strtok(NULL, ",");
				chnID = 0;
				while(tokenStr != NULL && chnID < 32)
				{
					double panPos = strtod(tokenStr, &endPtr);
					if (endPtr > tokenStr)
						panOpts.chnPan[0][chnID] = (INT16)(panPos * 0x100);
					
					tokenStr = strtok(NULL, ",");
					chnID ++;
				}
				
				player->SetDeviceOptions((UINT32)chipID, devOpts);
				printf("-> Panning: ");
				for (curChn = 0; curChn < chnID; curChn ++)
					printf("%.2f,", panOpts.chnPan[0][chnID] / (float)0x100);
				printf("\b \n");
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
				
				if (! strcmp(line, "SPD"))
				{
					double spd = strtod(tokenStr, &endPtr);
					if (endPtr > tokenStr)
						droplay->SetPlaybackSpeed(spd);
				}
				else if (! strcmp(line, "OPL3"))
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
			case FCC_S98:
			{
				S98Player* s98play = dynamic_cast<S98Player*>(player);
				S98_PLAY_OPTIONS playOpts;
				char* tokenStr;
				
				s98play->GetPlayerOptions(playOpts);
				
				printf("Command [SPD data]: ");
				fgets(line, 0x80, stdin);
				StripNewline(line);
				
				tokenStr = strtok(line, " ");
				for (endPtr = line; *endPtr != '\0'; endPtr ++)
					*endPtr = (char)toupper((unsigned char)*endPtr);
				tokenStr = endPtr + 1;
				
				if (! strcmp(line, "SPD"))
				{
					double spd = strtod(tokenStr, &endPtr);
					if (endPtr > tokenStr)
						s98play->SetPlaybackSpeed(spd);
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
				
				printf("Command [SPD/PHZ/HSO data]: ");
				fgets(line, 0x80, stdin);
				StripNewline(line);
				
				tokenStr = strtok(line, " ");
				for (endPtr = line; *endPtr != '\0'; endPtr ++)
					*endPtr = (char)toupper((unsigned char)*endPtr);
				tokenStr = endPtr + 1;
				
				if (! strcmp(line, "SPD"))
				{
					double spd = strtod(tokenStr, &endPtr);
					if (endPtr > tokenStr)
						vgmplay->SetPlaybackSpeed(spd);
				}
				else if (! strcmp(line, "PHZ"))
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
			case FCC_GYM:
			{
				GYMPlayer* gymplay = dynamic_cast<GYMPlayer*>(player);
				GYM_PLAY_OPTIONS playOpts;
				char* tokenStr;
				
				gymplay->GetPlayerOptions(playOpts);
				
				printf("Command [SPD data]: ");
				fgets(line, 0x80, stdin);
				StripNewline(line);
				
				tokenStr = strtok(line, " ");
				for (endPtr = line; *endPtr != '\0'; endPtr ++)
					*endPtr = (char)toupper((unsigned char)*endPtr);
				tokenStr = endPtr + 1;
				
				if (! strcmp(line, "SPD"))
				{
					double spd = strtod(tokenStr, &endPtr);
					if (endPtr > tokenStr)
						gymplay->SetPlaybackSpeed(spd);
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
			printf("Command [T/FI/LL data]: ");
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
						showTags = val;
					else if (! strcmp(line, "FI"))
						showFileInfo = !!val;
				}
			}
			else if (! strcmp(line, "LL"))
			{
				UINT8 newLevel = (UINT8)strtoul(tokenStr, &endPtr, 0);
				if (endPtr > tokenStr)
					logLevel = newLevel;
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
	if(fileData == NULL)
	{
		fclose(hFile);
		return NULL;
	}
	if(fread(fileData,1,hFileSize,hFile) != hFileSize)
	{
		free(fileData);
		fclose(hFile);
		return NULL;
	}
	fclose(hFile);
	*fileSize = hFileSize;
	return fileData;
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
	PlayerA& myPlr = *(PlayerA*)userParam;
	if (! (myPlr.GetState() & PLAYSTATE_PLAY))
	{
		fprintf(stderr, "Player Warning: calling Render while not playing! playState = 0x%02X\n", myPlr.GetState());
		memset(data, 0x00, bufSize);
		return bufSize;
	}
	
	UINT32 renderedBytes;
	OSMutex_Lock(renderMtx);
	renderedBytes = myPlr.Render(bufSize, data);
	OSMutex_Unlock(renderMtx);
	
	return renderedBytes;
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
			if (player->GetState() & PLAYSTATE_SEEK)
				break;
			printf("Loop %u.\n", 1 + *curLoop);
		}
		break;
	case PLREVT_END:
		if (playState & PLAYSTATE_END)
			break;
		playState |= PLAYSTATE_END;
		printf("Song End.\n");
		break;
	}
	return 0x00;
}

static DATA_LOADER* RequestFileCallback(void* userParam, PlayerBase* player, const char* fileName)
{
	DATA_LOADER* dLoad = FileLoader_Init(fileName);
	UINT8 retVal = DataLoader_Load(dLoad);
	if (! retVal)
		return dLoad;
	DataLoader_Deinit(dLoad);
	return NULL;
}

static const char* LogLevel2Str(UINT8 level)
{
	static const char* LVL_NAMES[6] = {" ??? ", "Error", "Warn ", "Info ", "Debug", "Trace"};
	if (level >= (sizeof(LVL_NAMES) / sizeof(LVL_NAMES[0])))
		level = 0;
	return LVL_NAMES[level];
}

static void PlayerLogCallback(void* userParam, PlayerBase* player, UINT8 level, UINT8 srcType,
	const char* srcTag, const char* message)
{
	if (level > logLevel)
		return;	// don't print messages with higher verbosity than current log level
	if (srcType == PLRLOGSRC_PLR)
		printf("[%s] %s: %s", LogLevel2Str(level), player->GetPlayerName(), message);
	else
		printf("[%s] %s %s: %s", LogLevel2Str(level), player->GetPlayerName(), srcTag, message);
	return;
}

static UINT32 GetNthAudioDriver(UINT8 adrvType, INT32 drvNumber)
{
	// special numbers for drvNumber:
	//	-1 - don't select any
	//	-2 - select last found driver
	if (drvNumber == -1)
		return (UINT32)-1;
	
	UINT32 drvCount;
	UINT32 curDrv;
	INT32 typedDrv;
	UINT32 lastDrv;
	AUDDRV_INFO* drvInfo;
	
	// go through all audio drivers get the ID of the requested Output/Disk Writer driver
	drvCount = Audio_GetDriverCount();
	lastDrv = (UINT32)-1;
	for (typedDrv = 0, curDrv = 0; curDrv < drvCount; curDrv ++)
	{
		Audio_GetDriverInfo(curDrv, &drvInfo);
		if (drvInfo->drvType == adrvType)
		{
			lastDrv = curDrv;
			if (typedDrv == drvNumber)
				return curDrv;
			typedDrv ++;
		}
	}
	
	if (drvNumber == -2)
		return lastDrv;
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
	UINT32 smplSize;
	UINT32 smplAlloc;
	UINT32 localAudBufSize;
	
	opts = NULL;
	smplAlloc = 0x00;
	
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
	
	locAudBuf.resize(localAudBufSize);
	mainPlr.SetOutputSettings(opts->sampleRate, opts->numChannels, opts->numBitsPerSmpl, smplAlloc);
	
	return 0x00;
}

static UINT8 StopAudioDevice(void)
{
	UINT8 retVal;
	
	retVal = 0x00;
	if (audDrv != NULL)
		retVal = AudioDrv_Stop(audDrv);
	locAudBuf.clear();
	
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
