import Player from './Player';
import {
  BasicMIDI,
  midiMessageTypes,
  SoundBankLoader,
  SpessaSynthProcessor,
  SpessaSynthSequencer
} from 'spessasynth_core';
import { SOUNDFONT_MOUNTPOINT } from '../config';
import { GM_DRUM_KITS, GM_INSTRUMENTS } from '../gm-patch-map';

export default class MIDIPlayer2 extends Player {
  constructor(core, sampleRate, bufferSize, debug) {
    super(core, sampleRate, bufferSize, debug);

    // Initialize Soundfont filesystem
    core.FS.mkdir(SOUNDFONT_MOUNTPOINT);
    core.FS.mount(core.FS.filesystems.IDBFS, {}, SOUNDFONT_MOUNTPOINT);

    this.playerKey = 'midi2';
    this.name = 'MIDI Player 2';
    this.fileExtensions = ['mid', 'midi', 'smf'];

    this.processor = new SpessaSynthProcessor(sampleRate, { enableEffects: false });
    this.sequencer = new SpessaSynthSequencer(this.processor);
    this.sequencer.onEventCall = (event) => {
      if (event.type === 'pause' && event.data.isFinished === true) {
        console.log('MIDI playback finished');
        this.stop();
      }
    };
  }

  handleFileSystemReady() {
    super.handleFileSystemReady();

    {
      const sf2Path = "/soundfonts/Yamaha-Grand-Lite-v1.1.sf2";
      const sf2Data = this.core.FS.readFile(sf2Path, { encoding: 'binary' });
      console.log('sf2Data', sf2Data);
      this.processor.soundBankManager.addSoundBank(
        SoundBankLoader.fromArrayBuffer(sf2Data.buffer),
        "piano",
      );
    }
    {
      const sf2Path = `${SOUNDFONT_MOUNTPOINT}/generaluser2.sf2`;
      const sf2Data = this.core.FS.readFile(sf2Path, { encoding: 'binary' });
      console.log('sf2Data', sf2Data);
      this.processor.soundBankManager.addSoundBank(
        SoundBankLoader.fromArrayBuffer(sf2Data.buffer),
        "main",
      );
    }
  }

  loadData(data, filepath, persistedSettings) {
    let t = performance.now();
    this.filepathMeta = this.metadataFromFilepath(filepath);

    this.resolveParamValues(persistedSettings);
    this.setTempo(persistedSettings.tempo || 1);

      console.log('1', performance.now() - t);
      t = performance.now();

    const parsedMIDI = BasicMIDI.fromArrayBuffer(data.buffer);

      console.log('2', performance.now() - t);
      t = performance.now();

    this.summarizeMidiEvents(parsedMIDI);

      console.log('3', performance.now() - t);
      t = performance.now();

    this.sequencer.loadNewSongList([parsedMIDI]);

      console.log('4', performance.now() - t);
      t = performance.now();

    this.sequencer.play();

    console.log('5', performance.now() - t);
    t = performance.now();

    console.log('isMultiport', this.sequencer.midiData.isMultiPort);
    console.log('midiChannels', this.processor.midiChannels);
    console.log('portChannelOffsetMap', this.sequencer.midiData.portChannelOffsetMap);

    this.resume();
    this.emit('playerStateUpdate', {
      ...this.getBasePlayerState(),
      isStopped: false,
    });
  }

