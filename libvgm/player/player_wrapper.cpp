//
// Created by Matt Montag on 6/18/23.
//
#include <cstring>
#include <vector>
#include <cstdio>
#include <bitset>
#include <map>
#include <string>

#include "../stdtype.h"
#include "../utils/DataLoader.h"
#include "playerbase.hpp"
#include "playera.hpp"
#include "utils/MemoryLoader.h"
#include "vgmplayer.hpp"
#include "s98player.hpp"
#include "droplayer.hpp"
#include "gymplayer.hpp"
#include "emu/SoundEmu.h"
#include "emu/SoundDevs.h"


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
};

const std::map<int, VoiceInfo> deviceToVoiceInfo = { // --------------------------------------------------
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
  {DEVID_OKIM6258, {1,  0, "PCM"}},
  {DEVID_OKIM6295, {4,  0, "PCM"}},
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
  {DEVID_Y8950,    {1,  0}},
  {DEVID_YM2151,   {8,  0, "FM"}},
  {DEVID_YM2203,   {3,  3, "", {"FM 1",    "FM 2",    "FM 3",     "PSG 1",  "PSG 2",  "PSG 3"}}},
  {DEVID_YM2413,   {6,  3, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "PSG 1", "PSG 2", "PSG 3"}}},
  {DEVID_YM2608,   {13, 3, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "PCM 1", "PCM 2", "PCM 3", "PCM 4", "PCM 5", "PCM 6", "PCM 7", "PSG 1", "PSG 2", "PSG 3"}}},
  {DEVID_YM2610,   {13, 3, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "PCM 1", "PCM 2", "PCM 3", "PCM 4", "PCM 5", "PCM 6", "PCM 7", "PSG 1", "PSG 2", "PSG 3"}}},
  {DEVID_YM2612,   {7,  0, "", {"FM 1",    "FM 2",    "FM 3",     "FM 4",   "FM 5",   "FM 6", "PCM"}}},
  {DEVID_YM3526,   {9,  0, "FM"}},
  {DEVID_YM3812,   {9,  0, "FM"}},
  {DEVID_YMF262,   {18, 0, "FM"}},
  {DEVID_YMF271,   {24, 0, "FM"}},
  {DEVID_YMF278B,  {24, 0}},
  {DEVID_YMW258,   {28, 0, "PCM"}},
  {DEVID_YMZ280B,  {8,  0}}};

struct Chip {
  size_t type;
  std::string name;
  // Index of the chip as defined in the current song
  size_t idx;
  // Voice count of this chip and linked device (if any)
  std::vector<size_t> voiceCounts;
};

// Vector of chips (populated after song load)
std::vector<Chip> chips;

struct DevIdVoiceName {
  size_t id;
  std::string name;
};

// Vector of voice names (populated after song load)
std::vector<DevIdVoiceName> voices;
static char const *meta[8];

