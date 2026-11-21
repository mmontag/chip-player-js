import autoBind from 'auto-bind';
import pathe from 'pathe';
import Player from "./Player.js";
import SubBass from "../effects/SubBass";
import { allOrNone } from '../util';

let core = null;

const INT16_MAX = 65535;
// "timesliced" seek in increments to prevent blocking UI/audio callback.
const TIMESLICED_SEEK_MS_MAP = {
  '.spc': 10000,
};
const fileExtensions = [
  'nsf',
  'nsfe',
  'spc',
  'ay',
  'gbs',
];

export default class GMEPlayer extends Player {
  paramDefs = [
    {
      id: 'subbass',
      label: 'Sub Bass',
      type: 'number',
      min: 0.0,
      max: 2.0,
      step: 0.01,
      defaultValue: 0.0,
    },
    {
      id: 'stereoWidth',
      label: 'Stereo Width',
      hint: 'Channel panning for NSF, GBS, and AY files.',
      type: 'number',
      min: 0.0,
      max: 1.0,
      step: 0.01,
      defaultValue: 1.0,
    },
    {
      id: 'disableEcho',
      label: 'Disable SPC Echo',
      hint: 'Disable echo effect for Super Nintendo SPC files.',
      type: 'toggle',
      defaultValue: false,
    },
    {
      id: 'enableAccuracy',
      label: 'Accurate SPC Filter',
      hint: 'Simple low-pass and high-pass filter to better match sound output of the SNES.',
      type: 'toggle',
      defaultValue: false,
    },
    {
      id: 'indefinitePlayback',
      label: 'Indefinite Playback',
      type: 'toggle',
      hint: 'Ignore track length metadata for looping tracks and loop indefinitely.',
      defaultValue: false,
    },
  ];

  constructor(...args) {
    super(...args);
    autoBind(this);

    core = this.core;

    this.playerKey = 'gme';
    this.name = 'Game Music Emu Player';
    this.paused = false;
    this.fileExtensions = fileExtensions;
    this.subtune = 0;
    this.tempo = 1.0;
    this.params = {};
    this.voiceMask = []; // GME does not expose a method to get the current voice mask
    this.gmeCtx = null;

    // Seek timeslicing TODO: move to base Player
    this.seekRequestId = null;
    this.seekTargetMs = null;
    this.currentFileExt = null;

    this.buffer = core._malloc(this.bufferSize * 16); // i16
    this.emuPtr = core._malloc(4); // i32

    this.subBass = new SubBass(this.sampleRate);
    this.hostFade = true;
  }

  processAudioInner(channels) {
    let ch, i;
    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    const playIndefinitely = this.isPlayingIndefinitely();
    const trackEnded = core._gme_track_ended(this.gmeCtx) === 1;

    if (!playIndefinitely && trackEnded) {
      this.handleSongEnd();
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    if (playIndefinitely && trackEnded) {
      // GME has no loop API, and once it reports the track ended gme_play only
      // produces silence. Restart the track to loop it (repeat-one or the
      // Indefinite Playback setting). Looping chip drivers (e.g. NSF) never
      // report track ended; they loop internally, so there is nothing to
      // restart -- letting them run is the seamless case. Never restart at
      // the track length: that would cut a seamlessly looping driver with a
      // hard restart.
      this.restartTrack();
    }

    core._gme_play(this.gmeCtx, this.bufferSize * 2, this.buffer);

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = core.getValue(
          this.buffer +
          i * 2 * 2 +
          ch * 2,
          'i16') / INT16_MAX;
      }
    }

