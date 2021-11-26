#include <string.h>
#include <vector>

#include "../stdtype.h"
#include "../common_def.h"
#include "../utils/DataLoader.h"
#include "playerbase.hpp"
#include "../emu/Resampler.h"

#include "playera.hpp"

struct int24_s
{
	INT32 data : 24;
};

static void SampleConv_toU8(void* buffer, INT32 value)
{
	value >>= 16;	// 24 bit -> 8 bit
	if (value < -0x80)
		value = -0x80;
	else if (value > +0x7F)
		value = +0x7F;
	*(UINT8*)buffer = (UINT8)(0x80 + value);
	return;
}

static void SampleConv_toS16(void* buffer, INT32 value)
{
	value >>= 8;	// 24 bit -> 16 bit
	if (value < -0x8000)
		value = -0x8000;
	else if (value > +0x7FFF)
		value = +0x7FFF;
	*(INT16*)buffer = (INT16)value;
	return;
}

static void SampleConv_toS24(void* buffer, INT32 value)
{
	if (value < -0x800000)
		value = -0x800000;
	else if (value > +0x7FFFFF)
		value = +0x7FFFFF;
	((int24_s*)buffer)->data = value;
	return;
}

static void SampleConv_toS32(void* buffer, INT32 value)
{
	// internal scale is 24-bit, so limit to that
	if (value < -0x800000)
		value = -0x800000;
	else if (value > +0x7FFFFF)
		value = +0x7FFFFF;
	value <<= 8;	// 24 bit -> 32 bit
	*(INT32*)buffer = value;
	return;
}

static void SampleConv_toF32(void* buffer, INT32 value)
{
	// limiting not required here
	*(float*)buffer = value / (float)0x800000;
	return;
}

static PlayerA::PLR_SMPL_PACK GetSampleConvFunc(UINT8 bits)
{
	if (bits == 8)
		return SampleConv_toU8;
	else if (bits == 16)
		return SampleConv_toS16;
	else if (bits == 24)
		return SampleConv_toS24;
	else if (bits == 32)
		return SampleConv_toS32;
	else
		return NULL;
}

PlayerA::PlayerA()
{
	_config.masterVol = 0x10000;	// fixed point 16.16
	_config.ignoreVolGain = false;
	_config.chnInvert = 0x00;
	_config.loopCount = 2;
	_config.fadeSmpls = 0;
	_config.endSilenceSmpls = 0;
	_config.pbSpeed = 1.0;
	
	_outSmplChns = 2;
	_outSmplBits = 16;
	_outSmplPack = GetSampleConvFunc(_outSmplBits);
	_smplRate = 44100;
	_outSmplSize1 = _outSmplBits / 8;
	_outSmplSizeA = _outSmplSize1 * _outSmplChns;
	
	_plrCbFunc = NULL;
	_plrCbParam = NULL;
	_myPlayState = 0x00;
	_player = NULL;
	_dLoad = NULL;
	_songVolume = CalcSongVolume();
	_fadeSmplStart = (UINT32)-1;
	_endSilenceStart = (UINT32)-1;
	
	return;
}

PlayerA::~PlayerA()
{
	Stop();
	UnloadFile();
	UnregisterAllPlayers();
	return;
}

void PlayerA::RegisterPlayerEngine(PlayerBase* player)
{
	player->SetEventCallback(PlayerA::PlayCallbackS, this);
	//player->SetFileReqCallback(_frCbFunc, _frCbParam);
	player->SetSampleRate(_smplRate);
	player->SetPlaybackSpeed(_config.pbSpeed);
	_avbPlrs.push_back(player);
	return;
}

void PlayerA::UnregisterAllPlayers(void)
{
	for (size_t curPlr = 0; curPlr < _avbPlrs.size(); curPlr ++)
		delete _avbPlrs[curPlr];
	_avbPlrs.clear();
	return;
}

const std::vector<PlayerBase*>& PlayerA::GetRegisteredPlayers(void) const
{
	return _avbPlrs;
}

