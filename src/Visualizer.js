const chroma = require('chroma-js');

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

export default class Visualizer {
  constructor(audioCtx, freqCanvasEl, specCanvasEl, minDb = -90, maxDb = -30) {
    this.updateFrame = this.updateFrame.bind(this);

    this.lastCalledTime = 0;
    this.fps = 0;

    this.analyserNode = audioCtx.createAnalyser();
    this.analyserNode.minDecibels = minDb;
    this.analyserNode.maxDecibels = maxDb;
    this.analyserNode.smoothingTimeConstant = 0.0;

    this.frequencyData = new Uint8Array(this.analyserNode.frequencyBinCount);
    this.freqCanvasEl = freqCanvasEl;
    this.specCanvasEl = specCanvasEl;

    this.ctxFq = this.freqCanvasEl.getContext('2d');
    this.ctxSp = this.specCanvasEl.getContext('2d');

    this.tempCanvas = document.createElement('canvas');
    this.tempCanvas.width = this.specCanvasEl.width;
    this.tempCanvas.height = this.specCanvasEl.height;
    this.tempCtx = this.tempCanvas.getContext('2d');
    this.updateFrame();
  }

  updateFrame() {
    this.analyserNode.getByteFrequencyData(this.frequencyData);
    this.drawFrequency(this.frequencyData);
    this.drawSpectrogram(this.frequencyData);

    if (!this.lastCalledTime) {
      this.lastCalledTime = new Date().getTime();
      this.fps = 0;
    } else {
      const delta = (new Date().getTime() - this.lastCalledTime) / 1000;
      this.lastCalledTime = new Date().getTime();
      this.fps = Math.round(1 / delta);
    }

    requestAnimationFrame(this.updateFrame);
  }

  drawFrequency(data) {
    let value;

    this.ctxFq.beginPath();
    this.ctxFq.fillStyle = 'black';
    this.ctxFq.rect(0, 0, this.freqCanvasEl.width, this.freqCanvasEl.height);
    this.ctxFq.fill();
    this.ctxFq.beginPath();
    this.ctxFq.moveTo(0, -999);

    const fqHeight = this.freqCanvasEl.height;
    const divider = (256.0 / fqHeight);
    for (let i = 0; i < data.length; ++i) {
      value = fqHeight - data[i] / divider;// - 128 + freqCanvasEl.height / 2;
      this.ctxFq.lineTo(i, value);
      this.ctxFq.fillStyle = colorMap(data[i]).hex();
      this.ctxFq.fillRect(i, fqHeight - data[i] / divider, 1, data[i] / divider);
    }
    this.ctxFq.moveTo(0, 999);
    this.ctxFq.closePath();
  }

  drawSpectrogram(array) {
    // copy the current canvas onto the temp canvas
    this.tempCtx.drawImage(this.specCanvasEl, 0, 0, this.specCanvasEl.width, this.specCanvasEl.height);
    const frameHeight = 3;
    // iterate over the elements from the array
    for (let i = 0; i < array.length; i++) {
      // draw each pixel with the specific color
      this.ctxSp.fillStyle = colorMap(array[i]).hex();
      // draw the line at the right side of the canvas
      this.ctxSp.fillRect(i, 0, 1, frameHeight);
    }

    // set translate on the canvas
    this.ctxSp.translate(0, frameHeight);
    // draw the copied image
    this.ctxSp.drawImage(this.tempCanvas,
      0, 0, this.specCanvasEl.width, this.specCanvasEl.height,
      0, 0, this.specCanvasEl.width, this.specCanvasEl.height);

    // reset the transformation matrix
    this.ctxSp.setTransform(1, 0, 0, 1, 0, 0);
  }
}
