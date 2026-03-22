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
#include <bitset>
#include <emscripten.h>
#include <emscripten/bind.h>

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
const char* sid_get_song_md5() {
  if (!currentTune) return nullptr;
  // char* is owned by SIDTuneBase
  return currentTune->createMD5();
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
void sid_set_speed(float ratio) {
  engine->setTempo(ratio);
}

EMSCRIPTEN_KEEPALIVE
int sid_get_position_ms() {
  if (!engine) return 0;
  return (int) engine->timeMs();
}

EMSCRIPTEN_KEEPALIVE
void sid_set_position_ms(int positionMs) {
  engine->seek(positionMs);
}

EMSCRIPTEN_KEEPALIVE
int sid_get_duration_ms() {
  // Duration requires the songlengths.md5 database.
  // Currently implemented in the host layer.
  return 150000; // 2m30s
}

EMSCRIPTEN_KEEPALIVE
void sid_set_voice_mask(unsigned mask) {
  if (!engine || !currentTune) return;
  auto *info = currentTune->getInfo();
  if (!info) return;

  std::bitset<9> bitmask{mask};
  for (int i = 0; i < info->sidChips(); i++) {
    for (int j = 0; j < 3; j++) {
      engine->mute(i, j, bitmask[i*3 + j]);
    }
  }
}

EMSCRIPTEN_KEEPALIVE
int sid_get_num_subtunes() {
  return (int) currentTune->getInfo()->songs();
}

EMSCRIPTEN_KEEPALIVE
int sid_get_subtune() {
  return (int) currentTune->getInfo()->currentSong() - 1;
}

EMSCRIPTEN_KEEPALIVE
void sid_set_subtune(int subtune) {
  engine->stop();
  currentTune->selectSong(subtune + 1);
}

EMSCRIPTEN_KEEPALIVE
void sid_stop() {
  engine->stop();
}

}

struct VoiceGroup {
  std::string group_name;
  std::vector<std::string> voice_names;
};

EMSCRIPTEN_KEEPALIVE
std::vector<VoiceGroup> sid_get_voice_groups() {
  std::vector<VoiceGroup> voice_groups;
  for (int i = 0; i < currentTune->getInfo()->sidChips(); i++) {
    std::string group_name = "MOS 6581";

    if (currentTune->getInfo()->sidModel(i) == SidTuneInfo::SIDMODEL_8580) {
      group_name = "MOS 8580";
    }

    static std::vector<std::string> voice_names = {"Osc 1", "Osc 2", "Osc 3"};
    voice_groups.push_back({.group_name=group_name, .voice_names=voice_names});
  }

  return voice_groups;
}

EMSCRIPTEN_BINDINGS(module) {
  emscripten::value_object<VoiceGroup>("VoiceGroup")
    .field("groupName", &VoiceGroup::group_name)
    .field("voiceNames", &VoiceGroup::voice_names)
    ;
  emscripten::register_vector<std::string>("vector<string>");
  emscripten::register_vector<VoiceGroup>("vector<VoiceGroup>");
  emscripten::function("sidGetVoiceGroups", &sid_get_voice_groups);
}