UINT8 PlayerA::SetOutputSettings(UINT32 smplRate, UINT8 channels, UINT8 smplBits, UINT32 smplBufferLen)
{
	if (channels != 2)
		return 0xF0;	// TODO: support channels = 1
	PLR_SMPL_PACK smplPackFunc = GetSampleConvFunc(smplBits);
	if (smplPackFunc == NULL)
		return 0xF1;	// unsupported sample format
	
	_outSmplChns = channels;
	_outSmplBits = smplBits;
	_outSmplPack = smplPackFunc;
	SetSampleRate(smplRate);
	_outSmplSize1 = _outSmplBits / 8;
	_outSmplSizeA = _outSmplSize1 * _outSmplChns;
	_smplBuf.resize(smplBufferLen);
	return 0x00;
}

UINT32 PlayerA::GetSampleRate(void) const
{
	return _smplRate;
}

void PlayerA::SetSampleRate(UINT32 sampleRate)
{
	_smplRate = sampleRate;
	for (size_t curPlr = 0; curPlr < _avbPlrs.size(); curPlr++)
	{
		if (_avbPlrs[curPlr] == _player && (_player->GetState() & PLAYSTATE_PLAY))
			continue;
		_avbPlrs[curPlr]->SetSampleRate(_smplRate);
	}
	return;
}

double PlayerA::GetPlaybackSpeed(void) const
{
	return _config.pbSpeed;
}

void PlayerA::SetPlaybackSpeed(double speed)
{
	_config.pbSpeed = speed;
	if (_player != NULL)
		_player->SetPlaybackSpeed(_config.pbSpeed);
	return;
}

UINT32 PlayerA::GetLoopCount(void) const
{
	return _config.loopCount;
}

void PlayerA::SetLoopCount(UINT32 loops)
{
	_config.loopCount = loops;
	return;
}

UINT32 PlayerA::GetFadeSamples(void) const
{
	return _config.fadeSmpls;
}

INT32 PlayerA::GetMasterVolume(void) const
{
	return _config.masterVol;
}

void PlayerA::SetMasterVolume(INT32 volume)
{
	_config.masterVol = volume;
	_songVolume = CalcSongVolume();
	return;
}

INT32 PlayerA::GetSongVolume(void) const
{
	return _songVolume;
}

void PlayerA::SetFadeSamples(UINT32 smplCnt)
{
	_config.fadeSmpls = smplCnt;
	return;
}

UINT32 PlayerA::GetEndSilenceSamples(void) const
{
	return _config.endSilenceSmpls;
}

void PlayerA::SetEndSilenceSamples(UINT32 smplCnt)
{
	_config.endSilenceSmpls = smplCnt;
	return;
}

const PlayerA::Config& PlayerA::GetConfiguration(void) const
{
	return _config;
}

void PlayerA::SetConfiguration(const PlayerA::Config& config)
{
	double oldPbSpeed = _config.pbSpeed;
	
	_config = config;
	if (_player != NULL && oldPbSpeed != _config.pbSpeed)
		_player->SetPlaybackSpeed(_config.pbSpeed);
	return;
}

void PlayerA::SetEventCallback(PLAYER_EVENT_CB cbFunc, void* cbParam)
{
	_plrCbFunc = cbFunc;
	_plrCbParam = cbParam;
	return;
}

void PlayerA::SetFileReqCallback(PLAYER_FILEREQ_CB cbFunc, void* cbParam)
{
	for (size_t curPlr = 0; curPlr < _avbPlrs.size(); curPlr ++)
		_avbPlrs[curPlr]->SetFileReqCallback(cbFunc, cbParam);
	return;
}

void PlayerA::SetLogCallback(PLAYER_LOG_CB cbFunc, void* cbParam)
{
	for (size_t curPlr = 0; curPlr < _avbPlrs.size(); curPlr ++)
		_avbPlrs[curPlr]->SetLogCallback(cbFunc, cbParam);
	return;
}

UINT8 PlayerA::GetState(void) const
{
	if (_player == NULL)
		return 0x00;
	UINT8 finalState = _myPlayState;
	if (_fadeSmplStart != (UINT32)-1)
		finalState |= PLAYSTATE_FADE;
	return finalState;
}