  processAudioInner(channels) {
    let i, ch;

    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        const channel = channels[ch];
        for (i = 0; i < channel.length; i++) {
          channel[i] = 0;
        }
      }
      return;
    }

    const chunkSize = 128;
    const chunks = this.bufferSize / chunkSize;
    for (i = 0; i < chunks; i++) {
      this.sequencer.processTick();
      this.processor.renderAudio(channels, [], [], i * chunkSize, chunkSize);
    }
  }

  metadataFromFilepath(filepath) {
    filepath = decodeURIComponent(filepath); // unescape, %25 -> %
    const parts = filepath.split('/');
    const len = parts.length;
    const meta = {};
    // HACK: MIDI metadata is guessed from filepath
    // based on the directory structure of Chip Player catalog.
    // Ideally, this data should be embedded in the MIDI files.
    if (parts.length >= 3) {
      meta.formatted = {
        title: `${parts[1]} - ${parts[len - 1]}`,
        subtitle: parts[0],
      };
    } else if (parts.length === 2) {
      meta.formatted = {
        title: parts[1],
        subtitle: parts[0],
      }
    } else {
      meta.formatted = {
        title: parts[0],
        subtitle: 'MIDI',
      }
    }
    return meta;
  }

  stop() {
    this.suspend();
    this.emit('playerStateUpdate', { isStopped: true });
    this.sequencer.loadNewSongList([]);
  }

  isPlaying() {
    return !this.stopped && !this.paused;
  }

  setVoiceMask(voiceMask) {
    voiceMask.forEach((isEnabled, i) => {
      const idx = this.activeChannels[i];
      const ch = this.processor.midiChannels[idx];
      ch.muteChannel(!isEnabled);
    });
  }
  getNumVoices() {
    return this.activeChannels.length;
  }

  getVoiceName(index) {
    const ch = this.activeChannels[index];
    const pgm = this.channelProgramNums[ch];
    return ch === 9 ? (GM_DRUM_KITS[pgm] || GM_DRUM_KITS[0]) : GM_INSTRUMENTS[pgm]
  }

  getVoiceMask() {
    return this.activeChannels.map(ch => this.channelMask[ch]);
  }

  seekMs(positionMs) {
    this.sequencer.currentTime = positionMs / 1000;
  }

  getPositionMs() {
    return this.sequencer.currentTime * 1000;
  }

  getDurationMs() {
    return this.sequencer.duration * 1000;
  }

  getMetadata() {
    return this.filepathMeta;
  }

  getTempo() {
    return this.sequencer.playbackRate;
  }

  setTempo(tempo) {
    this.sequencer.playbackRate = tempo;
  }

  /**
   * Summarizes MIDI events and updates channel usage and metadata.
   * @param {import('spessasynth_core').BasicMIDI} basicMidi
   */
  summarizeMidiEvents(basicMidi) {
    // TODO: clean this up
    this.channelsInUse = [];
    this.channelMask = [];
    this.channelProgramNums = [];
    this.activeChannels = [];
    this.textInfo = [];

    for (let i = 0; i < 16; i++) {
      this.channelsInUse[i] = 0;
      this.channelProgramNums[i] = 0;
      this.channelMask[i] = true;
    }

    // console.log('basicMidi', basicMidi);

    basicMidi.iterate((event) => {
      const eventType = event.statusByte & 0xF0;
      const channel = event.statusByte & 0x0F;

      switch (eventType) {
        case midiMessageTypes.noteOn:
          // console.log('noteON', channel, event);
          this.channelsInUse[channel] = 1;
          break;
        case midiMessageTypes.programChange:
          // console.log('programChane', channel, event);
          if (!this.channelProgramNums[channel]) this.channelProgramNums[channel] = event.data[0];
          break;
        // case MIDIEvents.EVENT_META_TEXT:
        // case MIDIEvents.EVENT_META_COPYRIGHT_NOTICE:
        // case MIDIEvents.EVENT_META_TRACK_NAME:
        // // case MIDIEvents.EVENT_META_INSTRUMENT_NAME:
        // case MIDIEvents.EVENT_META_LYRICS:
        // case MIDIEvents.EVENT_META_MARKER:
        // case MIDIEvents.EVENT_META_CUE_POINT:
        //   const text = event.data.map(c => String.fromCharCode(c)).join('').trim();
        //   if (text && !text.match(/nstd/i))
        //     this.textInfo.push(`${META_LABELS[event.subtype]}: ${text}`);
        //   break;
        default:
          break;
      }
    });

    for (let i = 0; i < 16; i++) {
      if (this.channelsInUse[i]) this.activeChannels.push(i);
    }
  };

}
