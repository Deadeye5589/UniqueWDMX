#pragma once
#include <Arduino.h>

void inputBegin();    // (re)open sockets according to cfg; call after config/network change
void inputLoop();
uint32_t inputPacketCount();
