#ifndef STUDYPET_THERMAL_CAMERA_H
#define STUDYPET_THERMAL_CAMERA_H
#include <Arduino.h>
#include <Wire.h>
#if USE_WIRELING_ADAPTER
#include <Wireling.h>
#endif

// AST1045 / AMG8833. Register layout and quarter-degree pixel conversion:
// https://learn.tinycircuits.com/Wirelings/Thermal-Camera_Wireling_Tutorial/
// https://github.com/adafruit/Adafruit_AMG88xx
// This device measures surface temperatures, NOT distance or identity.
class ThermalCamera {
public:
  void begin(unsigned long now) {
    ready_ = false; valid_ = false; warm_ = false;
    lastPoll_ = now; retryAt_ = now + 250UL;
  }
  void setPort(uint8_t port, unsigned long now) { port_ = port; begin(now); }
  uint8_t port() const { return port_; }
  bool valid() const { return valid_; }
  bool warming() const { return valid_ && (unsigned long)(lastPoll_ - initializedAt_) < 15000UL; }
  bool warmTarget() const { return valid_ && warm_; }
  int16_t peakQuarterC() const { return peak_; }

  void update(unsigned long now) {
    if (!ready_) {
      if ((long)(now - retryAt_) < 0) return;
      retryAt_ = now + 2000UL;
      selectPort();
      address_ = 0x69;
      if (!ack()) { address_ = 0x68; if (!ack()) return; }
      // Normal mode, initial reset, no interrupt, 10 frames/sec.
      if (!writeRegister(0x00, 0x00) || !writeRegister(0x01, 0x3F) ||
          !writeRegister(0x03, 0x00) || !writeRegister(0x02, 0x00)) return;
      ready_ = true; initializedAt_ = now; lastPoll_ = now;
      return; // give the sensor time to generate its first frame
    }
    if ((unsigned long)(now - lastPoll_) < 250UL) return;
    lastPoll_ = now;
    selectPort();
    int16_t pixels[64];
    // Small reads fit Arduino Wire implementations with a 32-byte buffer.
    for (uint8_t start = 0; start < 64; start += 8) {
      Wire.beginTransmission(address_); Wire.write(uint8_t(0x80 + start * 2));
      if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address_, uint8_t(16)) != 16) {
        fail(now); return;
      }
      for (uint8_t i = 0; i < 8; ++i) {
        const uint8_t low = Wire.read(), high = Wire.read();
        int16_t value = (uint16_t(high & 0x0F) << 8) | low;
        if (value & 0x0800) value -= 4096; // signed 12-bit, 0.25 C/LSB
        if (value < -160 || value > 500) { fail(now); return; }
        pixels[start + i] = value;
      }
    }
    // Sort this local frame to obtain a background estimate and warmest value.
    for (uint8_t i = 1; i < 64; ++i) {
      const int16_t v = pixels[i]; uint8_t j = i;
      while (j && pixels[j - 1] > v) { pixels[j] = pixels[j - 1]; --j; }
      pixels[j] = v;
    }
    peak_ = pixels[63];
    const int16_t background = (pixels[31] + pixels[32]) / 2;
    uint8_t warmPixels = 0;
    for (uint8_t i = 0; i < 64; ++i)
      if (pixels[i] >= THERMAL_MIN_QUARTER_C && pixels[i] <= THERMAL_MAX_QUARTER_C &&
          pixels[i] >= background + THERMAL_CONTRAST_QUARTER_C) ++warmPixels;
    warm_ = warmPixels >= THERMAL_MIN_PIXELS;
    valid_ = true;
  }
private:
  uint8_t port_ = THERMAL_CAMERA_PORT, address_ = 0x69;
  bool ready_ = false, valid_ = false, warm_ = false;
  int16_t peak_ = 0;
  unsigned long lastPoll_ = 0, retryAt_ = 0, initializedAt_ = 0;
  void selectPort() {
#if USE_WIRELING_ADAPTER
    Wireling.selectPort(port_);
#endif
  }
  bool ack() { Wire.beginTransmission(address_); return Wire.endTransmission() == 0; }
  bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address_); Wire.write(reg); Wire.write(value);
    return Wire.endTransmission() == 0;
  }
  void fail(unsigned long now) {
    valid_ = false; warm_ = false; ready_ = false; retryAt_ = now + 2000UL;
  }
};
#endif
