const chroma = require('chroma-js');

const MODE_LINEAR = 0;
const MODE_LOG = 1;
const MODE_CONSTANT_Q = 2;

const WEIGHTING_NONE = 0;
const WEIGHTING_A = 1;
const colorMap = new chroma.scale([
  '#000000',
  '#0000a0',
  '#6000a0',
  '#962761',
  '#dd1440',
  '#f0b000',
  '#ffffa0',
  '#ffffff',
]).domain([0, 255]);
const _debug = window.location.search.indexOf('debug=true') !== -1;
let _aWeightingLUT;
let _calcTime = 0;
let _totalTime = 0;
let _timeCount = 0;
let _lastTime = 0;

function _getAWeighting(f) {
  var f2 = f*f;
  return 1.5 * 1.2588966 * 148840000 * f2*f2 /
    ((f2 + 424.36) * Math.sqrt((f2 + 11599.29) * (f2 + 544496.41)) * (f2 + 148840000));
}

export default class Spectrogram {
  constructor(chipCore, audioCtx, sourceNode, freqCanvas, specCanvas, pianoKeysImage, minDb = -90, maxDb = -30) {
    this.updateFrame = this.updateFrame.bind(this);
    this.setPaused = this.setPaused.bind(this);

    // Constant Q setup
    this.lib = chipCore;
    const db = 32;
    const supersample = 0;
    const cqtBins = freqCanvas.width;
    //                MIDI note  16 ==   20.60 hz
    // Piano key  1 = MIDI note  21 ==   27.50 hz
    // Piano key 88 = MIDI note 108 == 4186.01 hz
    //                MIDI note 127 == 12543.8 hz
    const fMin = 25.95;
    const fMax = 4504.0;
    const cqtSize = this.lib._cqt_init(audioCtx.sampleRate, cqtBins, db, fMin, fMax, supersample);
    if (!cqtSize) {
      console.error('Error initializing constant Q transform. Constant Q will be disabled.');
    } else {
      this.cqtFreqs = Array(cqtBins).fill().map((_, i) => this.lib._cqt_bin_to_freq(i));
      _aWeightingLUT = this.cqtFreqs.map(f => 0.5 + 0.5 * _getAWeighting(f));
    }
    this.cqtSize = cqtSize;
    this.dataPtr = this.lib._malloc(cqtSize * 4);

    this.paused = true;
    this.mode = MODE_LINEAR;
    this.weighting = WEIGHTING_NONE;

    this.analyserNode = audioCtx.createAnalyser();
    sourceNode.connect(this.analyserNode);

    this.analyserNode.minDecibels = minDb;
    this.analyserNode.maxDecibels = maxDb;
    this.analyserNode.smoothingTimeConstant = 0.0;
    this.analyserNode.fftSize = this.fftSize = 2048;

    this.byteFrequencyData = new Uint8Array(this.analyserNode.frequencyBinCount);

    this.freqCanvas = freqCanvas;
    this.specCanvas = specCanvas;
    this.tempCanvas = document.createElement('canvas');
    this.tempCanvas.width = this.specCanvas.width;
    this.tempCanvas.height = this.specCanvas.height;

    this.freqCtx = this.freqCanvas.getContext('2d', {alpha: false});
    this.specCtx = this.specCanvas.getContext('2d', {alpha: false});
    this.tempCtx = this.tempCanvas.getContext('2d', {alpha: false});

    this.pianoKeysImage = pianoKeysImage;

    this.updateFrame();
  }

  setPaused(paused) {
    this.paused = paused;
  }

  setMode(mode) {
    this.mode = mode;
    if (mode === MODE_CONSTANT_Q) {
      this.analyserNode.fftSize = this.cqtSize || 2048;
    } else {
      this.analyserNode.fftSize = this.fftSize;
    }
  }

  setFFTSize(size) {
    this.fftSize = size;
    this.analyserNode.fftSize = size;
  }

  setWeighting(mode) {
    this.weighting = mode;
  }

