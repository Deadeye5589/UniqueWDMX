#include "sync.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include "dmx_out.h"
#include "network.h"
#include "settings.h"

static const uint8_t MAGIC[4] = {'U', 'W', 'D', 'S'};
static constexpr uint8_t PROTO_VERSION = 1;
static constexpr uint32_t KEEPALIVE_MS = 1000;  // lets late-joining receivers catch up
static constexpr uint32_t MIN_INTERVAL_MS = 20;
static constexpr uint32_t RX_TIMEOUT_MS = 3000;

struct __attribute__((packed)) SyncPacket {
  uint8_t magic[4];
  uint8_t version;
  uint8_t groups;
  uint8_t fan;
  uint8_t pump;
};

static WiFiUDP udp;
static bool udpOpen = false;
static volatile int8_t masterRequest = -1;
static uint32_t lastTxMs = 0;
static uint32_t lastRxMs = 0;
static int lastFan = -1;
static int lastPump = -1;

void syncBegin() {
  if (udpOpen) udp.stop();
  udpOpen = udp.begin(cfg.syncPort);
  lastFan = lastPump = -1;
  Serial.printf("[SYNC] Port %lu, master %s, receive %s\n", (unsigned long)cfg.syncPort,
                cfg.syncMaster ? "on" : "off", cfg.syncReceive ? "on" : "off");
}

void syncRequestMaster(bool on) { masterRequest = on ? 1 : 0; }

bool syncReceiving() { return lastRxMs && millis() - lastRxMs < RX_TIMEOUT_MS; }

static void sendTo(IPAddress dest, const SyncPacket &p, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    udp.beginPacket(dest, cfg.syncPort);
    udp.write((const uint8_t *)&p, sizeof(p));
    udp.endPacket();
  }
}

static void receive() {
  int len;
  while ((len = udp.parsePacket()) > 0) {
    SyncPacket p;
    if (len != sizeof(p)) {
      udp.flush();
      continue;
    }
    udp.read((uint8_t *)&p, sizeof(p));
    if (memcmp(p.magic, MAGIC, sizeof(MAGIC)) != 0 || p.version != PROTO_VERSION) continue;
    IPAddress from = udp.remoteIP();
    if (from == WiFi.localIP() || from == WiFi.softAPIP()) continue;  // own broadcast
    if (cfg.syncMaster || !cfg.syncReceive || !(p.groups & cfg.syncRecvGroups)) continue;
    // ArtNet, sACN and External DMX take priority over sync
    if (dmxActiveSource() != DmxSource::Manual) continue;
    dmxSetManual(p.fan, p.pump);
    lastRxMs = millis();
  }
}

static void transmit() {
  if (!cfg.syncMaster || !cfg.syncSendGroups || dmxActiveSource() != DmxSource::Manual) return;

  uint8_t fan = dmxManualFan();
  uint8_t pump = dmxManualPump();
  uint32_t now = millis();
  bool changed = fan != lastFan || pump != lastPump;
  if (!(changed && now - lastTxMs >= MIN_INTERVAL_MS) && now - lastTxMs < KEEPALIVE_MS) return;

  SyncPacket p;
  memcpy(p.magic, MAGIC, sizeof(MAGIC));
  p.version = PROTO_VERSION;
  p.groups = cfg.syncSendGroups;
  p.fan = fan;
  p.pump = pump;
  uint8_t count = changed ? constrain(cfg.syncRepeat, 1, 10) : 1;

  if (netStaConnected()) {
    IPAddress ip = WiFi.localIP(), mask = WiFi.subnetMask();
    sendTo(IPAddress(ip[0] | (uint8_t)~mask[0], ip[1] | (uint8_t)~mask[1], ip[2] | (uint8_t)~mask[2],
                     ip[3] | (uint8_t)~mask[3]),
           p, count);
  }
  if (netApActive() && WiFi.softAPgetStationNum() > 0) {
    IPAddress ap = WiFi.softAPIP();
    sendTo(IPAddress(ap[0], ap[1], ap[2], 255), p, count);  // AP subnet is always /24
  }

  lastFan = fan;
  lastPump = pump;
  lastTxMs = now;
}

void syncLoop() {
  if (masterRequest >= 0) {
    cfg.syncMaster = masterRequest == 1;
    masterRequest = -1;
    lastFan = lastPump = -1;
    settingsSave();
    Serial.printf("[SYNC] Master %s\n", cfg.syncMaster ? "on" : "off");
  }
  if (!udpOpen) return;
  receive();
  transmit();
}
