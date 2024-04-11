// Super Nintendo SPC music file emulator

// Game_Music_Emu $vers
#ifndef SPC_EMU_H
#define SPC_EMU_H

#include "Music_Emu.h"
#include "Snes_Spc.h"
#include "Spc_Filter.h"

#define SPC_LESS_ACCURATE 1
#define GME_SPC_FAST_RESAMPLER 1

#if GME_SPC_FAST_RESAMPLER
#include "Upsampler.h"
	typedef Upsampler Spc_Emu_Resampler;
#else
#include "Fir_Resampler.h"
typedef Fir_Resampler<24> Spc_Emu_Resampler;
#endif


class Spc_Emu : public Music_Emu {
public:
  // The Super Nintendo hardware samples at 32kHz. Other sample rates are
  // handled by resampling the 32kHz output; emulation accuracy is not affected.
  enum { native_sample_rate = 32000 };

  // Disables annoying pseudo-surround effect some music uses
  void disable_surround( bool disable = true )    { apu.disable_surround( disable ); }

  // Enables gaussian, cubic or sinc interpolation
  void interpolation_level( int level = 0 )   { apu.interpolation_level( level ); }

  Snes_Spc const* get_apu() const;
  Snes_Spc * get_apu();

  // SPC file header
  struct header_t
  {
    enum { size = 0x100 };

    char tag       [35];
    byte format;
    byte version;
    byte pc        [ 2];
    byte a, x, y, psw, sp;
    byte unused    [ 2];
    char song      [32];
    char game      [32];
    char dumper    [16];
    char comment   [32];
    byte date      [11];
    byte len_secs  [ 3];
    byte fade_msec [ 4];
    char author    [32]; // sometimes first char should be skipped (see official SPC spec)
    byte mute_mask;
    byte emulator;
    byte unused2   [46];
  };

  // Header for currently loaded file
  header_t const& header() const                  { return *(header_t const*) file_begin(); }

  blargg_err_t hash_( Hash_Function& ) const;

  static gme_type_t static_type()                 { return gme_spc_type; }

  // Implementation
public:
  Spc_Emu();
  ~Spc_Emu();

protected:
  virtual blargg_err_t load_mem_( byte const [], int );
  virtual blargg_err_t track_info_( track_info_t*, int track ) const;
  virtual blargg_err_t set_sample_rate_( int );
  virtual blargg_err_t start_track_( int );
  virtual blargg_err_t play_( int, sample_t [] );
  virtual blargg_err_t skip_( int );
  virtual void mute_voices_( int );
  virtual void set_tempo_( double );

private:
  Spc_Emu_Resampler resampler;
  Spc_Filter filter;
  Snes_Spc apu;

  byte const* trailer_() const;
  int trailer_size_() const;
  blargg_err_t play_and_filter( int count, sample_t out [] );
};

inline Snes_Spc const* Spc_Emu::get_apu() const { return &apu; }
inline Snes_Spc * Spc_Emu::get_apu() { return &apu; }

#endif
