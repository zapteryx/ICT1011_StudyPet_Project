#ifndef STUDYPET_DISTANCE_SENSOR_H
#define STUDYPET_DISTANCE_SENSOR_H
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h> // Pololu VL53L0X library, available in Library Manager
#if USE_WIRELING_ADAPTER
#include <Wireling.h>
#endif

// AST1024 uses VL53L0X at the 7-bit I2C address 0x29. Pololu handles
// calibration/setup; checked, non-blocking register reads handle live frames.
class DistanceSensor {
public:
  enum State : uint8_t { STARTING, ONLINE, NO_TARGET, SENSOR_ERROR };
  void begin(unsigned long now) {
    initialized_ = false; valid_ = false; state_ = STARTING;
    retryAt_ = now + 250UL; lastFrame_ = lastPoll_ = now;
  }
  void setPort(uint8_t port, unsigned long now) {
    if (initialized_) { selectPort(); sensor_.stopContinuous(); }
    port_ = port; begin(now);
  }
  uint8_t port() const { return port_; }
  bool valid() const { return valid_; }
  uint16_t millimeters() const { return mm_; }
  State state() const { return state_; }
  void update(unsigned long now) {
    if (!initialized_) {
      if ((long)(now - retryAt_) < 0) return;
      retryAt_ = now + 2000UL; selectPort();
      sensor_.setTimeout(100);
      if (!sensor_.init() || !sensor_.setMeasurementTimingBudget(33000)) { fail(now); return; }
      sensor_.startContinuous(100);
      if (sensor_.last_status != 0) { fail(now); return; }
      initialized_ = true; lastFrame_ = lastPoll_ = now;
      return;
    }
    if ((unsigned long)(now - lastPoll_) < 50UL) return;
    lastPoll_ = now; selectPort();
    uint8_t ready;
    if (!read(0x13, &ready, 1)) { fail(now); return; }
    if (!(ready & 7)) {
      // Never keep a stale near reading if the sensor stops producing frames.
      if ((unsigned long)(now - lastFrame_) >= 400UL) fail(now);
      return;
    }
    uint8_t frame[12];
    if (!read(0x14, frame, sizeof frame) || !write(0x0B, 1)) { fail(now); return; }
    lastFrame_ = now;
    mm_ = (uint16_t(frame[10]) << 8) | frame[11];
    // ST's device range-complete status is 11 (not the PAL status code 0).
    const uint8_t status = (frame[0] & 0x78) >> 3;
    valid_ = status == 11 && mm_ >= 20 && mm_ <= 2000;
    state_ = valid_ ? ONLINE : NO_TARGET;
  }
private:
  VL53L0X sensor_;
  uint8_t port_ = TOF_SENSOR_PORT;
  uint16_t mm_ = 0;
  bool initialized_ = false, valid_ = false;
  State state_ = STARTING;
  unsigned long retryAt_ = 0, lastFrame_ = 0, lastPoll_ = 0;
  void selectPort() {
#if USE_WIRELING_ADAPTER
    Wireling.selectPort(port_);
#endif
  }
  bool read(uint8_t reg, uint8_t *data, uint8_t length) {
    Wire.beginTransmission(0x29); Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(uint8_t(0x29), length) != length) return false;
    for (uint8_t i = 0; i < length; ++i) data[i] = Wire.read();
    return true;
  }
  bool write(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(0x29); Wire.write(reg); Wire.write(value);
    return Wire.endTransmission() == 0;
  }
  void fail(unsigned long now) {
    initialized_ = false; valid_ = false; state_ = SENSOR_ERROR; retryAt_ = now + 2000UL;
  }
};
#endif
