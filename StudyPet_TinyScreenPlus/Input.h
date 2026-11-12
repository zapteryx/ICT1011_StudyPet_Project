#ifndef STUDYPET_INPUT_H
#define STUDYPET_INPUT_H

#include <Arduino.h>
#include <TinyScreen.h>

// =====================================================================
//  PIN DEFINITIONS
//  Override any of these by #defining them in the .ino ABOVE the
//  #include "Input.h" line. TinyScreen+ GPIO is 3.3 V ONLY - never feed
//  a 5 V sensor output straight into these pins.
// =====================================================================
#ifndef BUZZER_PIN
#define BUZZER_PIN A0          // piezo / buzzer signal pin
#endif

// 1 = passive piezo, driven with tone() (pitch is software controlled)
// 0 = active buzzer (has its own oscillator; simply switched HIGH/LOW)
#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE 1
#endif

#ifndef IR_SENSOR_PIN
#define IR_SENSOR_PIN A1       // digital OUT of the IR / PIR module
#endif

// PIR motion modules (AM312, HC-SR501):  output HIGH when someone is seen.
// IR obstacle / reflective boards (FC-51, TCRT5000): usually LOW when seen.
#ifndef IR_ACTIVE_LEVEL
#define IR_ACTIVE_LEVEL HIGH
#endif

#ifndef IR_USE_PULLUP
#define IR_USE_PULLUP 0        // set to 1 for open-collector sensor outputs
#endif

// Your TinyScreen+ uses the ASD2022 Wireling Adapter. Wireling.begin() powers
// it and initializes its I2C multiplexer. Set to 0 for directly wired parts.
#ifndef USE_WIRELING_ADAPTER
#define USE_WIRELING_ADAPTER 1
#endif

// IR_SENSOR_PIN is ONLY for a digital IR/PIR sensor. A thermal camera needs
// its own I2C driver and Wireling.selectPort(port); digitalRead(A1) cannot
// read temperatures or establish that a person is present.
#ifndef IR_USE_THERMAL_CAMERA
#define IR_USE_THERMAL_CAMERA 0  // optional legacy AST1045 mode
#endif
#ifndef PRESENCE_USE_TOF
#define PRESENCE_USE_TOF 1 // AST1024 / VL53L0X replaces the thermal camera
#endif
#ifndef TOF_SENSOR_PORT
#define TOF_SENSOR_PORT 1 // adapter socket number, not the A1 GPIO value
#endif
#ifndef TOF_PRESENCE_MAX_MM
#define TOF_PRESENCE_MAX_MM 1000 // exactly 1 metre, inclusive
#endif
#if PRESENCE_USE_TOF
#include "DistanceSensor.h"
#endif
#ifndef THERMAL_CAMERA_PORT
#define THERMAL_CAMERA_PORT 1  // adapter socket number, NOT the A1 GPIO value
#endif
#ifndef THERMAL_MIN_QUARTER_C
#define THERMAL_MIN_QUARTER_C 112  // 28 C
#endif
#ifndef THERMAL_MAX_QUARTER_C
#define THERMAL_MAX_QUARTER_C 160  // 40 C
#endif
#ifndef THERMAL_CONTRAST_QUARTER_C
#define THERMAL_CONTRAST_QUARTER_C 10  // 2.5 C above median background
#endif
#ifndef THERMAL_MIN_PIXELS
#define THERMAL_MIN_PIXELS 3
#endif
#if IR_USE_THERMAL_CAMERA && !PRESENCE_USE_TOF
#include "ThermalCamera.h"
#endif

// =====================================================================
//  BUTTONS -> NAVIGATION KEYS
//    Top Right    = UP        Bottom Right = DOWN
//    Top Left     = BACK      Bottom Left  = SELECT
// =====================================================================
enum NavKey : uint8_t {
  KEY_NONE = 0,
  KEY_UP,      // top right
  KEY_DOWN,    // bottom right
  KEY_BACK,    // top left
  KEY_SELECT   // bottom left
};

enum PressType : uint8_t {
  PRESS_SHORT = 0,  // fired on release, if the button was not held long
  PRESS_LONG,       // fired once after LONG_PRESS_MS of holding
  PRESS_REPEAT      // fired repeatedly while still held after a long press
};

