#ifndef __PLAYERBASE_HPP__
#define __PLAYERBASE_HPP__

#include <stdtype.h>
#include <emu/Resampler.h>


#define PLAYSTATE_PLAY	0x01	// is playing
#define PLAYSTATE_PAUSE	0x02	// is paused (render wave, but don't advance in the song)
#define PLAYSTATE_END	0x04	// has reached the end of the file

class PlayerBase;
typedef UINT8 (*PLAYER_EVENT_CB)(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam);
#define PLREVT_NONE		0x00
#define PLREVT_START	0x01	// playback started
#define PLREVT_STOP		0x02	// playback stopped
#define PLREVT_LOOP		0x03	// starting next loop [evtParam: UINT32* loopNumber, ret == 0x01 -> stop processing]
#define PLREVT_END		0x04	// reached the end of the song



//	--- concept ---
//	- Player class does file rendering at fixed volume (but changeable speed)
//	- host program handles master volume + fading + stopping after X loops (notified via callback)

class PlayerBase
{
public:
	PlayerBase();
	virtual ~PlayerBase();
	
	virtual UINT32 GetPlayerType(void) const;
	virtual const char* GetPlayerName(void) const;
	virtual UINT8 LoadFile(const char* fileName) = 0;
	virtual UINT8 UnloadFile(void) = 0;
	virtual const char* GetSongTitle(void) = 0;
	
	virtual UINT32 GetSampleRate(void) const;
	virtual UINT8 SetSampleRate(UINT32 sampleRate);
	virtual UINT8 SetPlaybackSpeed(double speed);
	virtual void SetCallback(PLAYER_EVENT_CB cbFunc, void* cbParam);
	virtual UINT32 Tick2Sample(UINT32 ticks) const = 0;
	virtual UINT32 Sample2Tick(UINT32 samples) const = 0;
	virtual double Tick2Second(UINT32 ticks) const = 0;
	virtual double Sample2Second(UINT32 samples) const;
	
	virtual UINT8 GetState(void) const = 0;
	virtual UINT32 GetCurFileOfs(void) const = 0;
	virtual UINT32 GetCurTick(void) const = 0;
	virtual UINT32 GetCurSample(void) const = 0;
	virtual UINT32 GetTotalTicks(void) const = 0;	// get time for playing once in ticks
	virtual UINT32 GetLoopTicks(void) const = 0;	// get time for one loop in ticks
	virtual UINT32 GetTotalPlayTicks(UINT32 numLoops) const;	// get time for playing + looping (without fading)
	virtual UINT32 GetCurrentLoop(void) const = 0;
	
	virtual UINT8 Start(void) = 0;
	virtual UINT8 Stop(void) = 0;
	virtual UINT8 Reset(void) = 0;
	virtual UINT32 Render(UINT32 smplCnt, WAVE_32BS* data) = 0;
	//virtual UINT8 Seek(...) = 0; // TODO
	
protected:
	UINT32 _outSmplRate;
	PLAYER_EVENT_CB _eventCbFunc;
	void* _eventCbParam;
};

#endif	// __PLAYERBASE_HPP__
