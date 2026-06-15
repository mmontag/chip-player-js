//
// Created by Matt Montag on 6/18/23.
// Language: C++14
//
#include <cstring>
#include <vector>
#include <cstdio>
#include <map>
#include <string>

#include "stdtype.h"
#include "emu/SoundEmu.h"
#include "emu/SoundDevs.h"
#include "player/playera.hpp"
#include "player/droplayer.hpp"
#include "player/gymplayer.hpp"
#include "player/s98player.hpp"
#include "player/vgmplayer.hpp"
#include "utils/FileLoader.h"
#include "utils/MemoryLoader.h"
#include "emu/EmuCores.h"

/* C wrapper functions */
typedef struct lvgm_player lvgm_player;

inline PlayerA *real(lvgm_player *player) {
  return reinterpret_cast<PlayerA *>(player);
}

struct VoiceInfo {
  size_t count = 0;
  size_t linkedCount = 0; // Channel count of linked device (e.g. YM2203 OPN linked device = AY8910 with 3 channels)
  std::string type = "Ch"; // Default name prefix
  std::vector<std::string> names = {};
  std::string suffix;
  std::string linkedSuffix;
};

const std::map<int, VoiceInfo> deviceToVoiceInfo = {
  {DEVID_32X_PWM,  {1,  0, "", {"PWM"}}},
  {DEVID_AY8910,   {3,  0}},
  {DEVID_C140,     {24, 0, "PCM"}},
  {DEVID_C219,     {16, 0, "PCM"}},
  {DEVID_C352,     {32, 0, "PCM"}},
  {DEVID_C6280,    {6,  0}},
  {DEVID_ES5503,   {32, 0}},
  {DEVID_ES5506,   {32, 0}},
  {DEVID_GA20,     {4,  0, "PCM"}},
  {DEVID_GB_DMG,   {4,  0, "", {"Pulse 1", "Pulse 2", "Wave",     "Noise"}}},
  {DEVID_K051649,  {5,  0, "Wave"}},
  {DEVID_K053260,  {4,  0, "PCM"}},
  {DEVID_K054539,  {8,  0, "PCM"}},
  {DEVID_MIKEY,    {4,  0, "Wave"}},
  {DEVID_NES_APU,  {5,  0, "", {"Pulse 1", "Pulse 2", "Triangle", "Noise",  "DMC"}}},
  {DEVID_MSM6258,  {1,  0, "PCM"}},
  {DEVID_MSM6295,  {4,  0, "PCM"}},
  {DEVID_POKEY,    {4,  0, "PSG"}},
  {DEVID_QSOUND,   {16, 0, "PCM"}},
  {DEVID_RF5C68,   {8,  0, "PCM"}},
  {DEVID_SAA1099,  {6,  0}},
  {DEVID_SCSP,     {32, 0, "FM/PCM"}},
  {DEVID_SEGAPCM,  {16, 0, "PCM"}},
  {DEVID_SN76496,  {4,  0, "", {"Pulse 1", "Pulse 2", "Pulse 3",  "Noise"}}},
  {DEVID_uPD7759,  {1,  0}},
  {DEVID_VBOY_VSU, {6,  0, "", {"Wave 1",  "Wave 2",  "Wave 3",   "Wave 4", "Wave 5", "Noise"}}},
  {DEVID_WSWAN,    {4,  0}},
  {DEVID_X1_010,   {16, 0}},
  // See fmopl.c for mute mask info
  /* OPL/OPL2 chips have 9 channels*/
  /* Mute Special: 5 Rhythm + 1 DELTA-T Channel */
  {DEVID_Y8950,    {15, 0, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6",  "FM 7",  "FM 8",  "FM 9", "FM Drum 1", "FM Drum 2", "FM Drum 3", "FM Drum 4", "FM Drum 5", "PCM"}}},
  {DEVID_YM2151,   {8,  0, "FM"}},
  {DEVID_YM2203,   {3,  3, "", {"FM 1",    "FM 2",    "FM 3",     "PSG 1",  "PSG 2",  "PSG 3"}, " (FM)", " (PSG)"}},
  {DEVID_YM2413,   {14, 0, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "FM 7", "FM 8", "FM 9", "Bass Drum", "Snare Drum", "Tom", "Top Cymbal", "Hi-Hat"}}},
  {DEVID_YM2608,   {13, 3, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "Bass Drum", "Snare Drum", "Cymbal", "Hi-Hat", "Tom", "Rimshot", "PCM", "PSG 1", "PSG 2", "PSG 3"}, " (FM/ADPCM)", " (PSG)"}},
  {DEVID_YM2610,   {13, 3, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "PCM 1", "PCM 2", "PCM 3", "PCM 4", "PCM 5", "PCM 6", "PCM 7", "PSG 1", "PSG 2", "PSG 3"}, " (FM/ADPCM)", " (PSG)"}},
  {DEVID_YM2612,   {7,  0, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "PCM"}}},
  {DEVID_YM3526,   {9,  0, "FM"}},
  {DEVID_YM3812,   {9,  0, "FM"}},
  {DEVID_YMF262,   {18, 0, "FM"}},
  {DEVID_YMF271,   {24, 0, "FM"}},
  {DEVID_YMF278B,  {24, 18, "", {
    "PCM 1", "PCM 2", "PCM 3", "PCM 4", "PCM 5", "PCM 6", "PCM 7", "PCM 8",
    "PCM 9", "PCM 10", "PCM 11", "PCM 12", "PCM 13", "PCM 14", "PCM 15", "PCM 16",
    "PCM 17", "PCM 18", "PCM 19", "PCM 20", "PCM 21", "PCM 22", "PCM 23", "PCM 24",
    "FM 1", "FM 2", "FM 3", "FM 4", "FM 5", "FM 6", "FM 7", "FM 8", "FM 9",
    "FM 10", "FM 11", "FM 12", "FM 13", "FM 14", "FM 15", "FM 16", "FM 17", "FM 18"}, " (PCM)", " (FM)"}},
  {DEVID_YMW258,   {28, 0, "PCM"}},
  {DEVID_YMZ280B,  {8,  0}}};

