// license:MIT
// copyright-holders:laoo
// C++17 -> C90 backport by Valley Bell
#include "../../stdtype.h"
#include "../../stdbool.h"
#include "emutypes.h"
#include "../snddef.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"
#include "mikey.h"

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

  NULL, // SetOptionBits
  mikey_set_mute_mask,
  NULL, // SetPanning
  NULL, // SetSampleRateChangeCallback
  NULL, // SetLoggingCallback
  NULL, // LinkDevice

  devFunc,  // rwFuncs
};

const DEV_DEF* devDefList_Mikey[] =
{
  &devDef,
  NULL
};


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
static void selectPOPCNT()
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

#ifndef INT8_MIN
#define INT8_MIN         (-0x80)
#define INT8_MAX         0x7F
#define INT64_MAX        0x7FFFFFFFFFFFFFFFull
#endif

//static const int64_t CNT_MAX = std::numeric_limits<int64_t>::max() & ~15;
#define CNT_MAX (INT64_MAX & ~15)

static int32_t clamp_i32( int32_t v, int32_t lo, int32_t hi )
{
  return v < lo ? lo : ( v > hi ? hi : v );
}

static int64_t min_i64( int64_t v1, int64_t v2 )
{
  return v1 > v2 ? v2 : v1;
}

static int64_t max_i64( int64_t v1, int64_t v2 )
{
  return v1 < v2 ? v2 : v1;
}

// mikey_timer_t::CONTROLA : uint8_t
#define TMR_CTRLA_RESET_DONE      0x40  // 0b01000000
#define TMR_CTRLA_ENABLE_RELOAD   0x10  // 0b00010000
#define TMR_CTRLA_ENABLE_COUNT    0x08  // 0b00001000
#define TMR_CTRLA_AUD_CLOCK_MASK  0x07  // 0b00000111
// mikey_timer_t::CONTROLB : uint8_t
#define TMR_CTRLB_TIMER_DONE      0x08  // 0b00001000

// mikey_timer_t
typedef struct
{
  int64_t mValueUpdateTick;
  int mAudShift;
  bool mEnableReload;
  bool mEnableCount;
  bool mTimerDone;
  uint8_t mBackup;
  uint8_t mValue;
} mikey_timer_t;

static void mikey_timer_updateValue( mikey_timer_t* timer, int64_t tick );
static int64_t mikey_timer_computeTriggerTime( mikey_timer_t* timer, int64_t tick );
static int64_t mikey_timer_computeAction( mikey_timer_t* timer, int64_t tick );

// mikey_timer_t public:
static void mikey_timer_Timer( mikey_timer_t* timer )
{
  timer->mValueUpdateTick = 0;
  timer->mAudShift = 0;
  timer->mEnableReload = false;
  timer->mEnableCount = false;
  timer->mTimerDone = false;
  timer->mBackup = 0;
  timer->mValue = 0;
}

static int64_t mikey_timer_setBackup( mikey_timer_t* timer, int64_t tick, uint8_t backup )
{
  timer->mBackup = backup;
  return mikey_timer_computeAction( timer, tick );
}

static int64_t mikey_timer_setControlA( mikey_timer_t* timer, int64_t tick, uint8_t controlA )
{
  timer->mTimerDone ^= ( controlA & TMR_CTRLA_RESET_DONE ) != 0;
  timer->mEnableReload = ( controlA & TMR_CTRLA_ENABLE_RELOAD ) != 0;
  timer->mEnableCount = ( controlA & TMR_CTRLA_ENABLE_COUNT ) != 0;
  timer->mAudShift = controlA & TMR_CTRLA_AUD_CLOCK_MASK;

  return mikey_timer_computeAction( timer, tick );
}

static int64_t mikey_timer_setCount( mikey_timer_t* timer, int64_t tick, uint8_t value )
{
  return mikey_timer_computeTriggerTime( timer, tick );
}

static void mikey_timer_setControlB( mikey_timer_t* timer, uint8_t controlB )
{
  timer->mTimerDone = ( controlB & TMR_CTRLB_TIMER_DONE ) != 0;
}

static int64_t mikey_timer_fireAction( mikey_timer_t* timer, int64_t tick )
{
  timer->mTimerDone = true;

  return mikey_timer_computeAction( timer, tick );
}

static uint8_t mikey_timer_getBackup( const mikey_timer_t* timer )
{
  return timer->mBackup;
}

static uint8_t mikey_timer_getCount( mikey_timer_t* timer, int64_t tick )
{
  mikey_timer_updateValue( timer, tick );
  return timer->mValue;
}

