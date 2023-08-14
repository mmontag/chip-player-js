import Player from "./Player.js";
import autoBind from 'auto-bind';

const INT16_MAX = Math.pow(2, 16) - 1;
const XMP_PLAYER_STATE = 8;
const XMP_STATE_PLAYING = 2;
const fileExtensions = [
  // libxmp-lite:
  'it',  //  Impulse Tracker  1.00, 2.00, 2.14, 2.15
  'mod', //  Sound/Noise/Protracker M.K., M!K!, M&K!, N.T., CD81
  's3m', //  Scream Tracker 3 3.00, 3.01+
  'xm',  //  Fast Tracker II  1.02, 1.03, 1.04
];

// noinspection PointlessArithmeticExpressionJS
export default class XMPPlayer extends Player {
  constructor(...args) {
    super(...args);
    autoBind(this);

    this.name = 'XMP Player';
    this.xmpCtx = this.core._xmp_create_context();
    this.infoPtr = this.core._malloc(2048);
    this.fileExtensions = fileExtensions;
    this.initialBPM = 125;
    this.tempoScale = 1; // TODO: rename to speed
    this._positionMs = 0;
    this._durationMs = 1000;
    this.buffer = this.core._malloc(this.bufferSize * 16); // i16
    this.infoTexts = [];
  }

