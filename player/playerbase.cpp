#include "playerbase.hpp"

PlayerBase::PlayerBase() :
	_outSmplRate(0),
	_eventCbFunc(NULL),
	_eventCbParam(NULL)
{
}

PlayerBase::~PlayerBase()
{
}

UINT32 PlayerBase::GetPlayerType(void) const
{
	return 0x00000000;
}

const char* PlayerBase::GetPlayerName(void) const
{
	return "";
}

/*static*/ UINT8 IsMyFile(DATA_LOADER *dataLoader)
{
	return 0xFF;
}

UINT32 PlayerBase::GetSampleRate(void) const
{
	return _outSmplRate;
}

UINT8 PlayerBase::SetSampleRate(UINT32 sampleRate)
{
	_outSmplRate = sampleRate;
	return 0x00;
}

UINT8 PlayerBase::SetPlaybackSpeed(double speed)
{
	return 0xFF;	// not yet supported
}

void PlayerBase::SetCallback(PLAYER_EVENT_CB cbFunc, void* cbParam)
{
	_eventCbFunc = cbFunc;
	_eventCbParam = cbParam;
	
	return;
}

double PlayerBase::Sample2Second(UINT32 samples) const
{
	return samples / (double)_outSmplRate;
}

UINT32 PlayerBase::GetTotalPlayTicks(UINT32 numLoops) const
{
	return GetTotalTicks() + GetLoopTicks() * (numLoops - 1);
}