//mikey_timer_t private:
static int64_t mikey_timer_scaleDiff( const mikey_timer_t* timer, int64_t older, int64_t newer )
{
  int64_t const mask = (int64_t)( ~0ull << ( timer->mAudShift + 4 ) );
  return ( ( newer & mask ) - ( older & mask ) ) >> ( timer->mAudShift + 4 );
}

static void mikey_timer_updateValue( mikey_timer_t* timer, int64_t tick )
{
  if ( timer->mEnableCount )
    timer->mValue = (uint8_t)max_i64( (int64_t)0, timer->mValue - mikey_timer_scaleDiff( timer, timer->mValueUpdateTick, tick ) );
  timer->mValueUpdateTick = tick;
}

static int64_t mikey_timer_computeTriggerTime( mikey_timer_t* timer, int64_t tick )
{
  if ( timer->mEnableCount && timer->mValue != 0 )
  {
    //tick value is increased by multipy of 16 (1 MHz resolution) lower bits are unchanged
    return tick + ( 1ull + timer->mValue ) * ( 1ull << ( timer->mAudShift + 4 ) );
  }
  else
  {
    return CNT_MAX;  //infinite
  }
}

static int64_t mikey_timer_computeAction( mikey_timer_t* timer, int64_t tick )
{
  mikey_timer_updateValue( timer, tick );
  if ( timer->mValue == 0 && timer->mEnableReload )
  {
    timer->mValue = timer->mBackup;
  }

  return mikey_timer_computeTriggerTime( timer, tick );
}
// mikey_timer_t end

// mikey_audio_channel_t
#define AC_FEEDBACK_7       0x80  // 0b10000000
#define AC_ENABLE_INTEGRATE 0x20  // 0b00100000
typedef struct
{
  mikey_timer_t mTimer;
  uint32_t mNumber;

  uint32_t mShiftRegister;
  uint32_t mTapSelector;
  bool mEnableIntegrate;
  int8_t mVolume;
  int8_t mOutput;
  uint8_t mCtrlA;
} mikey_audio_channel_t;

static int64_t mikey_audio_channel_adjust( const mikey_audio_channel_t* ac, int64_t tick );
static void mikey_audio_channel_trigger( mikey_audio_channel_t* ac );

//mikey_audio_channel_t public:
static void mikey_audio_channel_AudioChannel( mikey_audio_channel_t* ac, uint32_t number )
{
  mikey_timer_Timer( &ac->mTimer );
  ac->mNumber = number;
  ac->mShiftRegister = 0;
  ac->mTapSelector = 0;
  ac->mEnableIntegrate = false;
  ac->mVolume = 0;
  ac->mOutput = 0;
  ac->mCtrlA = 0;
  return;
}

static int64_t mikey_audio_channel_fireAction( mikey_audio_channel_t* ac, int64_t tick )
{
  mikey_audio_channel_trigger( ac );
  return mikey_audio_channel_adjust( ac, mikey_timer_fireAction( &ac->mTimer, tick ) );
}

static void mikey_audio_channel_setVolume( mikey_audio_channel_t* ac, int8_t value )
{
  ac->mVolume = value;
}

static void mikey_audio_channel_setFeedback( mikey_audio_channel_t* ac, uint8_t value )
{
  ac->mTapSelector = ( ac->mTapSelector & 0x3c0 ) | ( value & 0x3f ) | ( ( (int)value & 0xc0 ) << 4 );
}

static void mikey_audio_channel_setOutput( mikey_audio_channel_t* ac, uint8_t value )
{
  ac->mOutput = value;
}

static void mikey_audio_channel_setShift( mikey_audio_channel_t* ac, uint8_t value )
{
  ac->mShiftRegister = ( ac->mShiftRegister & 0xff00 ) | value;
}

static int64_t mikey_audio_channel_setBackup( mikey_audio_channel_t* ac, int64_t tick, uint8_t value )
{
  return mikey_audio_channel_adjust( ac, mikey_timer_setBackup( &ac->mTimer, tick, value ) );
}

static int64_t mikey_audio_channel_setControl( mikey_audio_channel_t* ac, int64_t tick, uint8_t value )
{
  if ( ac->mCtrlA == value )
    return 0;
  ac->mCtrlA = value;

  ac->mTapSelector = ( ac->mTapSelector & 0xf7f ) | ( value & AC_FEEDBACK_7 );
  ac->mEnableIntegrate = ( value & AC_ENABLE_INTEGRATE ) != 0;
  return mikey_audio_channel_adjust( ac, mikey_timer_setControlA( &ac->mTimer, tick, value & ~( AC_FEEDBACK_7 | AC_ENABLE_INTEGRATE ) ) );
}

