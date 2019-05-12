import Fili from 'fili';

export default class SubBass {
  constructor(sampleRate) {
    const iirCalculator = new Fili.CalcCascades();
    const lowpass1Coeffs = iirCalculator.lowpass({
      order: 3,
      characteristic: 'butterworth',
      Fs: sampleRate,
      Fc: 200,
      gain: 0,
    });

    const lowpass2Coeffs = iirCalculator.lowpass({
      order: 3,
      characteristic: 'butterworth',
      Fs: sampleRate,
      Fc: 120,
      gain: 0,
    });

    const highpassCoeffs = iirCalculator.highpass({
      order: 3,
      characteristic: 'butterworth',
      Fs: sampleRate,
      Fc: 25,
      gain: 0,
    });

    const lowpass1 = new Fili.IirFilter(lowpass1Coeffs);
    const lowpass2 = new Fili.IirFilter(lowpass2Coeffs);
    const highpass = new Fili.IirFilter(highpassCoeffs);

    // Test signal
    // let s = 0;
    // const TWO_PI = 2 * Math.PI;
    // const sr = 48000;
    // const f0 = 600 * TWO_PI;
    // const fd = 300 * TWO_PI;
    // const fm = 0.5 * TWO_PI;

    let n0 = 0;
    let n1 = 0;
    let kloop = 0;
    let adown1 = 0;
    let adown2 = 0;

    const dcResponseTime = 0.2;
    let dcAvg = 0;
    let dcWeight = Math.exp(-1 / (sampleRate * dcResponseTime));
    function removeDC(sample) {
      dcAvg = dcWeight * dcAvg + (1 - dcWeight) * sample;
      return sample - dcAvg;
    }

    const envResponseTime = 0.016;
    let envAvg = 0;
    let envWeight = Math.exp(-1 / (sampleRate * envResponseTime));
    function envelope(sample) {
      sample = removeDC(sample);
      envAvg = envWeight * envAvg + (1 - envWeight) * Math.abs(sample);
      return envAvg * 1.414; // invert sine RMS
    }

    this.process = (input) => {
      // Test signal
      // const t = s++ / sr;
      // const sin1 = Math.sin(f0 * t + fd / fm * Math.sin(fm * t));
      // const sin2 = Math.sin(f0 * t + fd / 1.1 * Math.sin(1.1 * t));
      // const aTest = sin1 + sin2;

      const abass = lowpass1.singleStep(input);
      const env = envelope(abass);

      n1 = n0;
      n0 = abass;

      // detect zero crossing (low to high)
      if (n1 < 0 && n0 >= 0) {
        kloop = (kloop + 1) % 4;
      }

      switch (kloop) {
        case 0:
          adown1 = 1;
          adown2 = 1;
          break;
        case 1:
          adown1 = -1;
          adown2 = 1;
          break;
        case 2:
          adown1 = 1;
          adown2 = -1;
          break;
        case 3:
          adown1 = -1;
          adown2 = -1;
          break;
        default:
      }

      const asub = highpass.singleStep(lowpass2.singleStep(adown1));
      const out = asub * env;

      return out;
    }
  }
}