struct Chip {
  size_t type;
  std::string name;
  // Index of the chip as defined in the current song
  size_t idx;
  size_t linkedIdx;
  size_t voiceCount;
};

// Vector of chips (populated after song load)
std::vector<Chip> chips;

struct DevIdVoiceName {
  size_t id;
  std::string name;
  std::string chipName;
};

// Vector of voice names (populated after song load)
std::vector<DevIdVoiceName> voices;
static char const *meta[8];

// Yamaha OPL4 ROM file path
static std::string g_yrw801_rom_path;

// Enhanced stereo for mono chips
static bool g_enhanced_stereo;

// Get voice name from deviceToVoiceInfo.at(id).names[v], fall back to voiceInfo[id].type if name is empty
std::string getVoiceName(const int id, const int v) {
  const auto& info = deviceToVoiceInfo.at(id);
  if (v < info.names.size() && !info.names[v].empty()) {
    return info.names[v];
  } else {
    return info.type + " " + std::to_string(v + 1);
  }
}

static void configure_enhanced_stereo(PlayerBase* base, bool stereoEnabled) {
  if (!base) return;
  PLR_DEV_OPTS devOpts{};
  UINT32 devOptID;
  UINT8 retVal;

  static constexpr INT16 monoPanPos3[3] = { 0, 0, 0 };
  static constexpr INT16 stereoPanPos3[3] = {-0x80, +0x80, 0};

  static constexpr INT16 monoPanPos4[4] = { 0, 0, 0, 0 };
  static constexpr INT16 stereoPanPos4[4] = {-0x80, +0x80, 0, 0};

  static constexpr INT16 monoPanPos5[5] = { 0, 0, 0, 0, 0 };
  static constexpr INT16 stereoPanPos5[5] = {-0x80, +0x80, 0, 0, 0};

  static constexpr INT16 monoPanPos14[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  static constexpr INT16 stereoPanPos14[14] = {
    -0x100, +0x100, -0x80, +0x80, -0x40, +0x40, -0xC0, +0xC0, 0x00,
    -0x60, +0x60, 0x00, -0xC0, +0xC0};

  const INT16* panPos3 = stereoEnabled ? stereoPanPos3 : monoPanPos3;
  const INT16* panPos4 = stereoEnabled ? stereoPanPos4 : monoPanPos4;
  const INT16* panPos5 = stereoEnabled ? stereoPanPos5 : monoPanPos5;
  const INT16* panPos14 = stereoEnabled ? stereoPanPos14 : monoPanPos14;

  for (int instance = 0; instance < 2; instance++) {
    // 3-channel panning (AY8910)
    for (auto deviceId : { DEVID_AY8910 }) {
      devOptID = PLR_DEV_ID(deviceId, instance);
      retVal = base->GetDeviceOptions(devOptID, devOpts);
      if (!(retVal & 0x80)) {
        memcpy(devOpts.panOpts.chnPan[0], panPos3, sizeof(INT16) * 3);
        base->SetDeviceOptions(devOptID, devOpts);
      }
    }

    // 3-channel panning for SSG channels (linked devices of OPN chips)
    for (auto deviceId : { DEVID_YM2203, DEVID_YM2608, DEVID_YM2610 }) {
      devOptID = PLR_DEV_ID(deviceId, instance);
      retVal = base->GetDeviceOptions(devOptID, devOpts);
      if (!(retVal & 0x80)) { //      v--- linked device
        memcpy(devOpts.panOpts.chnPan[1], panPos3, sizeof(INT16) * 3);
        base->SetDeviceOptions(devOptID, devOpts);
      }
    }

    // 4-channel panning (SN76496 / Sega PSG)
    for (auto deviceId : { DEVID_SN76496 }) {
      devOptID = PLR_DEV_ID(DEVID_SN76496, instance);
      retVal = base->GetDeviceOptions(devOptID, devOpts);
      if (!(retVal & 0x80)) {
        memcpy(devOpts.panOpts.chnPan[0], panPos4, sizeof(INT16) * 4);
        base->SetDeviceOptions(devOptID, devOpts);
      }
    }

    // 5-channel panning (NES APU)
    for (auto deviceId : { DEVID_NES_APU }) {
      devOptID = PLR_DEV_ID(deviceId, instance);
      retVal = base->GetDeviceOptions(devOptID, devOpts);
      if (!(retVal & 0x80)) {
        memcpy(devOpts.panOpts.chnPan[0], panPos5, sizeof(INT16) * 5);
        base->SetDeviceOptions(devOptID, devOpts);
      }
    }

    // 14-channel panning (YM2413)
    for (auto deviceId : {DEVID_YM2413}) {
      devOptID = PLR_DEV_ID(deviceId, instance);
      retVal = base->GetDeviceOptions(devOptID, devOpts);
      if (!(retVal & 0x80)) {
        memcpy(devOpts.panOpts.chnPan[0], panPos14, sizeof(INT16) * 14);
        base->SetDeviceOptions(devOptID, devOpts);
      }
    }
  }
}

std::string getNiceChipName(UINT8 type, const char* devName) {
  switch (type) {
    case DEVID_YM2151:    return "Yamaha YM2151 (OPM)";
    case DEVID_YM2612:    return "Yamaha YM2612 (OPN2)";
    case DEVID_YM2413:    return "Yamaha YM2413 (OPLL/VRC7)";
    case DEVID_YM2203:    return "Yamaha YM2203 (OPN)";
    case DEVID_YM2608:    return "Yamaha YM2608 (OPNA)";
    case DEVID_YM2610:    return "Yamaha YM2610 (OPNB)";
    case DEVID_YMF262:    return "Yamaha YMF262 (OPL3)";
    case DEVID_YM3812:    return "Yamaha YM3812 (OPL2)";
    case DEVID_YM3526:    return "Yamaha YM3526 (OPL)";
    case DEVID_YMF271:    return "Yamaha YMF271 (OPX)";
    case DEVID_YMW258:    return "Yamaha YMW258 (GEW-8/MultiPCM)";
    case DEVID_YMF278B:   return "Yamaha YMF278B (OPL4)";
    case DEVID_Y8950:     return "Yamaha Y8950 (MSX-Audio)";
    case DEVID_AY8910:    return "AY-3-8910 (PSG)";
    case DEVID_SN76496:   return "SN76496 (Sega PSG)";
    case DEVID_NES_APU:   return "Ricoh 2A03 (NES APU)";
    case DEVID_VBOY_VSU:  return "Virtual Boy VSU";
    case DEVID_C6280:     return "Hudson C6280";
    case DEVID_C352:      return "Namco C352";
    case DEVID_C140:      return "Namco C140";
    case DEVID_C219:      return "Namco C219";
    case DEVID_K051649:   return "Konami K051649";
    case DEVID_K053260:   return "Konami K053260";
    case DEVID_K054539:   return "Konami K054539";
    default:              return devName;
  }
}

#ifdef __cplusplus
extern "C" {
#endif

lvgm_player* lvgm_init(UINT32 sample_rate) {
  auto* player = new PlayerA();
  player->SetSampleRate(sample_rate);
  player->SetPlaybackSpeed(1.0);
  player->SetOutputSettings(sample_rate, 2, 32, 2048);
  player->RegisterPlayerEngine(new VGMPlayer());
  player->RegisterPlayerEngine(new S98Player());
  player->RegisterPlayerEngine(new DROPlayer());
  player->RegisterPlayerEngine(new GYMPlayer());
  {
    PlayerA::Config pCfg = player->GetConfiguration();
    pCfg.masterVol = 0x800; // ? no idea. 0x1000 seemed too loud
    pCfg.loopCount = 2;
    pCfg.fadeSmpls = sample_rate * 4;	// fade over 4 seconds
    pCfg.endSilenceSmpls = sample_rate / 2;	// 0.5 seconds of silence at the end
    pCfg.pbSpeed = 1.0;
    player->SetConfiguration(pCfg);
  }

  return reinterpret_cast<lvgm_player *>(player);
}

UINT8 lvgm_load_data(lvgm_player *player, const UINT8 *data, const UINT32 size) {
  PlayerA *playerA = real(player);
  UINT8 retVal;
  PLR_DEV_OPTS devOpts{};
  UINT32 devOptID;

  DATA_LOADER *dLoad = MemoryLoader_Init(data, size);
  DataLoader_Load(dLoad);
  playerA->LoadFile(dLoad);

  PlayerBase* base = playerA->GetPlayer();
  std::vector<PLR_DEV_INFO> diList;
  base->GetSongDeviceInfo(diList);

  // Enhanced stereo panning
  configure_enhanced_stereo(base, g_enhanced_stereo);

  devOptID = PLR_DEV_ID(DEVID_YMF278B, 0);
  retVal = base->GetDeviceOptions(devOptID, devOpts);
  if (!(retVal & 0x80)) {
    // emuCore[0] is the YMF278B (PCM), emuCore[1] is the linked YMF262 (OPL3)
    if (!devOpts.emuCore[1]) {
      printf("YMF278B detected without OPL3 core. Using MAME OPL3 core.\n");
      devOpts.emuCore[1] = FCC_MAME;
    }
    // Some chips require specific core options to enable OPL3 extensions
    base->SetDeviceOptions(devOptID, devOpts);
  }

  // Force Maxim core for SN76496 (Sega PSG) to enable channel panning
  for (int instance = 0; instance < 2; instance++) {
    devOptID = PLR_DEV_ID(DEVID_SN76496, instance);
    retVal = base->GetDeviceOptions(devOptID, devOpts);
    if (!(retVal & 0x80)) {
      if (!devOpts.emuCore[0]) {
        devOpts.emuCore[0] = FCC_MAXM;
        base->SetDeviceOptions(devOptID, devOpts);
      }
    }
  }

  voices.clear();
  chips.clear();
  size_t curDev;
  for (curDev = 0; curDev < diList.size(); curDev++) {
    const PLR_DEV_INFO& pdi = diList[curDev];
    if (pdi.parentIdx != (UINT32)-1) {
      continue;
    }
    const char* rawDevName = SndEmu_GetDevName(pdi.type, 1, pdi.devCfg);
    std::string devName = getNiceChipName(pdi.type, rawDevName);

    const VoiceInfo &info = deviceToVoiceInfo.at(pdi.type);

    // 1. Primary Part
    std::string primaryName = devName + info.suffix;
    UINT32 devId = PLR_DEV_ID(pdi.type, pdi.instance);
    for (size_t i = 0; i < info.count; i++) {
      voices.emplace_back(DevIdVoiceName({devId, getVoiceName(pdi.type, i), primaryName}));
    }
    chips.emplace_back(Chip{pdi.type, primaryName, devId, 0, info.count});

    // 2. Linked Part (if any)
    if (info.linkedCount > 0) {
      std::string linkedName = devName + info.linkedSuffix;
      for (size_t i = 0; i < info.linkedCount; i++) {
        voices.emplace_back(DevIdVoiceName({devId, getVoiceName(pdi.type, info.count + i), linkedName}));
      }
      chips.emplace_back(Chip{pdi.type, linkedName, devId, 1, info.linkedCount});
    }
  }

  // Reset muting masks to 0 (unmuted) for all channels of active chips in the new song
  for (const auto& chip : chips) {
    if (base->GetDeviceOptions(chip.idx, devOpts) == 0) {
      devOpts.muteOpts.chnMute[chip.linkedIdx] = 0;
      base->SetDeviceMuting(chip.idx, devOpts.muteOpts);
    }
  }

  return 0;
}

UINT32 lvgm_get_position_ms(lvgm_player *player) {
  PlayerA* playerA = real(player);
  return UINT32(playerA->GetCurTime(PLAYTIME_TIME_FILE) * 1000.);
}

UINT32 lvgm_get_duration_ms(lvgm_player *player) {
  PlayerA* playerA = real(player);
  return UINT32(playerA->GetTotalTime(PLAYTIME_TIME_FILE) * 1000.);
}

UINT8 lvgm_start(lvgm_player *player) {
  return real(player)->Start();
}

UINT32 lvgm_render(lvgm_player *player, WAVE_32BS *data, UINT32 count) {
  PlayerA* playerA = real(player);
  count *= 8; // 2 channels, 4 bytes per sample. PlayerA::Render expects buffer size in bytes.
  UINT32 rendered = playerA->Render(count, data);
  if (playerA->GetState() & PLAYSTATE_FIN) {
    // Return 0 to signal end of song playback.
    return 0;
  } else {
    return rendered;
  }
}

UINT8 lvgm_stop(lvgm_player *player) {
  return real(player)->Stop();
}

UINT8 lvgm_reset(lvgm_player *player) {
  return real(player)->Reset();
}

void lvgm_set_playback_speed(lvgm_player *player, double speed) {
  PlayerA* playerA = real(player);
  playerA->SetPlaybackSpeed(speed);
}

double lvgm_get_playback_speed(lvgm_player *player) {
  return real(player)->GetPlaybackSpeed();
}

UINT8 lvgm_seek_ms(lvgm_player *player, UINT32 ms) {
  PlayerA* playerA = real(player);
  auto smp = UINT32(playerA->GetSampleRate() / 1000. * ms / playerA->GetPlaybackSpeed());
  return playerA->Seek(PLAYPOS_SAMPLE, smp);
}

const char* const* lvgm_get_metadata(lvgm_player *player) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return nullptr;

  for (auto & i : meta) {
    i = nullptr;
  }
  const char* const* tagList = base->GetTags();
  for (const char* const* t = tagList; *t; t += 2) {
    if (!strcmp(t[0], "TITLE"))
      meta[0] = t[1];
    else if (!strcmp(t[0], "ARTIST"))
      meta[1] = t[1];
    else if (!strcmp(t[0], "GAME"))
      meta[2] = t[1];
    else if (!strcmp(t[0], "SYSTEM"))
      meta[3] = t[1];
    else if (!strcmp(t[0], "DATE"))
      meta[4] = t[1];
    else if (!strcmp(t[0], "COMMENT"))
      meta[5] = t[1];
  }

  return meta;
}

UINT8 lvgm_get_voice_count(lvgm_player *player) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return 0;

  return voices.size();
}

