#include "dmx_out.h"
#include <esp_dmx.h>
#include "config.h"
#include "settings.h"

static const dmx_port_t DMX_PORT = DMX_NUM_1;

static uint8_t frame[DMX_PACKET_SIZE];
static uint8_t netData[DMX_PACKET_SIZE - 1];
static DmxSource netSource = DmxSource::ArtNet;
static uint32_t lastNetMs = 0;
static bool netValid = false;
static uint32_t lastSendMs = 0;
static volatile bool paused = false;
static volatile uint8_t manualFan = 0;
static volatile uint8_t manualPump = 0;
static volatile bool external = true;
// Guards the driver against pause/resume from the web task (OTA)
static SemaphoreHandle_t lock;

void dmxBegin() {
  pinMode(PIN_SPDT_CTRL, OUTPUT);
  digitalWrite(PIN_SPDT_CTRL, LOW);
  lock = xSemaphoreCreateMutex();
  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_driver_install(DMX_PORT, &config, nullptr, 0);
  dmx_set_pin(DMX_PORT, PIN_DMX_TX, PIN_DMX_RX, PIN_DMX_DE);
  memset(frame, 0, sizeof(frame));
}

static bool networkActive() {
  return netValid && (millis() - lastNetMs) < (uint32_t)cfg.inputTimeoutS * 1000UL;
}

void dmxLoop() {
  if (external || paused || millis() - lastSendMs < DMX_REFRESH_MS) return;
  if (xSemaphoreTake(lock, 0) != pdTRUE) return;
  if (paused || !dmx_wait_sent(DMX_PORT, 0)) {
    xSemaphoreGive(lock);
    return;
  }

  frame[0] = DMX_SC;
  if (networkActive()) {
    memcpy(frame + 1, netData, sizeof(netData));
  } else {
    netValid = false;
    memset(frame + 1, 0, DMX_PACKET_SIZE - 1);
    uint16_t base = constrain(cfg.baseAddress, 1, 511);
    frame[base] = manualFan;
    frame[base + 1] = manualPump;
  }
  dmx_write(DMX_PORT, frame, DMX_PACKET_SIZE);
  dmx_send(DMX_PORT);
  lastSendMs = millis();
  xSemaphoreGive(lock);
}

void dmxPause() {
  xSemaphoreTake(lock, portMAX_DELAY);
  if (!paused) {
    dmx_wait_sent(DMX_PORT, DMX_TIMEOUT_TICK);
    dmx_driver_disable(DMX_PORT);
    paused = true;
  }
  xSemaphoreGive(lock);
}

void dmxResume() {
  xSemaphoreTake(lock, portMAX_DELAY);
  if (paused) {
    dmx_driver_enable(DMX_PORT);
    paused = false;
  }
  xSemaphoreGive(lock);
}

void dmxSetExternal(bool ext) {
  external = ext;
  digitalWrite(PIN_SPDT_CTRL, ext ? LOW : HIGH);
  Serial.printf("[DMX] Mode: %s\n", ext ? "external DMX" : "internal");
}

bool dmxIsExternal() { return external; }

void dmxSetManual(int fan, int pump) {
  if (fan >= 0) manualFan = (uint8_t)constrain(fan, 0, 255);
  if (pump >= 0) manualPump = (uint8_t)constrain(pump, 0, 255);
}

uint8_t dmxManualFan() { return manualFan; }
uint8_t dmxManualPump() { return manualPump; }

void dmxNetworkFrame(DmxSource src, const uint8_t *data, uint16_t len) {
  len = min<uint16_t>(len, sizeof(netData));
  memcpy(netData, data, len);
  if (len < sizeof(netData)) memset(netData + len, 0, sizeof(netData) - len);
  netSource = src;
  lastNetMs = millis();
  netValid = true;
}

void dmxNetworkStop() { netValid = false; }

DmxSource dmxActiveSource() {
  if (external) return DmxSource::External;
  return networkActive() ? netSource : DmxSource::Manual;
}

const char *dmxSourceName(DmxSource s) {
  switch (s) {
    case DmxSource::ArtNet: return "artnet";
    case DmxSource::Sacn: return "sacn";
    case DmxSource::External: return "external";
    default: return "manual";
  }
}

uint8_t dmxOutputSlot(uint16_t address) {
  if (address < 1 || address > 512) return 0;
  return frame[address];
}
