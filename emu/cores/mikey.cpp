// license:MIT
// copyright-holders:laoo

#include <cstdint>
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <cassert>
#include <algorithm>
#include <limits>

extern "C"
{

#include "mikey.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

static void mikey_write( void*, uint8_t address, uint8_t value );
static uint8_t mikey_read( void*, uint8_t address );
static void mikey_set_mute_mask( void*, UINT32 mutes );
static UINT8 mikey_start( const DEV_GEN_CFG*, DEV_INFO* );
static void mikey_stop( void* );
static void mikey_reset( void* );
static void mikey_update( void*, UINT32 samples, DEV_SMPL** outputs );

static DEVDEF_RWFUNC devFunc[] =
{
  {RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void*)mikey_write},
  {RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, (void*)mikey_read},
  {RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, (void*)mikey_set_mute_mask},
  {0x00, 0x00, 0, NULL}
};

static DEV_DEF devDef =
{
  "MIKEY", "laoo", FCC_LAOO,

  mikey_start,
  mikey_stop,
  mikey_reset,
  mikey_update,

  NULL,	// SetOptionBits
  mikey_set_mute_mask,
  NULL,	// SetPanning
  NULL,	// SetSampleRateChangeCallback
  NULL,	// SetLoggingCallback
  NULL,	// LinkDevice

  devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_Mikey[] =
{
  &devDef,
  NULL
};

}

namespace Lynx
{

class MikeyPimpl;
class ActionQueue;

struct Mikey
{
public:

  DEV_DATA devData;

  Mikey( uint32_t sampleRate );
  ~Mikey();

  void write( uint8_t address, uint8_t value );
  uint8_t read( uint8_t address );
  void sampleAudio( int32_t* bufL, int32_t* bufR, size_t size );
  void mute( int channel, bool mute );
  void reset();


private:
  void enqueueSampling();

private:

  std::unique_ptr<MikeyPimpl> mMikey;
  std::unique_ptr<ActionQueue> mQueue;
  uint64_t mTick;
  uint64_t mNextTick;
  uint32_t mSampleRate;
  uint32_t mSamplesRemainder;
  std::pair<uint32_t, uint32_t> mTicksPerSample;
};

}


#if defined( _MSC_VER )

#include <intrin.h>

static uint32_t popcnt_generic( uint32_t x )
{
  int v = 0;
  while ( x != 0 )
  {
    x &= x - 1;
    v++;
  }
  return v;
}

#if defined( _M_IX86 ) || defined( _M_X64 )

static uint32_t popcnt_intrinsic( uint32_t x )
{
  return __popcnt( x );
}

static uint32_t( *popcnt )( uint32_t );

//detecting popcnt availability on msvc intel
static void selectPOPCNT()
{
  int info[4];
  __cpuid( info, 1 );
  if ( ( info[2] & ( (int)1 << 23 ) ) != 0 )
  {
    popcnt = &popcnt_intrinsic;
  }
  else
  {
    popcnt = &popcnt_generic;
  }
}

#else //defined( _M_IX86 ) || defined( _M_X64 )

//MSVC non INTEL should use generic implementation
inline void selectPOPCNT()
{
}

#define popcnt popcnt_generic

#endif

#else //defined( _MSC_VER )

//non MVSC should use builtin implementation

inline void selectPOPCNT()
{
}

#define popcnt __builtin_popcount

#endif

namespace Lynx
{

namespace
{

static const int64_t CNT_MAX = std::numeric_limits<int64_t>::max() & ~15;

int32_t clamp( int32_t v, int32_t lo, int32_t hi )
{
  return v < lo ? lo : ( v > hi ? hi : v );
}

class Timer
{
public:
  Timer() : mValueUpdateTick(), mAudShift(), mEnableReload(), mEnableCount(), mTimerDone(), mBackup( 0 ), mValue( 0 )
  {
  }

  int64_t setBackup( int64_t tick, uint8_t backup )
  {
    mBackup = backup;
    return computeAction( tick );
  }

