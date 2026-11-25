#pragma once
#include <Arduino.h>

void netBegin();
bool netLoop();  // returns true when connectivity changed (sockets should be reopened)
bool netStaConnected();
bool netApActive();
bool netApForced();
void netToggleForcedAp();  // safe to call from any task; applied in netLoop()