UINT32 PlayerA::GetCurPos(UINT8 unit) const
{
	if (_player == NULL)
		return (UINT32)-1;
	return _player->GetCurPos(unit);
}

double PlayerA::GetCurTime(UINT8 includeLoops) const
{
	if (_player == NULL)
		return -1.0;
	// using samples here, as it may be more accurate than the (possibly low-resolution) ticks
	double ticks = _player->Sample2Second(_player->GetCurPos(PLAYPOS_SAMPLE));
	if (! includeLoops)
	{
		UINT32 curLoop = _player->GetCurLoop();
		if (curLoop > 0)
			ticks -= _player->Tick2Second(_player->GetLoopTicks() * curLoop);
	}
	return ticks;
}

double PlayerA::GetTotalTime(UINT8 includeLoops) const
{
	if (_player == NULL)
		return -1.0;
	if (includeLoops)
		return _player->Tick2Second(_player->GetTotalPlayTicks(_config.loopCount));
	else
		return _player->Tick2Second(_player->GetTotalPlayTicks(1));
}

UINT32 PlayerA::GetCurLoop(void) const
{
	if (_player == NULL)
		return (UINT32)-1;
	return _player->GetCurLoop();
}

double PlayerA::GetLoopTime(void) const
{
	if (_player == NULL)
		return -1.0;
	return _player->Tick2Second(_player->GetLoopTicks());
}

PlayerBase* PlayerA::GetPlayer(void)
{
	return _player;
}

const PlayerBase* PlayerA::GetPlayer(void) const
{
	return _player;
}

void PlayerA::FindPlayerEngine(void)
{
	size_t curPlr;
	
	_player = NULL;
	for (curPlr = 0; curPlr < _avbPlrs.size(); curPlr ++)
	{
		UINT8 retVal = _avbPlrs[curPlr]->CanLoadFile(_dLoad);
		if (! retVal)
		{
			_player = _avbPlrs[curPlr];
			return;
		}
	}
	
	return;
}

UINT8 PlayerA::LoadFile(DATA_LOADER* dLoad)
{
	_dLoad = dLoad;
	FindPlayerEngine();
	if (_player == NULL)
		return 0xFF;
	
	UINT8 retVal = _player->LoadFile(dLoad);
	if (retVal >= 0x80)
		return retVal;
	return retVal;
}

UINT8 PlayerA::UnloadFile(void)
{
	if (_player == NULL)
		return 0xFF;
	
	_player->Stop();
	UINT8 retVal = _player->UnloadFile();
	_player = NULL;
	_dLoad = NULL;
	return retVal;
}

UINT32 PlayerA::GetFileSize(void)
{
	if (_dLoad == NULL)
		return 0x00;
	
	UINT32 result = DataLoader_GetTotalSize(_dLoad);
	if (result != (UINT32)-1)
		return result;
	else
		return DataLoader_GetSize(_dLoad);
}

UINT8 PlayerA::Start(void)
{
	if (_player == NULL)
		return 0xFF;
	_player->SetSampleRate(_smplRate);
	_player->SetPlaybackSpeed(_config.pbSpeed);
	_songVolume = CalcSongVolume();
	_fadeSmplStart = (UINT32)-1;
	_endSilenceStart = (UINT32)-1;
	
	UINT8 retVal = _player->Start();
	_myPlayState = _player->GetState() & (PLAYSTATE_PLAY | PLAYSTATE_END);
	return retVal;
}

UINT8 PlayerA::Stop(void)
{
	if (_player == NULL)
		return 0xFF;
	UINT8 retVal = _player->Stop();
	_myPlayState = _player->GetState() & (PLAYSTATE_PLAY | PLAYSTATE_END);
	_myPlayState |= PLAYSTATE_FIN;
	return retVal;
}

UINT8 PlayerA::Reset(void)
{
	if (_player == NULL)
		return 0xFF;
	_fadeSmplStart = (UINT32)-1;
	_endSilenceStart = (UINT32)-1;
	UINT8 retVal = _player->Reset();
	_myPlayState = _player->GetState() & (PLAYSTATE_PLAY | PLAYSTATE_END);
	return retVal;
}

