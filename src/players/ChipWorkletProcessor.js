class ChipWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process(inputList, outputList, parameters) {
    return true;
  }
}

registerProcessor("chip-worklet-processor", ChipWorkletProcessor);