// Get voice name from deviceToVoiceInfo.at(id).names[v], fall back to voiceInfo[id].type if name is empty
std::string getVoiceName(const int id, const int v) {
  const auto& info = deviceToVoiceInfo.at(id);
  if (v < info.names.size() && !info.names[v].empty()) {
    return info.names[v];
  } else {
    return info.type + " " + std::to_string(v + 1);
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

  DATA_LOADER *dLoad = MemoryLoader_Init(data, size);
  DataLoader_Load(dLoad);
  playerA->LoadFile(dLoad);

  PlayerBase* base = playerA->GetPlayer();
  std::vector<PLR_DEV_INFO> diList;
  base->GetSongDeviceInfo(diList);

  // Enhanced stereo panning
  PLR_DEV_OPTS devOpts{};
  UINT32 devOptID;
  devOptID = PLR_DEV_ID(DEVID_YM2413, 0);
  retVal = base->GetDeviceOptions(devOptID, devOpts);
  if (! (retVal & 0x80))
  {
    static const INT16 panPos[14] = {
      -0x100, +0x100, -0x80, +0x80, -0x40, +0x40, -0xC0, +0xC0, 0x00,
      -0x60, +0x60, 0x00, -0xC0, +0xC0};
    memcpy(devOpts.panOpts.chnPan[0], panPos, sizeof(panPos));
    base->SetDeviceOptions(devOptID, devOpts);
  }
  devOptID = PLR_DEV_ID(DEVID_AY8910, 0);
  retVal = base->GetDeviceOptions(devOptID, devOpts);
  if (! (retVal & 0x80))
  {
    static const INT16 panPos[3] = {-0x80, +0x80, 0x00};
    memcpy(devOpts.panOpts.chnPan[0], panPos, sizeof(panPos));
    base->SetDeviceOptions(devOptID, devOpts);
  }
  for (auto deviceId : { DEVID_YM2203, DEVID_YM2608 }) {
    devOptID = PLR_DEV_ID(deviceId, 0);
    retVal = base->GetDeviceOptions(devOptID, devOpts);
    if (! (retVal & 0x80))
    {
      static const INT16 panPos[3] = {-0x80, +0x80, 0x00};
      // Set pan for SSG channels (linked device)
      memcpy(devOpts.panOpts.chnPan[1], panPos, sizeof(panPos));
      base->SetDeviceOptions(devOptID, devOpts);
    }
  }

  voices.clear();
  chips.clear();
  size_t curDev;
  int totalVoiceCount = 0;
  for (curDev = 0; curDev < diList.size(); curDev ++)
  {
    const PLR_DEV_INFO& pdi = diList[curDev];
    const char* devName = SndEmu_GetDevName(pdi.type, 1, pdi.devCfg);

    const VoiceInfo &info = deviceToVoiceInfo.at(pdi.type);
    size_t devVoiceCount = info.count + info.linkedCount;
    printf("  %s: %d voices\n", devName, (int)devVoiceCount);
    // for each dev voice
    for (int i = 0; i < devVoiceCount; i ++) {
      // get voice name
      const std::string voiceName = getVoiceName(pdi.type, i);
      // add to voiceNames
      voices.emplace_back(DevIdVoiceName({curDev, voiceName}));
    }
    // add to chips
    std::vector<size_t> voiceCounts;
    voiceCounts.emplace_back(info.count);
    if (info.linkedCount > 0) {
      voiceCounts.emplace_back(info.linkedCount);
    }
    chips.emplace_back(Chip{pdi.type, devName, curDev, voiceCounts});
  }

  return 0;
}

UINT32 lvgm_get_position_ms(lvgm_player *player) {
  PlayerA* playerA = real(player);
  // Note: GetCurTime varies with speed. Multiply by speed to get the unscaled position.
  return UINT32(playerA->GetCurTime(1) * 1000. * playerA->GetPlaybackSpeed());
}

UINT32 lvgm_get_duration_ms(lvgm_player *player) {
  PlayerA* playerA = real(player);
  return UINT32(playerA->GetTotalTime(1) * 1000. * playerA->GetPlaybackSpeed());
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

void lvgm_set_voice_mask(lvgm_player *player, UINT64 mask) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return;

  size_t bitOffset = 0;
  // Iterate over chips
  for (const auto& chip : chips) {
    UINT32 masks[2] = {0};
    // Iterate over devices (there will either be 1 or 2, in case of a linked device)
    for (size_t i = 0; i < chip.voiceCounts.size(); i++) {
      // Create mask for this device
      masks[i] |= mask >> bitOffset;
      // Slide window by number of voices in this device
      bitOffset += chip.voiceCounts[i];
      // std::bitset<32> bits(masks[i]); printf("Chip %d: %s (%d voices)\n", (int)chip.idx, bits.to_string().c_str(), (int)chip.voiceCounts[i]);
    }
    PLR_MUTE_OPTS muteOpts = {0, masks[0], masks[1]};
    base->SetDeviceMuting(chip.idx, muteOpts);
  }

}

UINT64 lvgm_get_voice_mask(lvgm_player *player) {
  PlayerA* playerA = real(player);
  PlayerBase* base = playerA->GetPlayer();
  if (base == nullptr) return 0;

  UINT64 mask = 0;
  size_t bitOffset = 0;
  // Iterate over chips
  for (const auto& chip : chips) {
    PLR_MUTE_OPTS muteOpts = {};
    base->SetDeviceMuting(chip.idx, muteOpts);
    // Iterate over devices (there will either be 1 or 2, in case of a linked device)
    for (size_t i = 0; i < chip.voiceCounts.size(); i++) {
      // Create mask for this device
      mask |= muteOpts.chnMute[i] << bitOffset;
      // Slide window by number of voices in this device
      bitOffset += chip.voiceCounts[i];
    }
  }

  return mask;
}

#ifdef __cplusplus
}
#endif
