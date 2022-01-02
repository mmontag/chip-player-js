import ChipCore from '../chip-core';
import Sequencer from '../Sequencer';
import * as Comlink from 'comlink';
import GMEPlayer from './GMEPlayer';
import XMPPlayer from './XMPPlayer';
import N64Player from './N64Player';
import V2MPlayer from './V2MPlayer';
import MDXPlayer from './MDXPlayer';
import MIDIPlayer from './MIDIPlayer';

class ChipWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();

    this.player = null;
    this.players = {};
    const fetchProxy = Comlink.wrap(this.port).fetch;                                    // COMLINK
    global.fetch = fetchProxy;

    this.port.onmessage = (e) => {
      const type = e.data.type;
      switch (type) {
        case 'SEQUENCER_RPC': {
          const [method, ...args] = e.data.payload;
          if (this.sequencer) {
            this.sequencer[method].apply(this, args);
          }
          break;
        }
        case 'WASM_DATA':
          console.log('Module received from main thread', e);
          const wasmData = e.data.payload;
          this.chipCore = new ChipCore({
            instantiateWasm: (imports, successCallback) => {
              WebAssembly.instantiate(wasmData, imports).then(instance => {
                successCallback(instance);
              });
            },
            onRuntimeInitialized: () => {
              console.log('Runtime initialized!', this.chipCore);
              this.players = {
                gmePlayer: new GMEPlayer(this.chipCore, sampleRate),
                xmpPlayer: new XMPPlayer(this.chipCore, sampleRate),
                n64Player: new N64Player(this.chipCore, sampleRate),
                v2mPlayer: new V2MPlayer(this.chipCore, sampleRate),
                mdxPlayer: new MDXPlayer(this.chipCore, sampleRate),
                midiPlayer: new MIDIPlayer(this.chipCore, sampleRate),
              };

              // Sequencer inside AudioWorklet
              // this.sequencer = new Sequencer(players, fetchProxy);
              // Comlink.expose({ players: players, sequencer: this.sequencer }, this.port);                  // COMLINK

              // Sequencer outside AudioWorklet
              // Comlink.expose({ getPlayerForExtension: (ext) => {
              //   return players.find(player => player.canPlay(ext));
              // }}, this.port);                  // COMLINK
              // Comlink.expose({ players: players.map(player => Comlink.proxy(player)) });
              Comlink.expose(this.players, this.port);

            },
          });
          break;
        default:
          // console.log('Unrecognized postMessage:', e);
      }
    }
  }

  process(inputList, outputList, parameters) {
    const output = outputList[0];

    const players = Object.values(this.players);
    // if (this.sequencer) {
    //   this.sequencer.process(output);
    if (players.length) {
      players.forEach(player => {
        if (!player.paused) player.process(output);
      });
    } else {
      for (let ch = 0; ch < output.length; ch++) {
        for (let i = 0; i < output[ch].length; i++) {
          output[ch][i] = Math.random() * 0.1;
        }
      }
    }
    return true;
  }
}

registerProcessor("chip-worklet-processor", ChipWorkletProcessor);
