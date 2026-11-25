#pragma once
#include <Arduino.h>

enum class DmxSource : uint8_t { Manual, ArtNet, Sacn, External };

void dmxBegin();
void dmxLoop();
void dmxPause();
void dmxResume();

// External mode routes an external DMX signal through the SPDT switch; UART output is stopped
void dmxSetExternal(bool external);
bool dmxIsExternal();

// Manual control (thread-safe for single-byte writes from the web task)
void dmxSetManual(int fan, int pump);  // pass -1 to keep a value
uint8_t dmxManualFan();
uint8_t dmxManualPump();

// Network input, called from the loop task
void dmxNetworkFrame(DmxSource src, const uint8_t *data, uint16_t len);
void dmxNetworkStop();

DmxSource dmxActiveSource();
const char *dmxSourceName(DmxSource s);
uint8_t dmxOutputSlot(uint16_t address);  // 1..512