static int64_t mikey_audio_channel_setCounter( mikey_audio_channel_t* ac, int64_t tick, uint8_t value )
{
  return mikey_audio_channel_adjust( ac, mikey_timer_setCount( &ac->mTimer, tick, value ) );
}

static void mikey_audio_channel_setOther( mikey_audio_channel_t* ac, uint8_t value )
{
  ac->mShiftRegister = ( ac->mShiftRegister & 0x0ff ) | ( ( (int)value & 0xf0 ) << 4 );
  mikey_timer_setControlB( &ac->mTimer, value & 0x0f );
}

static int8_t mikey_audio_channel_getOutput( const mikey_audio_channel_t* ac )
{
  return ac->mOutput;
}

static uint8_t mikey_audio_channel_readRegister( mikey_audio_channel_t* ac, int64_t tick, int reg )
{
  switch ( reg )
  {
  case 0:
    return ac->mVolume;
  case 1:
    return ac->mTapSelector & 0xff;
  case 2:
    return ac->mOutput;
  case 3:
    return ac->mShiftRegister & 0xff;
  case 4:
    return mikey_timer_getBackup( &ac->mTimer );
  case 5:
    return ac->mCtrlA;
  case 6:
    return mikey_timer_getCount( &ac->mTimer, tick );
  case 7:
    return ( ( ac->mShiftRegister >> 4 ) & 0xf0 );
  default:
    return 0xff;
  }
}

// mikey_audio_channel_t private:
static int64_t mikey_audio_channel_adjust( const mikey_audio_channel_t* ac, int64_t tick )
{
  //ticks are advancing in 1 MHz resolution, so lower 4 bits are unused.
  //timer number is encoded on lowest 2 bits.
  return tick | ac->mNumber;
}

static void mikey_audio_channel_trigger( mikey_audio_channel_t* ac )
{
  uint32_t xorGate = ac->mTapSelector & ac->mShiftRegister;
  uint32_t parity = popcnt( xorGate ) & 1;
  uint32_t newShift = ( ac->mShiftRegister << 1 ) | ( parity ^ 1 );
  ac->mShiftRegister = newShift;

  if ( ac->mEnableIntegrate )
  {
    int32_t temp = ac->mOutput + ( ( newShift & 1 ) ? ac->mVolume : -ac->mVolume );
    ac->mOutput = (int8_t)clamp_i32( temp, INT8_MIN, INT8_MAX );
  }
  else
  {
    ac->mOutput = ( newShift & 1 ) ? ac->mVolume : -ac->mVolume;
  }
}
// mikey_audio_channel_t end


/*
  "Queue" holding event timepoints.
  - 4 channel timer fire points
  - 1 sample point
  Time is in 16 MHz units but only with 1 MHz resolution.
  Four LSBs are used to encode event kind 0-3 are channels, 4 is sampling.
*/
// mikey_action_queue_t
#define AQ_TAB_SIZE 5
typedef struct
{
  int64_t mTab[AQ_TAB_SIZE];
} mikey_action_queue_t;

// mikey_action_queue_t public:
static void mikey_action_queue_ActionQueue( mikey_action_queue_t* aq )
{
  int i;
  for (i = 0; i < AQ_TAB_SIZE; i ++)
    aq->mTab[i] = CNT_MAX | i;
}

static void mikey_action_queue_push( mikey_action_queue_t* aq, int64_t value )
{
  size_t idx = value & 15;
  if ( idx < AQ_TAB_SIZE )
  {
    if ( value & ~15 )
    {
      //writing only non-zero values
      aq->mTab[idx] = value;
    }
  }
}

static int64_t mikey_action_queue_pop( mikey_action_queue_t* aq )
{
  int64_t min1 = min_i64( aq->mTab[0], aq->mTab[1] );
  int64_t min2 = min_i64( aq->mTab[2], aq->mTab[3] );
  int64_t min3 = min_i64( min1, aq->mTab[4] );
  int64_t min4 = min_i64( min2, min3 );

  //assert( ( min4 & 15 ) < AQ_TAB_SIZE );
  aq->mTab[min4 & 15] = CNT_MAX | ( min4 & 15 );

  return min4;
}


typedef struct
{
  int16_t left;
  int16_t right;
} mikey_audio_sample_t;

// mikey_pimpl_t
typedef struct
{
  mikey_audio_channel_t mAudioChannels[4];
  int mAttenuationLeft[4];
  int mAttenuationRight[4];
  bool mMute[4];
  #define MIKEY_REGPOOL_SIZE  (4 * 8 + 4)
  uint8_t mRegisterPool[MIKEY_REGPOOL_SIZE];

  uint8_t mPan;
  uint8_t mStereo;
} mikey_pimpl_t;

