#include "status.h"
#include <Arduino.h>
#include "config.h"
#include "dmx_out.h"
#include "network.h"
#include "settings.h"

static uint32_t pressedSinceMs = 0;
static uint32_t apPressedSinceMs = 0;
static bool apPressHandled = false;

void statusBegin() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_RESET_BUTTON, INPUT_PULLUP);
  pinMode(PIN_AP_BUTTON, INPUT_PULLUP);
}

void statusLoop() {
  uint32_t now = millis();

  if (digitalRead(PIN_AP_BUTTON) == LOW) {
    if (!apPressedSinceMs) apPressedSinceMs = now;
    // Fire once per press; button must be released before the next toggle
    if (!apPressHandled && now - apPressedSinceMs > AP_BUTTON_HOLD_MS) {
      netToggleForcedAp();
      apPressHandled = true;
    }
  } else {
    uint32_t held = apPressedSinceMs ? now - apPressedSinceMs : 0;
    if (!apPressHandled && held >= SHORT_PRESS_MIN_MS && held < SHORT_PRESS_MAX_MS) dmxSetExternal(!dmxIsExternal());
    apPressedSinceMs = 0;
    apPressHandled = false;
  }

  if (digitalRead(PIN_RESET_BUTTON) == LOW) {
    if (!pressedSinceMs) pressedSinceMs = now;
    if (now - pressedSinceMs > FACTORY_RESET_HOLD_MS) settingsFactoryReset();
    // Rapid blink while held to signal the pending reset
    digitalWrite(PIN_STATUS_LED, (now / 50) & 1);
    return;
  }
  pressedSinceMs = 0;

  // LED: flicker = network DMX input, slow blink = AP, solid = WiFi connected, fast blink = connecting
  bool on;
  DmxSource src = dmxActiveSource();
  if (src == DmxSource::ArtNet || src == DmxSource::Sacn) on = (now / 60) & 1;
  else if (netApActive()) on = (now / 1000) & 1;
  else if (netStaConnected()) on = true;
  else on = (now / 200) & 1;
  digitalWrite(PIN_STATUS_LED, on);
}