struct InputEvent {
  NavKey key;
  PressType type;
};

class ButtonInput {
public:
  static const unsigned long DEBOUNCE_MS = 30;
  static const unsigned long LONG_PRESS_MS = 650;
  static const unsigned long REPEAT_DELAY_MS = 220;
  static const unsigned long REPEAT_EVERY_MS = 110;

  void begin(unsigned long now) {
    lastRaw_ = 0;
    stable_ = 0;
    changedAt_ = now;
    held_ = 0;
    heldKey_ = KEY_NONE;
    longFired_ = false;
  }

  // Call every loop. Returns at most one event (KEY_NONE when nothing).
  InputEvent update(TinyScreen &display, unsigned long now) {
    InputEvent ev;
    ev.key = KEY_NONE;
    ev.type = PRESS_SHORT;

    const uint8_t raw = display.getButtons() & ALL_MASK;
    if (raw != lastRaw_) {
      lastRaw_ = raw;
      changedAt_ = now;
    }

    // Debounced edge detection.
    if ((unsigned long)(now - changedAt_) >= DEBOUNCE_MS && raw != stable_) {
      const uint8_t pressed = raw & static_cast<uint8_t>(~stable_);
      const uint8_t released = stable_ & static_cast<uint8_t>(~raw);
      stable_ = raw;

      // Release of the tracked button -> short press (unless it went long).
      if (held_ && (released & held_)) {
        if (!longFired_) {
          ev.key = heldKey_;
          ev.type = PRESS_SHORT;
        }
        held_ = 0;
        heldKey_ = KEY_NONE;
      }
      // Track one button at a time; others pressed meanwhile are ignored.
      if (!held_ && pressed) {
        held_ = firstMask(pressed);
        heldKey_ = maskToKey(held_);
        heldAt_ = now;
        longFired_ = false;
      }
      if (ev.key != KEY_NONE) return ev;
    }

    if (held_) {
      if (!longFired_ && (unsigned long)(now - heldAt_) >= LONG_PRESS_MS) {
        longFired_ = true;
        nextRepeat_ = now + REPEAT_DELAY_MS;
        ev.key = heldKey_;
        ev.type = PRESS_LONG;
        return ev;
      }
      if (longFired_ && (long)(now - nextRepeat_) >= 0) {
        nextRepeat_ += REPEAT_EVERY_MS;
        ev.key = heldKey_;
        ev.type = PRESS_REPEAT;
        return ev;
      }
    }
    return ev;
  }

  static const char *keyName(NavKey k) {
    switch (k) {
      case KEY_UP: return "UP";
      case KEY_DOWN: return "DOWN";
      case KEY_BACK: return "BACK";
      case KEY_SELECT: return "SELECT";
      default: return "-";
    }
  }

private:
  static const uint8_t ALL_MASK = TSButtonUpperLeft | TSButtonUpperRight |
                                  TSButtonLowerLeft | TSButtonLowerRight;

  static uint8_t firstMask(uint8_t m) {
    if (m & TSButtonUpperRight) return TSButtonUpperRight;
    if (m & TSButtonLowerRight) return TSButtonLowerRight;
    if (m & TSButtonUpperLeft) return TSButtonUpperLeft;
    if (m & TSButtonLowerLeft) return TSButtonLowerLeft;
    return 0;
  }

  static NavKey maskToKey(uint8_t m) {
    if (m & TSButtonUpperRight) return KEY_UP;
    if (m & TSButtonLowerRight) return KEY_DOWN;
    if (m & TSButtonUpperLeft) return KEY_BACK;
    if (m & TSButtonLowerLeft) return KEY_SELECT;
    return KEY_NONE;
  }

  uint8_t lastRaw_ = 0;
  uint8_t stable_ = 0;
  unsigned long changedAt_ = 0;
  uint8_t held_ = 0;
  NavKey heldKey_ = KEY_NONE;
  unsigned long heldAt_ = 0;
  bool longFired_ = false;
  unsigned long nextRepeat_ = 0;
};

