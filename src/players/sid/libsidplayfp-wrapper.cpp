/**
 * A thin libsidplayfp wrapper for Chip Player JS.
 */
#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/sidbuilder.h>
#include <builders/resid-builder/resid.h>

#include <vector>
#include <emscripten.h>

sidplayfp *engine = nullptr;
SidTune *currentTune = nullptr;
ReSIDBuilder *builder = nullptr;
SidConfig cfg;
std::vector<int16_t> pcmBuf;

const uint8_t KERNAL[] = {
#include "./kernal.inc"
};

const uint8_t BASIC[] = {
#include "./basic.inc"
};

const uint8_t CHARGEN[] = {
#include "./chargen.inc"
};

extern "C" {

EMSCRIPTEN_KEEPALIVE
int sid_init(int sample_rate) {
  engine = new sidplayfp();
  if (!engine) {
    fprintf(stderr, "Failed to create sidplayfp instance\n");
    return -1;
  }
  engine->setRoms(KERNAL, BASIC, CHARGEN);

  builder = new ReSIDBuilder("WASM-SID");

  if (!builder) {
    fprintf(stderr, "Failed to create ReSIDBuilder instance\n");
    return -1;
  }

  builder->create(engine->info().maxsids());
  builder->bias(0.); // CRITICAL: Value is otherwise uninitialized and leads to corrupted audio output

  if (!builder->getStatus()) {
    fprintf(stderr, "Failed to create SID emulation: %s\n", builder->error());
    return -1;
  }

  cfg.frequency = sample_rate;
  cfg.playback = SidConfig::STEREO;
  cfg.samplingMethod = SidConfig::INTERPOLATE;
  cfg.sidEmulation = builder;
  cfg.fastSampling = true;
  cfg.digiBoost = false;

  if (!engine->config(cfg)) {
    fprintf(stderr, "Failed to configure sidplayfp: %s\n", engine->error());
    return -1;
  }

  return 0;
}

EMSCRIPTEN_KEEPALIVE
int sid_load_data(const uint8_t *data, uint32_t size) {
  delete currentTune;

  double _perf_start = emscripten_get_now();

  printf("Loading SID data of size %u bytes\n", size);
  currentTune = new SidTune(data, size);
  if (!currentTune->getStatus()) {
    char const *err = currentTune->statusString();
    fprintf(stderr, "Failed to parse SID data: %s\n", err ? err : "unknown error");
    return -1;
  }
  printf("Parsed SID data successfully (t=%.3f ms)\n", emscripten_get_now() - _perf_start);
  currentTune->selectSong(0);
  if (!engine->load(currentTune)) {
    char const *err = engine->error();
    fprintf(stderr, "Failed to load SID tune: %s\n", err ? err : "unknown error");
    return -1;
  }

  printf("Loaded SID tune successfully (t=%.3f ms)\n", emscripten_get_now() - _perf_start);
  auto *info = currentTune->getInfo();
  if (!info) {
    fprintf(stderr, "Failed to get SID info\n");
    return -1;
  }
  printf("Loaded SID: %s (t=%.3f ms)\n", info->infoString(0), emscripten_get_now() - _perf_start);

  return 0;
}

EMSCRIPTEN_KEEPALIVE
int sid_render(float *bufferL, float *bufferR, int length) {
  if (!engine) return 0;

  unsigned numChannels = engine->config().playback == SidConfig::MONO ? 1 : 2;
  unsigned numSamples = length * numChannels;

  // Resize the global buffer if needed to accommodate the requested sample count
  if (pcmBuf.size() < numSamples) {
    pcmBuf.resize(numSamples);
  }
  int rendered = engine->play(pcmBuf.data(), numSamples);

  if (numChannels == 1) {
    for (int i = 0; i < length; i++) {
      bufferL[i] = (float) pcmBuf[i] / 32768.0f; // Convert to float in range [-1.0, 1.0]
      bufferR[i] = bufferL[i]; // Duplicate to right channel for mono
    }
  } else {
    for (int i = 0; i < length; i++) {
      bufferL[i] = (float) pcmBuf[i * 2] / 32768.0f; // Convert to float in range [-1.0, 1.0]
      bufferR[i] = (float) pcmBuf[i * 2 + 1] / 32768.0f; // For stereo, duplicate to right channel
    }
  }

  return rendered;
}

EMSCRIPTEN_KEEPALIVE
int sid_set_speed(float ratio) {
  return 0;
}

EMSCRIPTEN_KEEPALIVE
int sid_get_position_ms() {
  if (!engine) return 0;
  return (int) engine->timeMs();
}

EMSCRIPTEN_KEEPALIVE
int sid_get_duration_ms() {
  return 150000; // 2m30s
}

EMSCRIPTEN_KEEPALIVE
int sid_set_position_ms(int positionMs) {

  int currentMs = (int) engine->timeMs();
  int deltaMs = positionMs < currentMs ? positionMs : positionMs - currentMs;
//  int numChannels = engine->config().playback == SidConfig::MONO ? 1 : 2;
  int numSamples = (int) (deltaMs * cfg.frequency / 1000.0);
  numSamples += numSamples % 2;

  printf("positionMs %d, currentMs %d, deltaMs %d, numSamples %d\n",
          positionMs, currentMs, deltaMs, numSamples);
//  engine->mute(0, 0, false);
//  engine->mute(0, 1, false);
//  engine->mute(0, 2, false);

  if (positionMs < currentMs) {
    engine->stop(); // Stop playback to reset internal state
    engine->play(pcmBuf.data(), 16); // ???
    engine->play(nullptr, numSamples);
  } else {
    engine->play(nullptr, numSamples); // Continue playback to advance internal state
  }

//  engine->mute(0, 0, true);
//  engine->mute(0, 1, true);
//  engine->mute(0, 2, true);

  return numSamples;
}

}
