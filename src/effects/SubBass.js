import { CalcCascades, IirFilter } from 'fili';

export default class SubBass {
  constructor(sampleRate) {
    const iirCalculator = new CalcCascades();
    const lowpass1Coeffs = iirCalculator.lowpass({
      order: 5,
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

    const lowpass1 = new IirFilter(lowpass1Coeffs);
    const lowpass2 = new IirFilter(lowpass2Coeffs);
    const highpass = new IirFilter(highpassCoeffs);
    const numOctaves = 1;
    const dcResponseTime = 0.16;
    const envResponseTime = 0.016;

    let sCurr = 0;
    let sPrev = 0;
    let cycleCounter = 0;
    let dcAvg = 0;
    let dcWeight = Math.exp(-1 / (sampleRate * dcResponseTime));
    let envAvg = 0;
    let envWeight = Math.exp(-1 / (sampleRate * envResponseTime));

    function removeDcStep(sample) {
      dcAvg = dcWeight * dcAvg + (1 - dcWeight) * sample;
      return sample - dcAvg;
    }

    function envelopeStep(sample) {
      sample = removeDcStep(sample);
      envAvg = envWeight * envAvg + (1 - envWeight) * Math.abs(sample);
      return envAvg * 1.414; // invert sine RMS
    }

    this.process = (sample) => {
      const sBass = lowpass1.singleStep(sample);
      const env = envelopeStep(sBass);

      sPrev = sCurr;
      sCurr = sBass;

      // detect zero crossing (low to high)
      if (sPrev < 0 && sCurr >= 0) {
        cycleCounter = (cycleCounter + 1) % Math.pow(2, numOctaves);
      }

      let sOctaves = 0;
      for (let i = 0; i < numOctaves; i++) {
        const bit = cycleCounter & (1 << i);
        sOctaves += bit ? 1 : -1;
      }

      const sSub = highpass.singleStep(lowpass2.singleStep(sOctaves));
      return sSub * env;
    }
  }
}