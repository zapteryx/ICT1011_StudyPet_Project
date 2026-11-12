#ifndef STUDYPET_INPUT_H
#define STUDYPET_INPUT_H

#include <Arduino.h>
#include <TinyScreen.h>
#include "StudyPetConfig.h"

// Hardware options are shared by the sketch and BLE source file. Set them
// in StudyPetConfig.h. TinyScreen+ GPIO is 3.3 V; digital IR/PIR mode expects
// a HIGH/LOW signal, while TOF/thermal modules communicate over I2C.
#if PRESENCE_USE_TOF
#include "DistanceSensor.h"
#elif IR_USE_THERMAL_CAMERA
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