    if (this.params.subbass > 0) {
      for (i = 0; i < this.bufferSize; i++) {
        const sub = this.subBass.process(channels[0][i]) * this.params.subbass;
        for (ch = 0; ch < channels.length; ch++) {
          channels[ch][i] += sub;
        }
      }
    }
  }

  playSubtune(subtune) {
    super.playSubtune(subtune);
    this.subtune = subtune;
    this.metadata = this._parseMetadata(subtune);
    console.debug('GMEPlayer.playSubtune(subtune=%s)', subtune);
    this.emit('playerStateUpdate', this.getBasePlayerState());
    const res = core._gme_start_track(this.gmeCtx, subtune);
    if (this.gmeCtx) {
      core._gme_set_fade(this.gmeCtx, 200000000);
    }
    return res;
  }

  async loadData(data, filepath, persistedSettings, subtune = 0) {
    this.subtune = subtune;
    this.currentFileExt = pathe.extname(filepath);
    this.filepathMeta = Player.metadataFromFilepath(filepath);

    const dataPtr = this.copyToHeap(data);
    const err = core._gme_open_data(dataPtr, data.length, this.emuPtr, this.sampleRate);
    core._free(dataPtr);

    if (err !== 0) {
      this.stop();
      throw Error('gme_open_data failed');
    }
    this.gmeCtx = core.getValue(this.emuPtr, "i32");
    this.voiceMask = Array(core._gme_voice_count(this.gmeCtx)).fill(true);

    core._gme_ignore_silence(this.gmeCtx, 0);
    this.resolveParamValues(persistedSettings);
    this.setTempo(persistedSettings.tempo || 1);
    this.resume();
    if (this.playSubtune(this.subtune) !== 0) {
      this.stop();
      throw Error('gme_start_track failed');
    }
  }

  _parseMetadata(subtune) {
    const metadataPtr = core._malloc(4); // i32
    if (core._gme_track_info(this.gmeCtx, metadataPtr, subtune) !== 0)
      console.error("could not load metadata");
    const ref = core.getValue(metadataPtr, "*");

    let offset = 0;

    const readInt32 = function () {
      var value = core.getValue(ref + offset, "i32");
      offset += 4;
      return value;
    };

    const readString = function () {
      let value = '';

      // Interpret as UTF8 (disabled)
      // value = core.UTF8ToString(core.getValue(ref + offset, "i8*"));

      // Interpret as ISO-8859-1 (unsigned integer values, 0 to 255)
      const ptr = core.getValue(ref + offset, 'i8*');
      for (let i = 0; i < 255; i++) {
        let char = core.getValue(ptr + i, 'i8');
        if (char === 0) {
          break;
        } else if (char < 0) {
          char = (Math.abs(char) ^ 255) + 1;
        }
        value += String.fromCharCode(char);
      }

      offset += 4;
      return value;
    };

    const meta = {};

    meta.length = readInt32();
    meta.intro_length = readInt32();
    meta.loop_length = readInt32();
    meta.play_length = readInt32();

    offset += 4 * 12; // skip unused bytes

    meta.system = readString();
    meta.game = readString();
    meta.title = readString() || meta.game || this.filepathMeta.title;
    meta.artist = readString() || this.filepathMeta.artist;
    meta.copyright = readString();
    meta.comment = readString();

    meta.formatted = {
      title: meta.game === meta.title ?
        meta.title :
        allOrNone(meta.game, ' - ') + meta.title,
      subtitle: [meta.artist, meta.system].filter(x => x).join(' - ') +
        allOrNone(' (', meta.copyright, ')'),
    };

    return meta;
  }

  getVoiceGroups() {
    if (!this.gmeCtx) return [];
    const numVoices = core._gme_voice_count(this.gmeCtx);
    const voices = [];
    for (let i = 0; i < numVoices; i++) {
      const name = core.UTF8ToString(core._gme_voice_name(this.gmeCtx, i));
      voices.push({
        idx: i,
        name: name || `Voice ${i + 1}`,
      });
    }
    return [{
      name: this.metadata?.system || 'Voices',
      icon: !!this.metadata?.system,
      voices,
    }];
  }

  getNumSubtunes() {
    if (this.gmeCtx) return core._gme_track_count(this.gmeCtx);
  }

  getSubtune() {
    return this.subtune;
  }

  getPositionMs() {
    if (this.gmeCtx) return core._gme_tell_scaled(this.gmeCtx);
    return 0;
  }

  getDurationMs() {
    if (this.gmeCtx && this.metadata) return this.metadata.play_length;
    return 0;
  }


  getParameter(id) {
    return this.params[id];
  }

  setParameter(id, value) {
    switch (id) {
      case 'subbass':
        this.params[id] = parseFloat(value);
        break;
      case 'stereoWidth':
        this.params[id] = parseFloat(value);
        if (this.gmeCtx) core._gme_set_stereo_depth(this.gmeCtx, value);
        break;
      case 'disableEcho':
        this.params[id] = !!value;
        // gme_disable_echo is a 0.6.4+ API; older builds omit it.
        if (this.gmeCtx && core._gme_disable_echo) core._gme_disable_echo(this.gmeCtx, value ? 1 : 0);
        break;
      case 'enableAccuracy':
        this.params[id] = !!value;
        if (this.gmeCtx) core._gme_enable_accuracy(this.gmeCtx, value ? 1 : 0);
        break;
      case 'indefinitePlayback':
        super.setParameter(id, !!value);
        break;
      default:
        console.warn('GMEPlayer has no parameter with id "%s".', id);
    }
  }

  isPlaying() {
    return !this.isPaused() && core._gme_track_ended(this.gmeCtx) !== 1;
  }

  getTempo() {
    return this.tempo;
  }

  setTempo(val) {
    this.tempo = val;
    if (this.gmeCtx) core._gme_set_tempo(this.gmeCtx, val);
  }

  setLooping(looping) {
    super.setLooping(looping);
    // Enabling Repeat One during the inter-song silence tail must cancel the
    // queued end action, or the base Player's silence countdown still stops the
    // song after repeat is on. Only on enable -- a toggle off leaves the ending
    // in place.
    if (looping) {
      this.silenceSamplesRemaining = 0;
    }
  }

  // Indefinite Playback behaves like Repeat One: the engine keeps rendering
  // past durationMs, so the base end detector must stay out of the way.
  isPlayingIndefinitely() {
    return this.looping || !!this.params.indefinitePlayback;
  }

  getDisplayPositionMs() {
    // Playlist position for the slider head. Tracks with a real loop region
    // fold into the band while looping; anything else shows the real
    // position. Blind loops (indefinite with no known region) are handled in
    // the footer, which parks the head and lets the elapsed time climb.
    const abs = this.getPositionMs();
    const band = this.getLoopBandMs();
    if (band) {
      if (abs <= band.startMs) return abs;
      if (!this.isPlayingIndefinitely()) return abs;
      const loopLength = band.endMs - band.startMs;
      if (loopLength <= 0) return abs;
      return band.startMs + ((abs - band.startMs) % loopLength);
    }
    return abs;
  }

  restartTrack() {
    this.resetFade();
    this.silenceSamplesRemaining = 0;
    core._gme_start_track(this.gmeCtx, this.subtune);
    // gme_start_track resets the fade; push it far out again so the restarted
    // track keeps looping instead of being cut off by the next fade.
    core._gme_set_fade(this.gmeCtx, 200000000);
  }

  getVoiceMask() {
    return this.voiceMask;
  }

  setVoiceMask(voiceMask) {
    if (this.gmeCtx) {
      let bitmask = 0;
      voiceMask.forEach((isEnabled, i) => {
        if (!isEnabled) {
          bitmask += 1 << i;
        }
      });
      core._gme_mute_voices(this.gmeCtx, bitmask);
      // Disable silence detection if any voice is muted.
      core._gme_ignore_silence(this.gmeCtx, bitmask === 0 ? 0 : 1);
      console.log('GMEPlayer: Silence detection is %s.', bitmask === 0 ? 'enabled' : 'disabled');
      this.voiceMask = voiceMask;
    }
  }

  doIncrementalSeek(seekMsIncrement) {
    // console.log('Scheduling incremental seek of %s ms...', seekMsIncrement);
    this.seekRequestId = requestIdleCallback(() => {
      const seekIntermediateMs = Math.min(this.getPositionMs() + seekMsIncrement, this.seekTargetMs);
      core._gme_seek_scaled(this.gmeCtx, seekIntermediateMs);
      if (seekIntermediateMs < this.seekTargetMs) {
        this.doIncrementalSeek(seekMsIncrement);
      } else {
        // console.log('Done Seeking');
        core._gme_set_tempo(this.gmeCtx, this.tempo);
        this.seekTargetMs = null;
        this.seekRequestId = null;
      }
    });
  }

  seekMs(positionMs) {
    super.seekMs(positionMs);
    if (this.gmeCtx) {
      if (TIMESLICED_SEEK_MS_MAP[this.currentFileExt]) {
        cancelIdleCallback(this.seekRequestId);
        this.seekTargetMs = positionMs;
        const seekMsIncrement = TIMESLICED_SEEK_MS_MAP[this.currentFileExt];
        if (positionMs < this.getPositionMs()) {
          // reset to position 0 if seeking backward
          core._gme_seek_scaled(this.gmeCtx, 0);
        }
        core._gme_set_tempo(this.gmeCtx, 2);
        this.doIncrementalSeek(seekMsIncrement);
      } else {
        this.muteAudioDuringCall(this.audioNode, () =>
          core._gme_seek_scaled(this.gmeCtx, positionMs));
      }
    }
  }

  stop() {
    super.stop();
    if (this.gmeCtx) core._gme_delete(this.gmeCtx);
    this.gmeCtx = null;
    console.debug('GMEPlayer.stop()');
  }

}