  int64_t setControlA( int64_t tick, uint8_t controlA )
  {
    mTimerDone ^= ( controlA & CONTROLA::RESET_DONE ) != 0;
    mEnableReload = ( controlA & CONTROLA::ENABLE_RELOAD ) != 0;
    mEnableCount = ( controlA & CONTROLA::ENABLE_COUNT ) != 0;
    mAudShift = controlA & CONTROLA::AUD_CLOCK_MASK;

    return computeAction( tick );
  }

  int64_t setCount( int64_t tick, uint8_t value )
  {
    return computeTriggerTime( tick );
  }

  void setControlB( uint8_t controlB )
  {
    mTimerDone = ( controlB & CONTROLB::TIMER_DONE ) != 0;
  }

  int64_t fireAction( int64_t tick )
  {
    mTimerDone = true;

    return computeAction( tick );
  }

  uint8_t getBackup() const
  {
    return mBackup;
  }

  uint8_t getCount( int64_t tick )
  {
    updateValue( tick );
    return mValue;
  }

private:

  int64_t scaleDiff( int64_t older, int64_t newer ) const
  {
    int64_t const mask = (int64_t)( ~0ull << ( mAudShift + 4 ) );
    return ( ( newer & mask ) - ( older & mask ) ) >> ( mAudShift + 4 );
  }

  void updateValue( int64_t tick )
  {
    if ( mEnableCount )
      mValue = (uint8_t)std::max( (int64_t)0, mValue - scaleDiff( mValueUpdateTick, tick ) );
    mValueUpdateTick = tick;
  }

  int64_t computeTriggerTime( int64_t tick )
  {
    if ( mEnableCount && mValue != 0 )
    {
      //tick value is increased by multipy of 16 (1 MHz resolution) lower bits are unchanged
      return tick + ( 1ull + mValue ) * ( 1ull << ( mAudShift + 4 ) );
    }
    else
    {
      return CNT_MAX;  //infinite
    }
  }

  int64_t computeAction( int64_t tick )
  {
    updateValue( tick );
    if ( mValue == 0 && mEnableReload )
    {
      mValue = mBackup;
    }

    return computeTriggerTime( tick );
  }

private:
  struct CONTROLA
  {
    static const uint8_t RESET_DONE = 0b01000000;
    static const uint8_t ENABLE_RELOAD = 0b00010000;
    static const uint8_t ENABLE_COUNT = 0b00001000;
    static const uint8_t AUD_CLOCK_MASK = 0b00000111;
  };
  struct CONTROLB
  {
    static const uint8_t TIMER_DONE = 0b00001000;
  };

private:
  int64_t mValueUpdateTick;
  int mAudShift;
  bool mEnableReload;
  bool mEnableCount;
  bool mTimerDone;
  uint8_t mBackup;
  uint8_t mValue;
};

class AudioChannel
{
public:
  AudioChannel( uint32_t number ) : mTimer(), mNumber( number ), mShiftRegister(), mTapSelector(), mEnableIntegrate(), mVolume(), mOutput(), mCtrlA()
  {
  }

  int64_t fireAction( int64_t tick )
  {
    trigger();
    return adjust( mTimer.fireAction( tick ) );
  }

  void setVolume( int8_t value )
  {
    mVolume = value;
  }

  void setFeedback( uint8_t value )
  {
    mTapSelector = ( mTapSelector & 0b0011'1100'0000 ) | ( value & 0b0011'1111 ) | ( ( (int)value & 0b1100'0000 ) << 4 );
  }

  void setOutput( uint8_t value )
  {
    mOutput = value;
  }

  void setShift( uint8_t value )
  {
    mShiftRegister = ( mShiftRegister & 0xff00 ) | value;
  }

  int64_t setBackup( int64_t tick, uint8_t value )
  {
    return adjust( mTimer.setBackup( tick, value ) );
  }

  int64_t setControl( int64_t tick, uint8_t value )
  {
    if ( mCtrlA == value )
      return 0;
    mCtrlA = value;

    mTapSelector = ( mTapSelector & 0b1111'0111'1111 ) | ( value & FEEDBACK_7 );
    mEnableIntegrate = ( value & ENABLE_INTEGRATE ) != 0;
    return adjust( mTimer.setControlA( tick, value & ~( FEEDBACK_7 | ENABLE_INTEGRATE ) ) );
  }

