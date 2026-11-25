#include "settings.h"
#include <Preferences.h>
#include "dmx_out.h"

// Bump when the Settings layout changes to force defaults
static constexpr uint32_t SETTINGS_MAGIC = 0x57444D01;
static const char *NVS_NS = "wdmx";
static const char *NVS_KEY = "cfg";

Settings cfg;

void settingsDefaults(Settings &s) {
  memset(&s, 0, sizeof(s));
  s.magic = SETTINGS_MAGIC;
  strlcpy(s.hostname, DEFAULT_HOSTNAME, sizeof(s.hostname));
  strlcpy(s.apSsid, DEFAULT_AP_SSID, sizeof(s.apSsid));
  strlcpy(s.apPass, DEFAULT_AP_PASS, sizeof(s.apPass));
  s.apChannel = 1;
  s.subnet = (uint32_t)IPAddress(255, 255, 255, 0);
  s.baseAddress = 1;
  s.artnetEnabled = true;
  s.sacnEnabled = false;
  s.sacnUniverse = 1;
  s.inputTimeoutS = DEFAULT_INPUT_TIMEOUT_S;
  strlcpy(s.shortName, "UniqueWDMX", sizeof(s.shortName));
  strlcpy(s.longName, "UniqueWDMX ArtNet/sACN to DMX Node", sizeof(s.longName));

  static const Preset defaults[NUM_PRESETS] = {
      {"Off", 0, 0}, {"Low", 64, 64}, {"Medium", 128, 128}, {"High", 192, 192}, {"Full", 255, 255}};
  memcpy(s.presets, defaults, sizeof(defaults));

  s.syncPort = DEFAULT_SYNC_PORT;
  s.syncMaster = false;
  s.syncReceive = true;
  s.syncSendGroups = 0x01;
  s.syncRecvGroups = 0x01;
  s.syncRepeat = 1;
}

void settingsLoad() {
  Preferences p;
  bool ok = false;
  settingsDefaults(cfg);
  if (p.begin(NVS_NS, true)) {
    size_t len = p.getBytesLength(NVS_KEY);
    if (len >= sizeof(uint32_t) && len <= sizeof(Settings)) {
      Settings stored;
      settingsDefaults(stored);
      p.getBytes(NVS_KEY, &stored, len);
      if (stored.magic == SETTINGS_MAGIC) {
        cfg = stored;
        ok = true;
      }
    }
    p.end();
  }
  if (!ok) Serial.println(F("[CFG] No valid settings, using defaults"));
}

void settingsSave() {
  // Flash writes disable the cache; pause the DMX driver so the frame isn't corrupted
  dmxPause();
  Preferences p;
  if (p.begin(NVS_NS, false)) {
    p.putBytes(NVS_KEY, &cfg, sizeof(Settings));
    p.end();
  }
  dmxResume();
  Serial.println(F("[CFG] Saved"));
}

void settingsFactoryReset() {
  Serial.println(F("[CFG] Factory reset"));
  dmxPause();
  Preferences p;
  if (p.begin(NVS_NS, false)) {
    p.clear();
    p.end();
  }
  delay(200);
  ESP.restart();
}