// mikey_pimpl_t public:
#define VOLCNTRL 0x0
#define FEEDBACK 0x1
#define OUTPUT 0x2
#define SHIFT 0x3
#define BACKUP 0x4
#define CONTROL 0x5
#define COUNTER 0x6
#define OTHER 0x7

#define ATTENREG0 0x40
#define ATTENREG1 0x41
#define ATTENREG2 0x42
#define ATTENREG3 0x43
#define MPAN 0x44
#define MSTEREO 0x50

static void mikey_pimpl_MikeyPimpl( mikey_pimpl_t* mikey )
{
  int i;
  for (i = 0; i < 4; i ++)
    mikey_audio_channel_AudioChannel( &mikey->mAudioChannels[i], i );
  mikey->mPan = 0xff;
  mikey->mStereo = 0;
  for (i = 0; i < MIKEY_REGPOOL_SIZE; i ++)
    mikey->mRegisterPool[i] = (uint8_t)0xff;
  for (i = 0; i < 4; i ++)
  {
    mikey->mAttenuationLeft[i] = 0x3c;
    mikey->mAttenuationRight[i] = 0x3c;
    mikey->mMute[i] = false;
  }
}

static int64_t mikey_pimpl_write( mikey_pimpl_t* mikey, int64_t tick, uint8_t address, uint8_t value )
{
  //assert( address >= 0x20 );
  if ( address < 0x20 )
    return 0;

  if ( address < 0x40 )
  {
    size_t idx = ( address >> 3 ) & 3;
    switch ( address & 0x7 )
    {
    case VOLCNTRL:
      mikey_audio_channel_setVolume( &mikey->mAudioChannels[idx], (int8_t)value );
      break;
    case FEEDBACK:
      mikey_audio_channel_setFeedback( &mikey->mAudioChannels[idx], value );
      break;
    case OUTPUT:
      mikey_audio_channel_setOutput( &mikey->mAudioChannels[idx], value );
      break;
    case SHIFT:
      mikey_audio_channel_setShift( &mikey->mAudioChannels[idx], value );
      break;
    case BACKUP:
      return mikey_audio_channel_setBackup( &mikey->mAudioChannels[idx], tick, value );
    case CONTROL:
      return mikey_audio_channel_setControl( &mikey->mAudioChannels[idx], tick, value );
    case COUNTER:
      return mikey_audio_channel_setCounter( &mikey->mAudioChannels[idx], tick, value );
    case OTHER:
      mikey_audio_channel_setOther( &mikey->mAudioChannels[idx], value );
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
      mikey->mRegisterPool[8*4+idx] = value;
      mikey->mAttenuationRight[idx] = ( value & 0x0f ) << 2;
      mikey->mAttenuationLeft[idx] = ( value & 0xf0 ) >> 2;
      break;
    case MPAN:
      mikey->mPan = value;
      break;
    case MSTEREO:
      mikey->mStereo = value;
      break;
    default:
      break;
    }
  }
  return 0;
}

static int64_t mikey_pimpl_fireTimer( mikey_pimpl_t* mikey, int64_t tick )
{
  size_t timer = tick & 0x0f;
  //assert( timer < 4 );
  return mikey_audio_channel_fireAction( &mikey->mAudioChannels[timer], tick );
}

static mikey_audio_sample_t mikey_pimpl_sampleAudio( const mikey_pimpl_t* mikey )
{
  int left = 0;
  int right = 0;
  size_t i;
  mikey_audio_sample_t as;

  for ( i = 0; i < 4; ++i )
  {
    if ( mikey->mMute[i] )
      continue;

    if ( ( mikey->mStereo & ( (uint8_t)0x01 << i ) ) == 0 )
    {
      const int attenuation = ( mikey->mPan & ( (uint8_t)0x01 << i ) ) != 0 ? mikey->mAttenuationLeft[i] : 0x3c;
      left += mikey_audio_channel_getOutput( &mikey->mAudioChannels[i] ) * attenuation;
    }

    if ( ( mikey->mStereo & ( (uint8_t)0x10 << i ) ) == 0 )
    {
      const int attenuation = ( mikey->mPan & ( (uint8_t)0x01 << i ) ) != 0 ? mikey->mAttenuationRight[i] : 0x3c;
      right += mikey_audio_channel_getOutput( &mikey->mAudioChannels[i] ) * attenuation;
    }
  }

  as.left = (int16_t)left;
  as.right = (int16_t)right;
  return as;
}