  processAudioInner(channels) {
    let i, ch, err;
    const infoPtr = this.infoPtr;

    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    err = this.core._xmp_play_buffer(this.xmpCtx, this.buffer, this.bufferSize * 4, 1);
    if (err === -1) {
      this.stop();
    } else if (err !== 0) {
      this.suspend();
      console.error("xmp_play_buffer failed. error code: %d", err);
      throw Error('xmp_play_buffer failed');
    }

    // Get current module BPM
    // see http://xmp.sourceforge.net/libxmp.html#id25
    this.core._xmp_get_frame_info(this.xmpCtx, infoPtr);
    const bpm = this.core.getValue(infoPtr + 6 * 4, 'i32');
    this._positionMs = this.core.getValue(infoPtr + 7 * 4, 'i32'); // xmp_frame_info.time
    this._maybeInjectTempo(bpm);

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
          ch * 2,                 // channel offset * bytes per sample
          'i16'                   // the sample values are signed 16-bit integers
        ) / INT16_MAX;            // convert int16 to float
      }
    }
  }

  _parseMetadata() {
    const meta = {};
    const infoTexts = [];
    const xmp = this.core;
    const infoPtr = this.infoPtr;

    // Match layout of xmp_module_info struct
    // http://xmp.sourceforge.net/libxmp.html
    // #void-xmp-get-module-info-xmp-context-c-struct-xmp-module-info-info
    xmp._xmp_get_module_info(this.xmpCtx, infoPtr);
    const xmp_modulePtr = xmp.getValue(infoPtr + 20, '*');
    meta.title = xmp.UTF8ToString(xmp_modulePtr, 256);
    meta.system = xmp.UTF8ToString(xmp_modulePtr + 64, 256);
    meta.comment = xmp.UTF8ToString(xmp.getValue(infoPtr + 24, '*'), 512);

    xmp._xmp_get_frame_info(this.xmpCtx, infoPtr);
    this._durationMs = xmp.getValue(infoPtr + 8 * 4, 'i32');

    // struct xmp_module {
    //   char name[XMP_NAME_SIZE];	/* Module title */
    //   char type[XMP_NAME_SIZE];	/* Module format */
    //   int pat;			/* Number of patterns */
    //   int trk;			/* Number of tracks */
    //   int chn;			/* Tracks per pattern */
    //   int ins;			/* Number of instruments */
    //   int smp;			/* Number of samples */
    //   int spd;			/* Initial speed */
    //   int bpm;			/* Initial BPM */
    //   int len;			/* Module length in patterns */
    //   int rst;			/* Restart position */
    //   int gvl;			/* Global volume */
    //
    //   struct xmp_pattern **xxp;	/* Patterns */
    //   struct xmp_track **xxt;		/* Tracks */
    //   struct xmp_instrument *xxi;	/* Instruments */
    //   struct xmp_sample *xxs;		/* Samples */
    //   struct xmp_channel xxc[XMP_MAX_CHANNELS]; /* Channel info */
    //   unsigned char xxo[XMP_MAX_MOD_LENGTH];	/* Orders */
    // };

    // struct xmp_instrument {
    //   char name[32];			/* Instrument name */
    //   int vol;			/* Instrument volume */
    //   int nsm;			/* Number of samples */
    //   int rls;			/* Release (fadeout) */
    //   struct xmp_envelope aei;	/* Amplitude envelope info */
    //   struct xmp_envelope pei;	/* Pan envelope info */
    //   struct xmp_envelope fei;	/* Frequency envelope info */
    //
    //   struct {
    //     unsigned char ins;	/* Instrument number for each key */
    //     signed char xpo;	/* Instrument transpose for each key */
    //   } map[XMP_MAX_KEYS];
    //
    //   struct xmp_subinstrument	*sub;
    //
    //   void *extra;			/* Extra fields */
    // };

    // struct xmp_sample {
    //   char name[32];			/* Sample name */
    //   int len;			/* Sample length */
    //   int lps;			/* Loop start */
    //   int lpe;			/* Loop end */
    //   #define XMP_SAMPLE_16BIT	(1 << 0)  /* 16bit sample */
    //   #define XMP_SAMPLE_LOOP		(1 << 1)  /* Sample is looped */
    //   #define XMP_SAMPLE_LOOP_BIDIR	(1 << 2)  /* Bidirectional sample loop */
    //   #define XMP_SAMPLE_LOOP_REVERSE	(1 << 3)  /* Backwards sample loop */
    //   #define XMP_SAMPLE_LOOP_FULL	(1 << 4)  /* Play full sample before looping */
    //   #define XMP_SAMPLE_SLOOP	(1 << 5)  /* Sample has sustain loop */
    //   #define XMP_SAMPLE_SLOOP_BIDIR	(1 << 6)  /* Bidirectional sustain loop */
    //   #define XMP_SAMPLE_SYNTH	(1 << 15) /* Data contains synth patch */
    //   int flg;			/* Flags */
    //   unsigned char *data;		/* Sample data */
    // };


    // XMP-specific metadata
    meta.patterns =        xmp.getValue(xmp_modulePtr + 128 + 4 * 0, 'i32'); // patterns
    meta.tracks =          xmp.getValue(xmp_modulePtr + 128 + 4 * 1, 'i32'); // tracks
    meta.numChannels =     xmp.getValue(xmp_modulePtr + 128 + 4 * 2, 'i32'); // tracks per pattern
    meta.numInstruments =  xmp.getValue(xmp_modulePtr + 128 + 4 * 3, 'i32'); // instruments
    meta.numSamples =      xmp.getValue(xmp_modulePtr + 128 + 4 * 4, 'i32'); // samples
    meta.initialSpeed =    xmp.getValue(xmp_modulePtr + 128 + 4 * 5, 'i32'); // initial speed
    meta.initialBPM =      xmp.getValue(xmp_modulePtr + 128 + 4 * 6, 'i32'); // initial bpm
    meta.moduleLength =    xmp.getValue(xmp_modulePtr + 128 + 4 * 7, 'i32'); // module length
    meta.restartPosition = xmp.getValue(xmp_modulePtr + 128 + 4 * 8, 'i32'); // restart position
    const xmp_instPtr =    xmp.getValue(xmp_modulePtr + 128 + 4 * 12, 'i32');
    const xmp_sampPtr =    xmp.getValue(xmp_modulePtr + 128 + 4 * 13, 'i32');

    // size of xmp_envelope struct is 7 * 4 + 128 = 156 bytes
    const envStructSize = 156;
    // size of xmp_instrument map struct is 2 * 122 (rounded up from 121) = 244 bytes
    const mapStructSize = 244;
    // size of xmp_instrument struct is 32 + 4 + 4 + 4 + 156 + 156 + 156 + 242 + 4 + 4
    const instStructSize = 32 + 4 + 4 + 4 + (envStructSize * 3) + mapStructSize + 4 + 4;
    // for (let j = instStructSize - 32; j < instStructSize + 32; j++) {
      const instStrings = [];
      for (let i = 0; i < meta.numInstruments; i++) {
        const ptr = xmp_instPtr + i * instStructSize;
        instStrings.push(xmp.UTF8ToString(ptr));
      }
    // }
    infoTexts.push('Instruments\n===========\n' + instStrings.join('\n'));

    // Filename fallback
    if (!meta.title) meta.title = this.filepathMeta.title;

    this.initialBPM = meta.initialBPM;

    this.metadata = meta;
    this.infoTexts = infoTexts;
  }

  loadData(data, filename) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    err = this.core.ccall(
      'xmp_load_module_from_memory', 'number',
      ['number', 'array', 'number'],
      [this.xmpCtx, data, data.length]
    );
    if (err !== 0) {
      console.error("xmp_load_module_from_memory failed. error code: %d", err);
      throw Error('xmp_load_module_from_memory failed');
    }

    err = this.core._xmp_start_player(this.xmpCtx, this.sampleRate, 0);
    if (err !== 0) {
      console.error('xmp_start_player failed. error code: %d', err);
      throw Error('xmp_start_player failed');
    }

    this._parseMetadata(filename);

    this.resume();
    this.emit('playerStateUpdate', {
      ...this.getBasePlayerState(),
      isStopped: false
    });
  }

  getVoiceMask() {
    const voiceMask = [];
    for (let i = 0; i < this.metadata.numChannels; i++) {
      voiceMask.push(!this.core._xmp_channel_mute(this.xmpCtx, i, -1));
    }
    return voiceMask;
  }

  setVoiceMask(voiceMask) {
    voiceMask.forEach((isEnabled, i) => {
      this.core._xmp_channel_mute(this.xmpCtx, i, isEnabled ? 0 : 1);
    });
  }

  getTempo() {
    return this.tempoScale;
  }

  setTempo(val) {
    if (this.metadata && !this.metadata.initialSpeed) {
      console.log('Unable to set speed for %s.', this.filepathMeta.title);
      return;
    }
    this.tempoScale = val;
  }

  _maybeInjectTempo(measuredBPM) {
    const xmp = this.core;
    const minBPM = 20;
    const maxBPM = 255;
    const targetBPM = Math.floor(Math.max(Math.min(this.metadata.initialBPM * this.tempoScale, maxBPM), minBPM));

    if (targetBPM === measuredBPM) return;

    console.log('Injecting %d BPM into libxmp. (Initial: %d)', targetBPM, this.metadata.initialBPM);
    const xmp_eventPtr = xmp._malloc(8);
    for (let i = 0; i < 8; i++) xmp.setValue(xmp_eventPtr + i, 0, 'i8');
    xmp.setValue(xmp_eventPtr + 3, 0x87, 'i8');
    xmp.setValue(xmp_eventPtr + 4, targetBPM, 'i32');
    xmp._xmp_inject_event(this.xmpCtx, 0, xmp_eventPtr);
  }

  getVoiceName(index) {
    return `Ch ${index + 1}`;
  }

  getNumVoices() {
    return this.metadata.numChannels;
  }

  getNumSubtunes() {
    return 0;
  }

  getPositionMs() {
    return this._positionMs;
  }

  getDurationMs() {
    return this._durationMs;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    const playingState = this.core._xmp_get_player(this.xmpCtx, XMP_PLAYER_STATE);
    return !this.isPaused() && playingState === XMP_STATE_PLAYING;
  }

  seekMs(seekMs) {
    this.core._xmp_seek_time(this.xmpCtx, seekMs);
  }

  stop() {
    this.suspend();
    this.core._xmp_stop_module(this.xmpCtx);
    console.debug('XMPPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
