#pragma once
#include "Arduino.h"
#define INPUT_PULLDOWN 3
#define MSBFIRST 1
#define SPI_MODE0 0
static bool mockBleIrqAttached = false, mockBlePresent = true;
inline void detachInterrupt(int) { mockBleIrqAttached = false; }
struct SPISettings { SPISettings(int, int, int) {} };
struct MockBleSPI {
  void begin() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t) { return mockBlePresent ? 2 : 0; }
};
static MockBleSPI SPI;
