#include "net_input.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include "dmx_out.h"
#include "settings.h"

static constexpr uint16_t ARTNET_PORT = 6454;
static constexpr uint16_t SACN_PORT = 5568;
static constexpr uint16_t OP_POLL = 0x2000;
static constexpr uint16_t OP_POLL_REPLY = 0x2100;
static constexpr uint16_t OP_DMX = 0x5000;
static constexpr size_t ART_POLL_REPLY_LEN = 239;

static WiFiUDP artUdp;
static WiFiUDP sacnUdp;
static bool artOpen = false;
static bool sacnOpen = false;
static uint8_t buf[640];
static uint32_t packetCount = 0;

static uint16_t artPortAddress() {
  return ((cfg.artNet & 0x7F) << 8) | ((cfg.artSubnet & 0x0F) << 4) | (cfg.artUniverse & 0x0F);
}

void inputBegin() {
  if (artOpen) artUdp.stop();
  if (sacnOpen) sacnUdp.stop();
  artOpen = sacnOpen = false;

  if (cfg.artnetEnabled) artOpen = artUdp.begin(ARTNET_PORT);
  if (cfg.sacnEnabled && cfg.sacnUniverse >= 1 && cfg.sacnUniverse <= 63999) {
    IPAddress group(239, 255, cfg.sacnUniverse >> 8, cfg.sacnUniverse & 0xFF);
    sacnOpen = sacnUdp.beginMulticast(group, SACN_PORT);
  }
  dmxNetworkStop();
  Serial.printf("[NET] ArtNet %s (port addr %u), sACN %s (universe %u)\n", artOpen ? "on" : "off",
                artPortAddress(), sacnOpen ? "on" : "off", cfg.sacnUniverse);
}

static IPAddress currentIp() {
  return WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP();
}

static void sendPollReply(IPAddress dest) {
  uint8_t r[ART_POLL_REPLY_LEN] = {0};
  memcpy(r, "Art-Net", 8);
  r[8] = OP_POLL_REPLY & 0xFF;
  r[9] = OP_POLL_REPLY >> 8;
  IPAddress ip = currentIp();
  for (int i = 0; i < 4; i++) r[10 + i] = ip[i];
  r[14] = ARTNET_PORT & 0xFF;
  r[15] = ARTNET_PORT >> 8;
  r[16] = 1;                         // firmware version hi
  r[17] = 0;                         // firmware version lo
  r[18] = cfg.artNet & 0x7F;         // NetSwitch
  r[19] = cfg.artSubnet & 0x0F;      // SubSwitch
  r[23] = 0xE0;                      // Status1: indicators normal, port address set via network
  strlcpy((char *)r + 26, cfg.shortName, 18);
  strlcpy((char *)r + 44, cfg.longName, 64);
  snprintf((char *)r + 108, 64, "#0001 [%04lu] OK", (unsigned long)(packetCount % 10000));
  r[173] = 1;                        // NumPorts
  r[174] = 0x80;                     // PortTypes[0]: output, DMX512
  r[182] = dmxActiveSource() == DmxSource::ArtNet ? 0x80 : 0x00;  // GoodOutput[0]
  r[190] = cfg.artUniverse & 0x0F;   // SwOut[0]
  r[200] = 0x00;                     // Style: StNode
  uint8_t mac[6];
  WiFi.macAddress(mac);
  memcpy(r + 201, mac, 6);
  for (int i = 0; i < 4; i++) r[207 + i] = ip[i];  // BindIp
  r[211] = 1;                        // BindIndex
  r[212] = 0x0D;                     // Status2: web config, DHCP capable, 15-bit port address

  artUdp.beginPacket(dest, ARTNET_PORT);
  artUdp.write(r, sizeof(r));
  artUdp.endPacket();
}

static void handleArtNet() {
  int len;
  while ((len = artUdp.parsePacket()) > 0) {
    if (len > (int)sizeof(buf)) {
      artUdp.flush();
      continue;
    }
    artUdp.read(buf, len);
    if (len < 10 || memcmp(buf, "Art-Net", 8) != 0) continue;
    uint16_t op = buf[8] | (buf[9] << 8);

    if (op == OP_DMX && len >= 18) {
      uint16_t portAddr = buf[14] | ((buf[15] & 0x7F) << 8);
      uint16_t dataLen = (buf[16] << 8) | buf[17];
      if (portAddr != artPortAddress() || dataLen < 2 || dataLen > 512 || 18 + dataLen > len) continue;
      dmxNetworkFrame(DmxSource::ArtNet, buf + 18, dataLen);
      packetCount++;
    } else if (op == OP_POLL) {
      sendPollReply(artUdp.remoteIP());
    }
  }
}

static void handleSacn() {
  static const uint8_t ACN_ID[12] = {'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', 0, 0, 0};
  int len;
  while ((len = sacnUdp.parsePacket()) > 0) {
    if (len > (int)sizeof(buf)) {
      sacnUdp.flush();
      continue;
    }
    sacnUdp.read(buf, len);
    if (len < 126 || memcmp(buf + 4, ACN_ID, sizeof(ACN_ID)) != 0) continue;

    uint32_t rootVector = (buf[18] << 24) | (buf[19] << 16) | (buf[20] << 8) | buf[21];
    uint32_t frameVector = (buf[40] << 24) | (buf[41] << 16) | (buf[42] << 8) | buf[43];
    if (rootVector != 0x00000004 || frameVector != 0x00000002 || buf[117] != 0x02) continue;

    uint16_t universe = (buf[113] << 8) | buf[114];
    if (universe != cfg.sacnUniverse) continue;

    if (buf[112] & 0x40) {  // stream terminated
      if (dmxActiveSource() == DmxSource::Sacn) dmxNetworkStop();
      continue;
    }
    if (buf[112] & 0x20) continue;  // preview data
    if (buf[125] != 0x00) continue;  // only null start code

    uint16_t count = (buf[123] << 8) | buf[124];  // includes start code
    if (count < 2 || count > 513 || 125 + count > len) continue;
    dmxNetworkFrame(DmxSource::Sacn, buf + 126, count - 1);
    packetCount++;
  }
}

void inputLoop() {
  if (artOpen) handleArtNet();
  if (sacnOpen) handleSacn();
}

uint32_t inputPacketCount() { return packetCount; }
