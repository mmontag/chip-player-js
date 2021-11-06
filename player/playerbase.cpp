#include "playerbase.hpp"

#include <stdlib.h>
#include <string.h>	// for memset()

PlayerBase::PlayerBase() :
	_outSmplRate(0),
	_eventCbFunc(NULL),
	_eventCbParam(NULL),
	_fileReqCbFunc(NULL),
	_fileReqCbParam(NULL),
	_logCbFunc(NULL),
	_logCbParam(NULL)
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

/*static*/ UINT8 PlayerBase::PlayerCanLoadFile(DATA_LOADER *dataLoader)
{
	return 0xFF;
}

UINT8 PlayerBase::CanLoadFile(DATA_LOADER *dataLoader) const
{
	return this->PlayerCanLoadFile(dataLoader);
}

/*static*/ UINT8 PlayerBase::InitDeviceOptions(PLR_DEV_OPTS& devOpts)
{
	devOpts.emuCore[0] = 0x00;
	devOpts.emuCore[1] = 0x00;
	devOpts.srMode = DEVRI_SRMODE_NATIVE;
	devOpts.resmplMode = 0x00;
	devOpts.smplRate = 0;
	devOpts.coreOpts = 0x00;
	devOpts.muteOpts.disable = 0x00;
	devOpts.muteOpts.chnMute[0] = 0x00;
	devOpts.muteOpts.chnMute[1] = 0x00;
	memset(devOpts.panOpts.chnPan, 0x00, sizeof(devOpts.panOpts.chnPan));
	return 0x00;
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

void PlayerBase::SetEventCallback(PLAYER_EVENT_CB cbFunc, void* cbParam)
{
	_eventCbFunc = cbFunc;
	_eventCbParam = cbParam;
	
	return;
}

void PlayerBase::SetFileReqCallback(PLAYER_FILEREQ_CB cbFunc, void* cbParam)
{
	_fileReqCbFunc = cbFunc;
	_fileReqCbParam = cbParam;
	
	return;
}

void PlayerBase::SetLogCallback(PLAYER_LOG_CB cbFunc, void* cbParam)
{
	_logCbFunc = cbFunc;
	_logCbParam = cbParam;
	
	return;
}

double PlayerBase::Sample2Second(UINT32 samples) const
{
	if (samples == (UINT32)-1)
		return -1.0;
	return samples / (double)_outSmplRate;
}

UINT32 PlayerBase::GetTotalPlayTicks(UINT32 numLoops) const
{
	if (numLoops == 0 && GetLoopTicks() > 0)
		return (UINT32)-1;
	return GetTotalTicks() + GetLoopTicks() * (numLoops - 1);
}
