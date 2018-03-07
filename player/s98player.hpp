#ifndef __S98PLAYER_HPP__
#define __S98PLAYER_HPP__

#include <stdtype.h>
#include <emu/EmuStructs.h>
#include <emu/Resampler.h>
#include <iconv.h>
#include "helper.h"
#include <vector>
#include <map>
#include <string>


#define PLAYSTATE_PLAY	0x01	// is playing
#define PLAYSTATE_PAUSE	0x02	// is paused (render wave, but don't advance in the song)
#define PLAYSTATE_END	0x04	// has reached the end of the file

typedef UINT8 (*PLAYER_EVENT_CB)(void* userParam, UINT8 evtType, void* evtParam);
#define PLREVT_NONE		0x00
#define PLREVT_START	0x01	// playback started
#define PLREVT_STOP		0x02	// playback stopped
#define PLREVT_LOOP		0x03	// starting next loop [evtParam: UINT32* loopNumber, ret == 0x01 -> stop processing]
#define PLREVT_END		0x04	// reached the end of the song



struct S98_HEADER
{
	UINT8 fileVer;
	UINT32 tickMult;	// tick timing numerator
	UINT32 tickDiv;		// [v2] tick timing denumerator
	UINT32 compression;	// [v1: 0 - no compression, >0 - size of uncompressed data] [v2: ??] [v3: must be 0]
	UINT32 tagOfs;		// [v1/2: song title file offset] [v3: tag data file offset]
	UINT32 dataOfs;		// play data file offset
	UINT32 loopOfs;		// loop file offset
};
struct S98_DEVICE
{
	UINT32 devType;
	UINT32 clock;
	UINT32 pan;			// [v2: reserved] [v3: pan setting]
	UINT32 app_spec;	// [v2: application-specific] [v3: reserved]
};

typedef struct _s98_chip_device S98_CHIPDEV;
struct _s98_chip_device
{
	VGM_BASEDEV base;
	DEVFUNC_WRITE_A8D8 write;
};

//	--- concept ---
//	- Player class does file rendering at fixed volume (but changeable speed)
//	- host program handles master volume + fading + stopping after X loops (notified via callback)

class S98Player
{
public:
	S98Player();
	~S98Player();
	
	UINT8 LoadFile(const char* fileName);
	UINT8 UnloadFile(void);
	const S98_HEADER* GetFileHeader(void) const;
	const char* GetSongTitle(void);
	
	UINT32 GetSampleRate(void) const;
	UINT8 SetSampleRate(UINT32 sampleRate);
	UINT8 SetPlaybackSpeed(double speed);
	void SetCallback(PLAYER_EVENT_CB cbFunc, void* cbParam);
	UINT32 Tick2Sample(UINT32 ticks) const;
	UINT32 Sample2Tick(UINT32 samples) const;
	
	UINT8 GetState(void) const;
	UINT32 GetCurFileOfs(void) const;
	UINT32 GetCurTick(void) const;
	UINT32 GetCurSample(void) const;
	UINT32 GetTotalTicks(void) const;	// get time for playing once in ticks
	UINT32 GetTotalPlayTicks(UINT32 numLoops) const;	// get time for playing + looping (without fading)
	UINT32 GetCurrentLoop(void) const;
	
	UINT8 Start(void);
	UINT8 Stop(void);
	UINT8 Reset(void);
	UINT32 Render(UINT32 smplCnt, WAVE_32BS* data);
	//UINT8 Seek(...); // TODO
	
private:
	UINT8 LoadTags(void);
	std::string GetUTF8String(const char* startPtr, const char* endPtr);
	UINT8 ParsePSFTags(const std::string& tagData);
	
	void RefreshTSRates(void);
	
	void ParseFile(UINT32 ticks);
	void DoCommand(void);
	
	iconv_t _icSJIS;	// ShiftJIS -> UTF-8 iconv object
	std::vector<UINT8> _fileData;
	
	S98_HEADER _fileHdr;
	std::vector<S98_DEVICE> _devHdrs;
	std::map<std::string, std::string> _tagData;
	std::string _tmpStr;
	
	UINT32 _outSmplRate;
	
	// tick/sample conversion rates
	UINT64 _tsMult;
	UINT64 _tsDiv;
	
	std::vector<S98_CHIPDEV> _devices;
	UINT32 _filePos;
	UINT32 _fileTick;
	UINT32 _playTick;
	UINT32 _playSmpl;
	UINT32 _curLoop;
	
	UINT8 _playState;
	UINT8 _psTrigger;	// used to temporarily trigger special commands
	PLAYER_EVENT_CB _eventCbFunc;
	void* _eventCbParam;
};

#endif	// __S98PLAYER_HPP__