UINT8 PlayerA::FadeOut(void)
{
	if (_player == NULL)
		return 0xFF;
	if (_fadeSmplStart == (UINT32)-1)
		_fadeSmplStart = _player->GetCurPos(PLAYPOS_SAMPLE);
	return 0x00;
}

UINT8 PlayerA::Seek(UINT8 unit, UINT32 pos)
{
	if (_player == NULL)
		return 0xFF;
	UINT8 retVal = _player->Seek(unit, pos);
	_myPlayState = _player->GetState() & (PLAYSTATE_PLAY | PLAYSTATE_END);
	
	UINT32 pbSmpl = _player->GetCurPos(PLAYPOS_SAMPLE);
	if (pbSmpl < _fadeSmplStart)
		_fadeSmplStart = (UINT32)-1;
	if (pbSmpl < _endSilenceStart)
		_endSilenceStart = (UINT32)-1;
	return retVal;
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

// 16.16 fixed point multiplication
#define MUL16X16_FIXED(a, b)	(INT32)(((INT64)a * b) >> 16)

INT32 PlayerA::CalcSongVolume(void)
{
	INT32 volume = _config.masterVol;
	
	if (! _config.ignoreVolGain && _player != NULL)
	{
		PLR_SONG_INFO songInfo;
		UINT8 retVal = _player->GetSongInfo(songInfo);
		if (! retVal)
			volume = MUL16X16_FIXED(volume, songInfo.volGain);
	}
	
	return volume;
}

INT32 PlayerA::CalcCurrentVolume(UINT32 playbackSmpl)
{
	INT32 curVol;	// 16.16 fixed point
	
	// 1. master volume
	curVol = _songVolume;
	
	// 2. apply fade-out factor
	if (playbackSmpl >= _fadeSmplStart)
	{
		UINT32 fadeSmpls;
		UINT64 fadeVol;	// 64 bit for less type casts when doing multiplications with .16 fixed point
		
		fadeSmpls = playbackSmpl - _fadeSmplStart;
		if (fadeSmpls >= _config.fadeSmpls)
			return 0x0000;	// going beyond fade time -> volume 0
		
		fadeVol = (UINT64)fadeSmpls * 0x10000 / _config.fadeSmpls;
		fadeVol = 0x10000 - fadeVol;	// fade from full volume to silence
		fadeVol = fadeVol * fadeVol;	// logarithmic fading sounds nicer
		curVol = (INT32)(((INT64)fadeVol * curVol) >> 32);
	}
	
	return curVol;
}

UINT32 PlayerA::Render(UINT32 bufSize, void* data)
{
	UINT8* bData = (UINT8*)data;
	UINT32 basePbSmpl;
	UINT32 smplCount;
	UINT32 smplRendered;
	UINT32 curSmpl;
	WAVE_32BS fnlSmpl;	// final sample value
	INT32 curVolume;
	
	smplCount = bufSize / _outSmplSizeA;
	if (_player == NULL)
	{
		memset(data, 0x00, smplCount * _outSmplSizeA);
		return smplCount * _outSmplSizeA;
	}
	if (! (_player->GetState() & PLAYSTATE_PLAY))
	{
		//fprintf(stderr, "Player Warning: calling Render while not playing! playState = 0x%02X\n", _player->GetState());
		memset(data, 0x00, smplCount * _outSmplSizeA);
		return smplCount * _outSmplSizeA;
	}
	
	if (! smplCount)
	{
		_player->Render(0, NULL);	// dummy-rendering
		return 0;
	}
	
	if (smplCount > (UINT32)_smplBuf.size())
		smplCount = (UINT32)_smplBuf.size();
	memset(&_smplBuf[0], 0, smplCount * sizeof(WAVE_32BS));
	basePbSmpl = _player->GetCurPos(PLAYPOS_SAMPLE);
	smplRendered = _player->Render(smplCount, &_smplBuf[0]);
	smplCount = smplRendered;
	
	curVolume = CalcCurrentVolume(basePbSmpl) >> VOL_SHIFT;
	for (curSmpl = 0; curSmpl < smplCount; curSmpl ++, basePbSmpl ++)
	{
		if (basePbSmpl >= _fadeSmplStart)
		{
			UINT32 fadeSmpls = basePbSmpl - _fadeSmplStart;
			if (fadeSmpls >= _config.fadeSmpls && ! (_myPlayState & PLAYSTATE_END))
			{
				if (_endSilenceStart == (UINT32)-1)
					_endSilenceStart = basePbSmpl;
				_myPlayState |= PLAYSTATE_END;
			}
			
			curVolume = CalcCurrentVolume(basePbSmpl) >> VOL_SHIFT;
		}
		if (basePbSmpl >= _endSilenceStart)
		{
			UINT32 silenceSmpls = basePbSmpl - _endSilenceStart;
			if (silenceSmpls >= _config.endSilenceSmpls && ! (_myPlayState & PLAYSTATE_FIN))
			{
				_myPlayState |= PLAYSTATE_FIN;
				if (_plrCbFunc != NULL)
					_plrCbFunc(_player, _plrCbParam, PLREVT_END, NULL);
				// NOTE: We are effectively discarding rendered samples here!
				// We can get away with that for now, as the application is supposed to
				// stop playback at this point, but we shouldn't really do this.
				break;
			}
		}
		
		// Input is about 24 bits (some cores might output a bit more)
		fnlSmpl = _smplBuf[curSmpl];
		
#ifdef VOLCALC64
		fnlSmpl.L = (INT32)( ((INT64)fnlSmpl.L * curVolume) >> VOL_BITS );
		fnlSmpl.R = (INT32)( ((INT64)fnlSmpl.R * curVolume) >> VOL_BITS );
#else
		fnlSmpl.L = ((fnlSmpl.L >> VOL_PRESH) * curVolume) >> VOL_POSTSH;
		fnlSmpl.R = ((fnlSmpl.R >> VOL_PRESH) * curVolume) >> VOL_POSTSH;
#endif
		
		if (_config.chnInvert & 0x01)
			fnlSmpl.L = -fnlSmpl.L;
		if (_config.chnInvert & 0x02)
			fnlSmpl.R = -fnlSmpl.R;
		
		_outSmplPack(&bData[(curSmpl * 2 + 0) * _outSmplSize1], fnlSmpl.L);
		_outSmplPack(&bData[(curSmpl * 2 + 1) * _outSmplSize1], fnlSmpl.R);
	}
	
	return curSmpl * _outSmplSizeA;
}

/*static*/ UINT8 PlayerA::PlayCallbackS(PlayerBase* player, void* userParam, UINT8 evtType, void* evtParam)
{
	PlayerA* plr = (PlayerA*)userParam;
	return plr->PlayCallback(player, evtType, evtParam);
}

UINT8 PlayerA::PlayCallback(PlayerBase* player, UINT8 evtType, void* evtParam)
{
	UINT8 retVal = 0x00;
	
	if (evtType != PLREVT_END)	// We will generate our own PLREVT_END event depending on fading/endSilence.
	{
		if (_plrCbFunc != NULL)
			retVal = _plrCbFunc(player, _plrCbParam, evtType, evtParam);
		if (retVal)
			return retVal;
	}
	
	switch(evtType)
	{
	case PLREVT_START:
	case PLREVT_STOP:
		break;
	case PLREVT_LOOP:
		{
			UINT32* curLoop = (UINT32*)evtParam;
			if (_config.loopCount > 0 && *curLoop >= _config.loopCount)
			{
				//if (_config.fadeSmpls == 0)
				//	return 0x01;	// send "stop" signal to player engine
				FadeOut();
			}
		}
		break;
	case PLREVT_END:
		_myPlayState |= PLAYSTATE_END;
		_endSilenceStart = player->GetCurPos(PLAYPOS_SAMPLE);
		break;
	}
	return 0x00;
}
