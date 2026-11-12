#pragma once
#include "Wire.h"
struct VL53L0X {
  uint8_t last_status = 0;
  void setTimeout(uint16_t) {}
  bool init() { return Wire.tofInitOK && Wire.tofConnected && mockSelectedPort == Wire.physicalPort; }
  bool setMeasurementTimingBudget(uint32_t) { return true; }
  void startContinuous(uint32_t) { last_status = 0; }
  void stopContinuous() {}
};
