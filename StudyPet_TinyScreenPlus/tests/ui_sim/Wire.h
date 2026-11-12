#pragma once
#include "Arduino.h"
#include <array>
#include <vector>
static uint8_t mockSelectedPort = 0;
struct MockWire {
  std::array<uint8_t, 256> registers{};
  std::vector<uint8_t> tx;
  bool cameraConnected = true, shortRead = false;
  bool tofConnected = true, tofReady = false, tofShortRead = false, tofInitOK = true;
  uint16_t tofMm = 700;
  uint8_t tofRangeStatus = 11;
  uint8_t physicalPort = 1, address = 0, cursor = 0;
  void begin() {}
  void beginTransmission(uint8_t a) { address = a; tx.clear(); }
  void write(uint8_t b) { tx.push_back(b); }
  uint8_t endTransmission(bool = true) {
    if (address == 0x29) {
      if (!tofConnected || mockSelectedPort != physicalPort) return 2;
      if (!tx.empty()) cursor = tx[0];
      if (tx.size() > 1 && cursor == 0x0B) tofReady = false;
      return 0;
    }
    if (!cameraConnected || address != 0x69 || mockSelectedPort != physicalPort) return 2;
    if (!tx.empty()) cursor = tx[0];
    if (tx.size() > 1) registers[cursor] = tx[1];
    return 0;
  }
  uint8_t requestFrom(uint8_t a, uint8_t n) { return (a == 0x29 ? tofShortRead : shortRead) ? n - 1 : n; }
  int read() {
    const uint8_t reg = cursor++;
    if (address != 0x29) return registers[reg];
    if (reg == 0x13) return tofReady ? 1 : 0;
    if (reg == 0x14) return tofRangeStatus << 3;
    if (reg == 0x1E) return tofMm >> 8;
    if (reg == 0x1F) return tofMm & 255;
    return 0;
  }
  void pixel(uint8_t i, int16_t quarterC) {
    registers[0x80 + i * 2] = quarterC & 0xFF;
    registers[0x81 + i * 2] = (uint16_t(quarterC) >> 8) & 0x0F;
  }
  void fill(int16_t quarterC) { for (uint8_t i = 0; i < 64; ++i) pixel(i, quarterC); }
};
static MockWire Wire;