static uint8_t mikey_pimpl_read( mikey_pimpl_t* mikey, int64_t tick, int address )
{
  size_t i = address >> 3;
  if ( i < 4 )
  {
    return mikey_audio_channel_readRegister( &mikey->mAudioChannels[i], tick, address & 7 );
  }
  else if ( (size_t)address < MIKEY_REGPOOL_SIZE )
    return mikey->mRegisterPool[address];
  else
    return 0xff;
}

static void mikey_pimpl_mute( mikey_pimpl_t* mikey, int channel, bool mute )
{
  mikey->mMute[channel] = mute;
}


// mikey_t
typedef struct
{
  DEV_DATA devData;
  mikey_pimpl_t mMikey;
  mikey_action_queue_t mQueue;
  uint64_t mTick;
  uint64_t mNextTick;
  uint32_t mSampleRate;
  uint32_t mSamplesRemainder;
  uint32_t mTicksPerSample1;
  uint32_t mTicksPerSample2;
} mikey_t;

static void mikey_enqueueSampling( mikey_t* mikey );

static UINT8 mikey_start( const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf )
{
  mikey_t* mikey;

  mikey = (mikey_t*)calloc(1, sizeof(mikey_t));
  if (mikey == NULL)
    return 0xFF;

  //mikey_pimpl_MikeyPimpl( &mikey->mMikey );
  //mikey_action_queue_ActionQueue( &mikey->mQueue );
  mikey->mSampleRate = cfg->smplRate;
  mikey->mTicksPerSample1 = 16000000 / mikey->mSampleRate;
  mikey->mTicksPerSample2 = 16000000 % mikey->mSampleRate;
  selectPOPCNT();

  mikey->devData.chipInf = (void*)mikey;
  INIT_DEVINF( retDevInf, &mikey->devData, cfg->smplRate, &devDef );
  return 0x00;
}

static void mikey_reset( void* info )
{
  mikey_t* mikey = (mikey_t*)info;
  mikey_pimpl_MikeyPimpl( &mikey->mMikey );
  mikey_action_queue_ActionQueue( &mikey->mQueue );
  mikey->mTick = 0;
  mikey->mNextTick = 0;
  mikey->mSamplesRemainder = 0;
  mikey_enqueueSampling( mikey );
}

static void mikey_stop( void* info )
{
  mikey_t* mikey = (mikey_t*)info;

  free( mikey );
}

static void mikey_write( void* info, uint8_t address, uint8_t value )
{
  mikey_t* mikey = (mikey_t*)info;

  int64_t action = mikey_pimpl_write( &mikey->mMikey, mikey->mTick, address, value );
  if ( action )
  {
    mikey_action_queue_push( &mikey->mQueue, action );
  }
}

static void mikey_enqueueSampling( mikey_t* mikey )
{
  mikey->mTick = mikey->mNextTick & ~15;
  mikey->mNextTick = mikey->mNextTick + mikey->mTicksPerSample1;
  mikey->mSamplesRemainder += mikey->mTicksPerSample2;
  if ( mikey->mSamplesRemainder > mikey->mSampleRate )
  {
    mikey->mSamplesRemainder %= mikey->mSampleRate;
    mikey->mNextTick += 1;
  }

  mikey_action_queue_push( &mikey->mQueue, ( mikey->mNextTick & ~15 ) | 4 );
}

static void mikey_update( void* info, UINT32 samples, DEV_SMPL** outputs )
{
  mikey_t* mikey = (mikey_t*)info;
  UINT32 i = 0;
  while ( i < samples )
  {
    int64_t value = mikey_action_queue_pop( &mikey->mQueue );
    if ( ( value & 4 ) == 0 )
    {
      int64_t newAction = mikey_pimpl_fireTimer( &mikey->mMikey, value );
      if ( newAction )
      {
        mikey_action_queue_push( &mikey->mQueue, newAction );
      }
    }
    else
    {
      mikey_audio_sample_t sample = mikey_pimpl_sampleAudio( &mikey->mMikey );
      outputs[0][i] = sample.left;
      outputs[1][i] = sample.right;
      i += 1;
      mikey_enqueueSampling( mikey );
    }
  }
}

static uint8_t mikey_read( void* info, uint8_t address )
{
  mikey_t* mikey = (mikey_t*)info;

  return mikey_pimpl_read( &mikey->mMikey, mikey->mTick, address );
}

static void mikey_set_mute_mask( void* info, UINT32 mutes )
{
  mikey_t* mikey = (mikey_t*)info;
  int i;

  for ( i = 0; i < 4; ++i )
  {
    mikey_pimpl_mute( &mikey->mMikey, i, ( mutes & ( 1 << i ) ) != 0 );
  }
}
