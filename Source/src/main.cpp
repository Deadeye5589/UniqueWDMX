#include <Arduino.h>
#include "config.h"
#include "settings.h"
#include "dmx_out.h"
#include "network.h"
#include "net_input.h"
#include "sync.h"
#include "web.h"
#include "status.h"

void setup() {
  Serial.begin(115200);
  Serial.printf("\nUniqueWDMX %s\n", FW_VERSION);

  statusBegin();
  settingsLoad();
  dmxBegin();
  netBegin();
  inputBegin();
  syncBegin();
  webBegin();
}

void loop() {
  if (netLoop()) {
    // Rebind sockets / rejoin multicast after connectivity change
    inputBegin();
    syncBegin();
  }
  inputLoop();
  syncLoop();
  dmxLoop();
  webLoop();
  statusLoop();
}