  int64_t setCounter( int64_t tick, uint8_t value )
  {
    return adjust( mTimer.setCount( tick, value ) );
  }

  void setOther( uint8_t value )
  {
    mShiftRegister = ( mShiftRegister & 0b0000'1111'1111 ) | ( ( (int)value & 0b1111'0000 ) << 4 );
    mTimer.setControlB( value & 0b0000'1111 );
  }

  int8_t getOutput() const
  {
    return mOutput;
  }

  uint8_t readRegister( int64_t tick, int reg )
  {
    switch ( reg )
    {
    case 0:
      return mVolume;
    case 1:
      return  mTapSelector & 0xff;
    case 2:
      return  mOutput;
    case 3:
      return  mShiftRegister & 0xff;
    case 4:
      return  mTimer.getBackup();
    case 5:
      return  mCtrlA;
    case 6:
      return  mTimer.getCount( tick );
    case 7:
      return  ( ( mShiftRegister >> 4 ) & 0xf0 );
    default:
      return 0xff;
    }
  }

private:

  int64_t adjust( int64_t tick ) const
  {
    //ticks are advancing in 1 MHz resolution, so lower 4 bits are unused.
    //timer number is encoded on lowest 2 bits.
    return tick | mNumber;
  }

  void trigger()
  {
    uint32_t xorGate = mTapSelector & mShiftRegister;
    uint32_t parity = popcnt( xorGate ) & 1;
    uint32_t newShift = ( mShiftRegister << 1 ) | ( parity ^ 1 );
    mShiftRegister = newShift;

    if ( mEnableIntegrate )
    {
      int32_t temp = mOutput + ( ( newShift & 1 ) ? mVolume : -mVolume );
      mOutput = (int8_t)clamp( temp, (int32_t)std::numeric_limits<int8_t>::min(), (int32_t)std::numeric_limits<int8_t>::max() );
    }
    else
    {
      mOutput = ( newShift & 1 ) ? mVolume : -mVolume;
    }
  }

private:
  static const uint8_t FEEDBACK_7 = 0b10000000;
  static const uint8_t ENABLE_INTEGRATE = 0b00100000;

private:
  Timer mTimer;
  uint32_t mNumber;

  uint32_t mShiftRegister;
  uint32_t mTapSelector;
  bool mEnableIntegrate;
  int8_t mVolume;
  int8_t mOutput;
  uint8_t mCtrlA;
};

}


/*
  "Queue" holding event timepoints.
  - 4 channel timer fire points
  - 1 sample point
  Time is in 16 MHz units but only with 1 MHz resolution.
  Four LSBs are used to encode event kind 0-3 are channels, 4 is sampling.
*/
class ActionQueue
{
public:


  ActionQueue() : mTab{ CNT_MAX | 0, CNT_MAX | 1, CNT_MAX | 2, CNT_MAX | 3, CNT_MAX | 4 }
  {
  }

  void push( int64_t value )
  {
    size_t idx = value & 15;
    if ( idx < mTab.size() )
    {
      if ( value & ~15 )
      {
        //writing only non-zero values
        mTab[idx] = value;
      }
    }
  }

  int64_t pop()
  {
    int64_t min1 = std::min( mTab[0], mTab[1] );
    int64_t min2 = std::min( mTab[2], mTab[3] );
    int64_t min3 = std::min( min1, mTab[4] );
    int64_t min4 = std::min( min2, min3 );

    assert( ( min4 & 15 ) < (int64_t)mTab.size() );
    mTab[min4 & 15] = CNT_MAX | ( min4 & 15 );

    return min4;
  }

private:
  std::array<int64_t, 5> mTab;
};


class MikeyPimpl
{
public:

  struct AudioSample
  {
    AudioSample( int16_t l, int16_t r ) : left( l ), right( r ) {}

    int16_t left;
    int16_t right;
  };

  static const uint16_t VOLCNTRL = 0x0;
  static const uint16_t FEEDBACK = 0x1;
  static const uint16_t OUTPUT = 0x2;
  static const uint16_t SHIFT = 0x3;
  static const uint16_t BACKUP = 0x4;
  static const uint16_t CONTROL = 0x5;
  static const uint16_t COUNTER = 0x6;
  static const uint16_t OTHER = 0x7;

  static const uint16_t ATTENREG0 = 0x40;
  static const uint16_t ATTENREG1 = 0x41;
  static const uint16_t ATTENREG2 = 0x42;
  static const uint16_t ATTENREG3 = 0x43;
  static const uint16_t MPAN = 0x44;
  static const uint16_t MSTEREO = 0x50;

  MikeyPimpl() : mAudioChannels{ AudioChannel( 0 ), AudioChannel( 1 ), AudioChannel( 2 ), AudioChannel( 3 ) },
    mAttenuationLeft(),
    mAttenuationRight(), mMute(),
    mRegisterPool(), mPan( 0xff ), mStereo()
  {
    std::fill_n( mRegisterPool.data(), mRegisterPool.size(), (uint8_t)0xff );
    std::fill_n( mAttenuationLeft.data(), mAttenuationLeft.size(), 0x3c );
    std::fill_n( mAttenuationRight.data(), mAttenuationRight.size(), 0x3c );
    std::fill_n( mMute.data(), mMute.size(), false );
  }

  ~MikeyPimpl() {}

  int64_t write( int64_t tick, uint8_t address, uint8_t value )
  {
    assert( address >= 0x20 );

    if ( address < 0x40 )
    {
      size_t idx = ( address >> 3 ) & 3;
      switch ( address & 0x7 )
      {
      case VOLCNTRL:
        mAudioChannels[idx].setVolume( (int8_t)value );
        break;
      case FEEDBACK:
        mAudioChannels[idx].setFeedback( value );
        break;
      case OUTPUT:
        mAudioChannels[idx].setOutput( value );
        break;
      case SHIFT:
        mAudioChannels[idx].setShift( value );
        break;
      case BACKUP:
        return mAudioChannels[idx].setBackup( tick, value );
      case CONTROL:
        return mAudioChannels[idx].setControl( tick, value );
      case COUNTER:
        return mAudioChannels[idx].setCounter( tick, value );
      case OTHER:
        mAudioChannels[idx].setOther( value );
        break;
      }
    }
    else
    {
      int idx = address & 3;
      switch ( address )
      {
      case ATTENREG0:
      case ATTENREG1:
      case ATTENREG2:
      case ATTENREG3:
        mRegisterPool[8*4+idx] = value;
        mAttenuationRight[idx] = ( value & 0x0f ) << 2;
        mAttenuationLeft[idx] = ( value & 0xf0 ) >> 2;
        break;
      case MPAN:
        mPan = value;
        break;
      case MSTEREO:
        mStereo = value;
        break;
      default:
        break;
      }
    }
    return 0;
  }

  int64_t fireTimer( int64_t tick )
  {
    size_t timer = tick & 0x0f;
    assert( timer < 4 );
    return mAudioChannels[timer].fireAction( tick );
  }

  AudioSample sampleAudio() const
  {
    int left = 0;
    int right = 0;

    for ( size_t i = 0; i < 4; ++i )
    {
      if ( mMute[i] )
        continue;

      if ( ( mStereo & ( (uint8_t)0x01 << i ) ) == 0 )
      {
        const int attenuation = ( mPan & ( (uint8_t)0x01 << i ) ) != 0 ? mAttenuationLeft[i] : 0x3c;
        left += mAudioChannels[i].getOutput() * attenuation;
      }

      if ( ( mStereo & ( (uint8_t)0x10 << i ) ) == 0 )
      {
        const int attenuation = ( mPan & ( (uint8_t)0x01 << i ) ) != 0 ? mAttenuationRight[i] : 0x3c;
        right += mAudioChannels[i].getOutput() * attenuation;
      }
    }

    return AudioSample( (int16_t)left, (int16_t)right );
  }

  uint8_t read( int64_t tick, int address )
  {
    size_t i = address >> 3;
    if ( i < 4 )
    {
      return mAudioChannels[i].readRegister( tick, address & 7 );
    }
    else if ( (size_t)address < mRegisterPool.size() )
      return mRegisterPool[address];
    else
      return 0xff;
  }

  void mute( int channel, bool mute )
  {
    mMute[channel] = mute;
  }

private:

  std::array<AudioChannel, 4> mAudioChannels;
  std::array<int, 4> mAttenuationLeft;
  std::array<int, 4> mAttenuationRight;
  std::array<bool, 4> mMute;
  std::array<uint8_t, 4 * 8 + 4> mRegisterPool;

  uint8_t mPan;
  uint8_t mStereo;
};


Mikey::Mikey( uint32_t sampleRate ) : mMikey( std::make_unique<MikeyPimpl>() ), mQueue( std::make_unique<ActionQueue>() ), mTick(), mNextTick(), mSampleRate( sampleRate ), mSamplesRemainder(), mTicksPerSample( 16000000 / mSampleRate, 16000000 % mSampleRate )
{
  selectPOPCNT();
  enqueueSampling();
}

Mikey::~Mikey()
{
}

void Mikey::write( uint8_t address, uint8_t value )
{
  if ( auto action = mMikey->write( mTick, address, value ) )
  {
    mQueue->push( action );
  }
}

void Mikey::enqueueSampling()
{
  mTick = mNextTick & ~15;
  mNextTick = mNextTick + mTicksPerSample.first;
  mSamplesRemainder += mTicksPerSample.second;
  if ( mSamplesRemainder > mSampleRate )
  {
    mSamplesRemainder %= mSampleRate;
    mNextTick += 1;
  }

  mQueue->push( ( mNextTick & ~15 ) | 4 );
}

void Mikey::sampleAudio( int32_t* bufL, int32_t* bufR, size_t size )
{
  size_t i = 0;
  while ( i < size )
  {
    int64_t value = mQueue->pop();
    if ( ( value & 4 ) == 0 )
    {
      if ( auto newAction = mMikey->fireTimer( value ) )
      {
        mQueue->push( newAction );
      }
    }
    else
    {
      auto sample = mMikey->sampleAudio();
      bufL[i] = sample.left;
      bufR[i] = sample.right;
      i += 1;
      enqueueSampling();
    }
  }
}

uint8_t Mikey::read( uint8_t address )
{
  return mMikey->read( mTick, address );
}

void Mikey::reset()
{
  mMikey = std::make_unique<MikeyPimpl>();
  mQueue = std::make_unique<ActionQueue>();
  mTick = 0;
  mNextTick = 0;
  mSamplesRemainder = 0;
  selectPOPCNT();
  enqueueSampling();
}

void Mikey::mute( int channel, bool mute )
{
  mMikey->mute( channel, mute );
}

}

static void mikey_write( void* info, uint8_t address, uint8_t value )
{
  Lynx::Mikey* mikey = (Lynx::Mikey*)info;

  mikey->write( address, value );
}

static uint8_t mikey_read( void* info, uint8_t address )
{
  Lynx::Mikey* mikey = (Lynx::Mikey*)info;

  return mikey->read( address );
}

static void mikey_set_mute_mask( void* info, UINT32 mutes )
{
  Lynx::Mikey* mikey = (Lynx::Mikey*)info;

  for ( int i = 0; i < 4; ++i )
  {
    mikey->mute( i, ( mutes & ( 1 << i ) ) != 0 );
  }
}

static UINT8 mikey_start( const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf )
{
  Lynx::Mikey* mikey = nullptr;

  try
  {
    mikey = new Lynx::Mikey( cfg->smplRate );
  }
  catch ( ... )
  {
    return 0xff;
  }

  mikey->devData.chipInf = (void*)mikey;

  INIT_DEVINF( retDevInf, &mikey->devData, cfg->smplRate, &devDef );
  return 0x00;
}

static void mikey_stop( void* info )
{
  Lynx::Mikey* mikey = (Lynx::Mikey*)info;

  if ( mikey )
    delete mikey;
}

static void mikey_reset( void* info )
{
  Lynx::Mikey* mikey = (Lynx::Mikey*)info;
  mikey->reset();
}

static void mikey_update( void* info, UINT32 samples, DEV_SMPL** outputs )
{
  Lynx::Mikey* mikey = (Lynx::Mikey*)info;
  mikey->sampleAudio( outputs[0], outputs[1], samples );
}