  updateFrame() {
    requestAnimationFrame(this.updateFrame);
    if (this.paused) return;

    const fqHeight = this.freqCanvas.height;
    const canvasWidth = this.freqCanvas.width;
    const hCoeff = fqHeight / 256.0;
    const specSpeed = 2;
    const data = this.byteFrequencyData;
    const analyserNode = this.analyserNode;
    const freqCtx = this.freqCtx;
    //const specCtx = this.specCtx;
    const tempCtx = this.tempCtx;
    freqCtx.fillStyle = 'black';
    freqCtx.fillRect(0, 0, this.freqCanvas.width, this.freqCanvas.height);
    tempCtx.fillStyle = '#000033';
    tempCtx.fillRect(0, 0, this.tempCanvas.width, specSpeed);
    const _start = performance.now();
    const dataHeap = new Float32Array(this.lib.HEAPF32.buffer, this.dataPtr, this.cqtSize);
    const bins = this.fftSize / 2;

    if (this.mode === MODE_LINEAR) {
      analyserNode.getByteFrequencyData(data);
      for (let x = 0; x < bins && x < canvasWidth; ++x) {
        const style = colorMap(data[x]).hex();
        const h =     data[x] * hCoeff | 0;
        freqCtx.fillStyle = style;
        freqCtx.fillRect(x, fqHeight - h, 1, h);
        tempCtx.fillStyle = style;
        tempCtx.fillRect(x, 0, 1, specSpeed);
      }
    } else if (this.mode === MODE_LOG) {
      analyserNode.getByteFrequencyData(data);
      const logmax = Math.log(bins);
      for (let i = 0; i < bins; i++) {
        const x =        (Math.log(i + 1) / logmax) * canvasWidth | 0;
        const binWidth = (Math.log(i + 2) / logmax) * canvasWidth - x | 0;
        const h =        (data[i] * hCoeff) | 0;
        const style =    colorMap(data[i] || 0).hex();
        freqCtx.fillStyle = style;
        freqCtx.fillRect(x, fqHeight - h, binWidth, h);
        tempCtx.fillStyle = style;
        tempCtx.fillRect(x, 0, binWidth, specSpeed);
      }
    } else if (this.mode === MODE_CONSTANT_Q) {
      analyserNode.getFloatTimeDomainData(dataHeap);
      if (!dataHeap.every(n => n === 0)) {
        this.lib._cqt_calc(this.dataPtr, this.dataPtr);
        this.lib._cqt_render_line(this.dataPtr);
        // copy output to canvas
        for (let x = 0; x < canvasWidth; x++) {
          const weighting = this.weighting === WEIGHTING_A ? _aWeightingLUT[x] : 1;
          const val = 255 * weighting * dataHeap[x] | 0; //this.lib.getValue(this.cqtOutput + x * 4, 'float') | 0;
          const h = val * hCoeff | 0;
          const style = colorMap(val).hex();
          freqCtx.fillStyle = style;
          freqCtx.fillRect(x, fqHeight - h, 1, h);
          tempCtx.fillStyle = style;
          tempCtx.fillRect(x, 0, 1, specSpeed);
        }
      }
    }

    const _middle = performance.now();

    // tempCtx.drawImage(this.specCanvas, 0, 0);
    // translate the transformation matrix. subsequent draws happen in this frame
    tempCtx.translate(0, specSpeed);
    // draw the copied image
    tempCtx.drawImage(this.tempCanvas, 0, 0);
    // reset the transformation matrix
    tempCtx.setTransform(1, 0, 0, 1, 0, 0);

    this.specCtx.drawImage(this.tempCanvas, 0, 0);
    // Disabled because this is rendered as plain HTML IMG element
    // if (this.mode === MODE_CONSTANT_Q) {
    //   this.specCtx.drawImage(this.pianoKeysImage, 0, 0);
    // }

    const _end = performance.now();

    if (_debug) {
      _calcTime += _middle - _start;
      _totalTime += _end - _start;
      _timeCount++;
      if (_timeCount >= 200) {
        console.log(
          '[Viz] %s ms analysis, %s ms total (%s fps) (%s% utilization)',
          (_calcTime / _timeCount).toFixed(2),
          (_totalTime / _timeCount).toFixed(2),
          (1000 * _timeCount / (_start - _lastTime)).toFixed(1),
          (100 * _totalTime / (_end - _lastTime)).toFixed(1),
        );
        _calcTime = 0;
        _timeCount = 0;
        _totalTime = 0;
        _lastTime = _start;
      }
    }
  }
}

// getFloatTimeDomainData polyfill for Safari
if (global.AnalyserNode && !global.AnalyserNode.prototype.getFloatTimeDomainData) {
  var uint8 = new Uint8Array(32768);
  global.AnalyserNode.prototype.getFloatTimeDomainData = function(array) {
    this.getByteTimeDomainData(uint8);
    for (var i = 0, imax = array.length; i < imax; i++) {
      array[i] = (uint8[i] - 128) * 0.0078125;
    }
  };
}
