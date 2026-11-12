#pragma once
#include "Arduino.h"
#include <array>
#include <vector>
#include <cassert>
static uint8_t mockSelectedPort = 0;
struct MockWire {
  std::array<uint8_t, 256> registers{};
  std::vector<uint8_t> tx;
  std::array<uint8_t, 512> eeprom;
  bool eepromConnected = true, eepromReadOnly = false, eepromShortRead = false;
  int failAfterBytes = -1, bytesWritten = 0;
  unsigned eepromWrites = 0, pageCrossings = 0;
  uint8_t busy = 0;
  uint16_t eepromCursor = 0;
  MockWire() { eeprom.fill(0xFF); }
  bool cameraConnected = true, shortRead = false;
  bool tofConnected = true, tofReady = false, tofShortRead = false, tofInitOK = true;
  uint16_t tofMm = 700;
  uint8_t tofRangeStatus = 11;
  uint8_t physicalPort = 1, address = 0, cursor = 0;
  void begin() {}
  void beginTransmission(uint8_t a) { address = a; tx.clear(); }
  void write(uint8_t b) { tx.push_back(b); }
  uint8_t endTransmission(bool = true) {
    if (address == 0x50) {
      if (!eepromConnected) return 2;
      if (busy) { --busy; return 2; }
      if (tx.empty()) return 0; // ACK polling
      assert(tx.size() >= 2 && tx.size() <= 18);
      eepromCursor = (uint16_t(tx[0]) << 8) | tx[1];
      if (tx.size() == 2) return 0;
      ++eepromWrites;
      if (eepromReadOnly) return 0; // WP still ACKs, readback must catch it
      for (size_t i = 2; i < tx.size(); ++i) {
        if (failAfterBytes >= 0 && bytesWritten >= failAfterBytes) return 2;
        const uint16_t target = (eepromCursor & ~127) | ((eepromCursor + i - 2) & 127);
        if (target != eepromCursor + i - 2) ++pageCrossings;
        assert(target < eeprom.size()); eeprom[target] = tx[i]; ++bytesWritten;
      }
      busy = 2; return 0;
    }
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
  uint8_t requestFrom(uint8_t a, uint8_t n) {
    const bool partial = a == 0x50 ? eepromShortRead : a == 0x29 ? tofShortRead : shortRead;
    return partial ? n - 1 : n;
  }
  int read() {
    if (address == 0x50) { assert(eepromCursor < eeprom.size()); return eeprom[eepromCursor++]; }
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
