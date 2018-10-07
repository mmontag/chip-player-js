const chroma = require('chroma-js');

const MODE_LINEAR = 0;
const MODE_LOG = 1;
const MODE_CONSTANT_Q = 2;

const colorMap = new chroma.scale([
  '#000000',
  '#0000a0',
  '#6000a0',
  '#962761',
  '#dd1440',
  '#f0b000',
  '#ffffa0',
  '#ffffff',
]).domain([0, 255]).mode('lch');

export default class Spectrogram {
  constructor(chipCore, audioCtx, freqCanvasEl, specCanvasEl, minDb = -90, maxDb = -30) {
    this.updateFrame = this.updateFrame.bind(this);
    this.setPaused = this.setPaused.bind(this);

    this.paused = false;
    this.lastCalledTime = 0;
    this.fps = 0;
    this.mode = MODE_LINEAR;

    this.analyserNode = audioCtx.createAnalyser();
    this.analyserNode.minDecibels = minDb;
    this.analyserNode.maxDecibels = maxDb;
    this.analyserNode.smoothingTimeConstant = 0.0;
    this.analyserNode.fftSize = 2048;

    this.frequencyData = new Uint8Array(this.analyserNode.frequencyBinCount);

    this.freqCanvas = freqCanvasEl;
    this.specCanvas = specCanvasEl;
    this.tempCanvas = document.createElement('canvas');
    this.tempCanvas.width = this.specCanvas.width;
    this.tempCanvas.height = this.specCanvas.height;

    this.freqCtx = this.freqCanvas.getContext('2d');
    this.specCtx = this.specCanvas.getContext('2d');
    this.tempCtx = this.tempCanvas.getContext('2d');

    this.updateFrame();
  }

  setPaused(paused) {
    this.paused = paused;
  }

  setMode(mode) {
    this.mode = mode;
  }

  setFFTSize(size) {
    this.analyserNode.fftSize = size;
  }

  updateFrame() {
    if (!this.paused) {
      this.analyserNode.getByteFrequencyData(this.frequencyData);
      this.drawFrequency(this.frequencyData);

      if (!this.lastCalledTime) {
        this.lastCalledTime = new Date().getTime();
        this.fps = 0;
      } else {
        const delta = (new Date().getTime() - this.lastCalledTime) / 1000;
        this.lastCalledTime = new Date().getTime();
        this.fps = Math.round(1 / delta);
      }
    }

    requestAnimationFrame(this.updateFrame);
  }

  drawFrequency(data) {
    this.freqCtx.beginPath();
    this.freqCtx.fillStyle = 'black';
    this.freqCtx.rect(0, 0, this.freqCanvas.width, this.freqCanvas.height);
    this.freqCtx.fill();

    const fqHeight = this.freqCanvas.height;
    const canvasWidth = this.freqCanvas.width;
    const divider = (256.0 / fqHeight);
    this.tempCtx.drawImage(this.specCanvas, 0, 0, this.specCanvas.width, this.specCanvas.height);
    const frameHeight = 3;

    if (this.mode === MODE_LINEAR) {
      for (let i = 0; i < data.length && i < canvasWidth; ++i) {
        this.freqCtx.fillStyle = colorMap(data[i]).hex();
        this.freqCtx.fillRect(i, fqHeight - data[i] / divider, 1, data[i] / divider);

        this.specCtx.fillStyle = colorMap(data[i]).hex();
        this.specCtx.fillRect(i, 0, 1, frameHeight);
      }
    } else if (this.mode === MODE_LOG) {
      const logmax = Math.log(data.length);
      for (let i = 0; i < data.length && i < canvasWidth; i++) {
        const x = Math.floor((Math.log(i) / logmax) * canvasWidth);
        const binWidth = Math.floor((Math.log(i + 1) / logmax) * canvasWidth - x);
        this.freqCtx.fillStyle = colorMap(data[i]).hex();
        this.freqCtx.fillRect(x, fqHeight - data[i] / divider, binWidth, data[i] / divider);
        this.specCtx.fillStyle = colorMap(data[i]).hex();
        this.specCtx.fillRect(x, 0, binWidth, frameHeight);
        // i += binWidth;
      }
    }

    // set translate on the canvas
    this.specCtx.translate(0, frameHeight);
    // draw the copied image
    this.specCtx.drawImage(this.tempCanvas,
      0, 0, this.specCanvas.width, this.specCanvas.height,
      0, 0, this.specCanvas.width, this.specCanvas.height);

    // reset the transformation matrix
    this.specCtx.setTransform(1, 0, 0, 1, 0, 0);
  }
}