const char* lvgm_get_voice_name(lvgm_player *player, UINT8 index) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return nullptr;

  if (index >= voices.size()) return nullptr;

  return voices[index].name.c_str();
}

const char* lvgm_get_voice_chip_name(lvgm_player *player, UINT8 index) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return "";

  if (index >= voices.size()) return nullptr;

  return voices[index].chipName.c_str();
}

void lvgm_set_voice_mask(lvgm_player *player, UINT64 mask) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return;

  size_t bitOffset = 0;
  for (const auto& chip : chips) {
    PLR_DEV_OPTS devOpts;
    if (base->GetDeviceOptions(chip.idx, devOpts) == 0) {
      devOpts.muteOpts.chnMute[chip.linkedIdx] = (UINT32)(mask >> bitOffset);
      base->SetDeviceMuting(chip.idx, devOpts.muteOpts);
    }
    bitOffset += chip.voiceCount;
  }
}

UINT64 lvgm_get_voice_mask(lvgm_player *player) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return 0;

  UINT64 mask = 0;
  size_t bitOffset = 0;
  for (const auto& chip : chips) {
    PLR_MUTE_OPTS muteOpts = {};
    base->GetDeviceMuting(chip.idx, muteOpts);
    mask |= (UINT64)muteOpts.chnMute[chip.linkedIdx] << bitOffset;
    bitOffset += chip.voiceCount;
  }

  return mask;
}

