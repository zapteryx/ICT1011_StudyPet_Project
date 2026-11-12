#ifndef STUDYPET_DEADLINE_STORAGE_H
#define STUDYPET_DEADLINE_STORAGE_H
#include <Arduino.h>
#include <Wire.h>
#include "StudyPetConfig.h"

// ASD2202-R-E / 24FC1025, factory A1=A0=0. Override for modified jumpers.
// Only the first 256 bytes of the lower 64 KiB bank are reserved here.
#ifndef DEADLINE_EEPROM_ADDRESS
#define DEADLINE_EEPROM_ADDRESS 0x50
#endif
#ifndef DEADLINE_EEPROM_OFFSET
#define DEADLINE_EEPROM_OFFSET 0
#endif

// Included after Deadline and the subject/type/reminder definitions in Features.h.
// Wire-format integers are explicitly little-endian; no C++ struct padding is saved.
class DeadlineStorage {
public:
  static const uint8_t MAX = 8;
  enum State : uint8_t { EMPTY, SAVED, RECOVERED, CORRUPT, IO_ERROR, UNINITIALIZED };
  State state() const { return state_; }
  bool load(Deadline *items) {
    uint8_t a[SIZE], b[SIZE]; int8_t active;
    if (!scan(a, b, active)) return false;
    memset(items, 0, sizeof(Deadline) * MAX);
    if (active >= 0) {
      const uint8_t *data = (active == 0 ? a : b) + 12;
      for (uint8_t i = 0; i < MAX; ++i, data += 8) {
        items[i].used = data[0] != 0;
        if (!items[i].used) continue;
        items[i].subject = data[1]; items[i].kind = data[2]; items[i].lead = data[3];
        items[i].due = get32(data + 4);
      }
    }
    return true; // blank/corrupt records are exposed distinctly in state()
  }
  bool save(const Deadline *items) {
    uint8_t a[SIZE], b[SIZE]; int8_t active;
    if (!scan(a, b, active)) return false; // never overwrite an unread record
    const uint8_t *old = active == 0 ? a : b;
    const uint32_t sequence = active < 0 ? 1 : get32(old + 8) + 1;
    const uint16_t target = DEADLINE_EEPROM_OFFSET + (active == 0 ? 128 : 0);
    uint8_t image[SIZE] = {};
    image[0] = COMMIT;
    memcpy(image + 1, "SPDL", 4); image[5] = 1; image[6] = MAX; image[7] = 64;
    put32(image + 8, sequence);
    for (uint8_t i = 0; i < MAX; ++i) {
      if (!items[i].used) continue;
      uint8_t *d = image + 12 + i * 8;
      d[0] = 1; d[1] = items[i].subject; d[2] = items[i].kind; d[3] = items[i].lead;
      put32(d + 4, items[i].due);
    }
    put32(image + 76, crc(image + 1, 75));
    if (!valid(image)) { state_ = IO_ERROR; return false; }
    // Commit last: a partial write cannot supersede the previous snapshot.
    const uint8_t invalid = 0;
    uint8_t verify[SIZE];
    if (!write(target, &invalid, 1) || !read(target, verify, 1) || verify[0] != 0 ||
        !write(target + 1, image + 1, SIZE - 1) || !read(target, verify, SIZE) ||
        verify[0] != 0 || memcmp(verify + 1, image + 1, SIZE - 1) != 0 ||
        !write(target, image, 1) || !read(target, verify, SIZE) ||
        memcmp(verify, image, SIZE) != 0 || !valid(verify)) {
      state_ = IO_ERROR; return false;
    }
    state_ = SAVED;
    return true;
  }
private:
  static const uint8_t SIZE = 80, COMMIT = 0xA5;
  static_assert((DEADLINE_EEPROM_ADDRESS & 0xFC) == 0x50, "Use lower-bank EEPROM address 0x50..0x53");
  static_assert(DEADLINE_EEPROM_OFFSET % 128 == 0 && DEADLINE_EEPROM_OFFSET <= 65280,
                "Reserve two aligned pages within lower EEPROM bank");
  State state_ = EMPTY;
  static uint32_t get32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
  }
  static void put32(uint8_t *p, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) { p[i] = value & 255; value >>= 8; }
  }
  static uint32_t crc(const uint8_t *p, uint8_t length) {
    uint32_t c = 0xFFFFFFFFUL;
    while (length--) {
      c ^= *p++;
      for (uint8_t bit = 0; bit < 8; ++bit) c = (c >> 1) ^ ((c & 1) ? 0xEDB88320UL : 0);
    }
    return ~c;
  }
  static bool blank(const uint8_t *p) {
    for (uint8_t i = 0; i < SIZE; ++i) if (p[i] != 0xFF) return false;
    return true;
  }
  static bool recognizable(const uint8_t *p) {
    // Prior example sketches/unformatted memory are not evidence of a lost
    // StudyPet record. A damaged magic string still counts as recognizable
    // when the commit marker and the rest of the format header are intact.
    return memcmp(p + 1, "SPDL", 4) == 0 ||
           (p[0] == COMMIT && p[5] == 1 && p[6] == MAX && p[7] == 64);
  }
  static bool valid(const uint8_t *p) {
    if (p[0] != COMMIT || memcmp(p + 1, "SPDL", 4) || p[5] != 1 || p[6] != MAX || p[7] != 64 ||
        crc(p + 1, 75) != get32(p + 76)) return false;
    for (uint8_t i = 0; i < MAX; ++i) {
      const uint8_t *d = p + 12 + i * 8;
      if (d[0] > 1) return false;
      if (d[0] && (d[1] >= SUBJECT_COUNT || d[2] >= KIND_COUNT || d[3] >= LEAD_COUNT ||
                   get32(d + 4) < 757382400UL || get32(d + 4) > 3155759999UL)) return false;
    }
    return true;
  }
  bool scan(uint8_t *a, uint8_t *b, int8_t &active) {
    active = -1;
    if (!read(DEADLINE_EEPROM_OFFSET, a, SIZE) || !read(DEADLINE_EEPROM_OFFSET + 128, b, SIZE)) {
      state_ = IO_ERROR; return false;
    }
    const bool va = valid(a), vb = valid(b);
    if (va && vb) active = int32_t(get32(b + 8) - get32(a + 8)) > 0 ? 1 : 0;
    else if (va) active = 0;
    else if (vb) active = 1;
    if (active < 0) {
      if (blank(a) && blank(b)) state_ = EMPTY;
      else state_ = recognizable(a) || recognizable(b) ? CORRUPT : UNINITIALIZED;
    } else {
      // Untouched non-FF memory in the second slot is normal after the first
      // save on a previously used shield; it is not a failed backup record.
      state_ = (!va && recognizable(a)) || (!vb && recognizable(b)) ? RECOVERED : SAVED;
    }
    return true;
  }
  static bool read(uint16_t address, uint8_t *out, uint8_t length) {
    while (length) {
      const uint8_t n = length > 16 ? 16 : length;
      Wire.beginTransmission(DEADLINE_EEPROM_ADDRESS);
      Wire.write(uint8_t(address >> 8)); Wire.write(uint8_t(address));
      if (Wire.endTransmission(false) != 0 || Wire.requestFrom(uint8_t(DEADLINE_EEPROM_ADDRESS), n) != n) return false;
      for (uint8_t i = 0; i < n; ++i) *out++ = Wire.read();
      address += n; length -= n;
    }
    return true;
  }
  static bool write(uint16_t address, const uint8_t *data, uint8_t length) {
    while (length) {
      uint8_t n = length > 16 ? 16 : length;
      const uint8_t pageLeft = 128 - (address & 127);
      if (n > pageLeft) n = pageLeft;
      Wire.beginTransmission(DEADLINE_EEPROM_ADDRESS);
      Wire.write(uint8_t(address >> 8)); Wire.write(uint8_t(address));
      for (uint8_t i = 0; i < n; ++i) Wire.write(data[i]);
      if (Wire.endTransmission() != 0) return false;
      bool ready = false;
      for (uint8_t attempt = 0; attempt < 12; ++attempt) {
        Wire.beginTransmission(DEADLINE_EEPROM_ADDRESS);
        if (Wire.endTransmission() == 0) { ready = true; break; }
        delay(1); // bounded ACK polling; tWR <= 5 ms on 24FC1025
      }
      if (!ready) return false;
      address += n; data += n; length -= n;
    }
    return true;
  }
};
#endif