// =====================================================================
//  PRESENCE SENSOR: AST1024 TOF by default; legacy thermal/digital optional.
//  - Detection must be stable for CONFIRM_MS before it counts (filters
//    glitches).
//  - TOF: > 1 m / invalid data immediately counts as AWAY. No old timeout.
//  - Legacy sensors: the user is only considered AWAY after `timeout` ms with no
//    detection. PIR sensors report MOTION, not presence, so someone
//    sitting very still can drop out briefly; a timeout of 1-2 min is a
//    good starting point for PIR. Reflective IR boards can use less.
// =====================================================================
class PresenceSensor {
public:
  static const unsigned long CONFIRM_MS = 150;

  void begin(uint8_t pin, uint8_t activeLevel, bool pullup, unsigned long now) {
    pin_ = pin;
    activeLevel_ = activeLevel;
#if PRESENCE_USE_TOF
    (void)pullup;
#if !USE_WIRELING_ADAPTER
    Wire.begin();
#endif
    distance.begin(now);
#elif IR_USE_THERMAL_CAMERA
    (void)pullup;
#if !USE_WIRELING_ADAPTER
    Wire.begin();
#endif
    camera.begin(now);
#else
    pinMode(pin_, pullup ? INPUT_PULLUP : INPUT);
#endif
    present_ = !PRESENCE_USE_TOF; // TOF waits for a confirmed nearby target
    lastSeen_ = now;
    detectSince_ = now;
    rawDetect_ = false;
  }

  void setEnabled(bool on, unsigned long now) {
    enabled_ = on;
    present_ = !on || !PRESENCE_USE_TOF;
    lastSeen_ = now;
    rawDetect_ = false;
    detectSince_ = now;
  }

  void setAbsentTimeout(unsigned long ms) { timeoutMs_ = ms; }

  // Returns true when the present/away state changed on this call.
  bool update(unsigned long now) {
    const bool before = present_;
    if (!enabled_) {
      present_ = true;
      rawDetect_ = false;
      return before != present_;
    }
#if PRESENCE_USE_TOF
    distance.update(now);
    const bool detect = distance.valid() && distance.millimeters() <= TOF_PRESENCE_MAX_MM;
    if (detect && !rawDetect_) detectSince_ = now;
    rawDetect_ = detect;
    if (!detect) present_ = false; // no old away timeout beyond 1 metre
    else if ((unsigned long)(now - detectSince_) >= CONFIRM_MS) present_ = true;
    return before != present_;
#elif IR_USE_THERMAL_CAMERA
    camera.update(now);
    if (!camera.valid() || camera.warming()) {
      // Missing/error/warming camera is unknown, not proof of absence.
      // Keep timers usable and expose the problem in Settings.
      present_ = true; lastSeen_ = now; rawDetect_ = false;
      return before != present_;
    }
    const bool detect = camera.warmTarget();
#else
    const bool detect = (digitalRead(pin_) == activeLevel_);
#endif
#if !PRESENCE_USE_TOF
    if (detect && !rawDetect_) detectSince_ = now;
    rawDetect_ = detect;

    if (detect && (unsigned long)(now - detectSince_) >= CONFIRM_MS) {
      lastSeen_ = now;
      present_ = true;  // resume immediately when the user returns
    } else if (present_ && (unsigned long)(now - lastSeen_) >= timeoutMs_) {
      present_ = false;
    }
    return before != present_;
#endif
  }

  bool present() const { return present_; }
  bool enabled() const { return enabled_; }
  bool rawDetect() const { return rawDetect_; }
#if PRESENCE_USE_TOF
  DistanceSensor distance;
#elif IR_USE_THERMAL_CAMERA
  ThermalCamera camera;
#endif

private:
  uint8_t pin_ = IR_SENSOR_PIN;
  uint8_t activeLevel_ = IR_ACTIVE_LEVEL;
  bool enabled_ = true;
  bool present_ = true;
  bool rawDetect_ = false;
  unsigned long lastSeen_ = 0;
  unsigned long detectSince_ = 0;
  unsigned long timeoutMs_ = 60000UL;
};

#endif
