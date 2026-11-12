#pragma once
#include "Arduino.h"
#include "Wire.h"
struct MockWireling {
  unsigned beginCalls = 0;
  void begin() { ++beginCalls; }
  void selectPort(uint8_t port) { mockSelectedPort = port; }
};
static MockWireling Wireling;