// Specify the absolute file path of the OPL4 wave ROM.
void lvgm_set_yrw801_rom_path(lvgm_player *player, const char* path) {
  if (path) g_yrw801_rom_path.assign(path);
  else return;

  PlayerA *playerA = real(player);

  playerA->SetFileReqCallback([](void* userParam, PlayerBase* engine, const char* fileName) -> DATA_LOADER* {
    const char* path = g_yrw801_rom_path.c_str();
    if (strcmp(fileName, "yrw801.rom") == 0) {
      FILE* f = fopen(path, "rb");
      if (!f) {
        fprintf(stderr, "[libvgm player_wrapper.cpp] ERROR: file %s not found .\n", path);
        return nullptr;
      }
      fclose(f);
      DATA_LOADER* romDLoad = FileLoader_Init(path);
      DataLoader_Load(romDLoad);
      return romDLoad;
    }
    return nullptr;
  }, nullptr);
}

void lvgm_set_enhanced_stereo(lvgm_player *player, const bool enable) {
  if (g_enhanced_stereo != enable) {
    g_enhanced_stereo = enable;

    PlayerA *playerA = real(player);
    PlayerBase* base = playerA->GetPlayer();
    if (base) {
      configure_enhanced_stereo(base, enable);
    }
  }
}

#ifdef __cplusplus
}
#endif
