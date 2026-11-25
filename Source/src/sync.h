#pragma once
#include <Arduino.h>

void syncBegin();  // (re)open the UDP socket; call after config/network change
void syncLoop();
void syncRequestMaster(bool on);  // safe to call from the web task; applied in syncLoop()
bool syncReceiving();             // true while packets from a master are being applied
