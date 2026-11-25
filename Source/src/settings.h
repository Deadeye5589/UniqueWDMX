#pragma once
#include <Arduino.h>
#include "config.h"

struct Preset {
  char name[17];
  uint8_t fan;
  uint8_t pump;
};

struct Settings {
  uint32_t magic;

  // WiFi client
  char ssid[33];
  char pass[65];
  char hostname[33];
  bool staticIp;
  uint32_t ip, gateway, subnet, dns;

  // Access point
  char apSsid[33];
  char apPass[65];
  uint8_t apChannel;
  bool apHidden;

  // DMX output
  uint16_t baseAddress;  // 1..511 (Fan = base, Pump = base + 1)

  // Network input
  bool artnetEnabled;
  uint8_t artNet;        // 0..127
  uint8_t artSubnet;     // 0..15
  uint8_t artUniverse;   // 0..15
  bool sacnEnabled;
  uint16_t sacnUniverse; // 1..63999
  uint16_t inputTimeoutS;
  char shortName[18];
  char longName[64];

  Preset presets[NUM_PRESETS];

  // New fields must be appended here so older stored settings stay loadable
  char deviceName[33];

  // Sync; uint32_t forces alignment so these start past the padding of older stored layouts
  uint32_t syncPort;
  bool syncMaster;
  bool syncReceive;
  uint8_t syncSendGroups;  // bitmask, groups 1..8
  uint8_t syncRecvGroups;  // bitmask, groups 1..8
  uint8_t syncRepeat;      // copies sent per change (1..10); keepalive is always sent once
};

extern Settings cfg;

void settingsLoad();
void settingsSave();
void settingsDefaults(Settings &s);
void settingsFactoryReset();  // erases NVS and reboots
