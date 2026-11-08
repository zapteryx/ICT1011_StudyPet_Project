#ifndef STUDYPET_FEATURES_H
#define STUDYPET_FEATURES_H

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include "../Input.h"

// =====================================================================
//  USER-EDITABLE LISTS
// =====================================================================
// Subjects for the study tracker and deadlines (max 8; keep names <= 8 chars
// so menu rows fit on the 96 px screen).
static const uint8_t SUBJECT_COUNT = 5;
static const char *const SUBJECT_NAMES[SUBJECT_COUNT] = {
  "INF1102", "INF1103", "INF1104", "Revision", "Other"
};

static const uint8_t KIND_COUNT = 6;
static const char *const KIND_NAMES[KIND_COUNT] = {
  "Assignment", "Quiz", "Exam", "Project", "Lab", "Other"
};
static const char *const KIND_SHORT[KIND_COUNT] = {
  "Asgn", "Quiz", "Exam", "Proj", "Lab", "Misc"
};

// When the visual reminder (and the first long beep) starts, per deadline.
static const uint8_t LEAD_COUNT = 9;
static const uint16_t LEAD_MINUTES[LEAD_COUNT] = {
  0, 60, 180, 360, 720, 1440, 2880, 4320, 10080
};
static const char *const LEAD_LABELS[LEAD_COUNT] = {
  "at due", "1h before", "3h before", "6h before", "12h before",
  "1d before", "2d before", "3d before", "1w before"
};

// How long without IR detection before the user counts as away.
static const uint8_t AWAY_COUNT = 5;
static const uint16_t AWAY_SECONDS[AWAY_COUNT] = {10, 30, 60, 120, 300};
static const char *const AWAY_LABELS[AWAY_COUNT] = {
  "10 s", "30 s", "1 min", "2 min", "5 min"
};

// Re-beep an unacknowledged deadline alert every N minutes (0 = never).
static const uint8_t REPEAT_COUNT = 4;
static const uint16_t REPEAT_MINUTES[REPEAT_COUNT] = {0, 15, 30, 60};
static const char *const REPEAT_LABELS[REPEAT_COUNT] = {
  "Off", "15 min", "30 min", "60 min"
};

// =====================================================================
//  BUZZER  (non-blocking pattern player)
//  Short beeps  = break timer.   Long beeps = deadlines.
// =====================================================================
struct BeepPattern {
  uint16_t onMs;
  uint16_t offMs;
  uint8_t count;
  uint16_t freqHz;   // used only with a passive piezo
  uint8_t priority;  // a lower-priority pattern never interrupts a higher one
};

static const BeepPattern BEEP_BREAK_START      = {120, 110, 3, 2600, 1};
static const BeepPattern BEEP_FOCUS_START      = {150,   0, 1, 2000, 1};
static const BeepPattern BEEP_CYCLE_DONE       = {100,  90, 2, 3000, 1};
static const BeepPattern BEEP_DEADLINE_SOON    = {650, 350, 2, 1500, 2};
static const BeepPattern BEEP_DEADLINE_OVERDUE = {900, 400, 3, 1200, 3};

class Buzzer {
public:
  void begin(uint8_t pin, bool passive) {
    pin_ = pin;
    passive_ = passive;
    pinMode(pin_, OUTPUT);
    off();
  }

  void setEnabled(bool on) {
    enabled_ = on;
    if (!on) stop();
  }

  bool busy() const { return on_ || remaining_ > 0; }

  void play(const BeepPattern &p, unsigned long now) {
    if (!enabled_) return;
    if (busy() && p.priority < pat_.priority) return;
    off();
    pat_ = p;
    remaining_ = p.count;
    startBeep(now);
  }

  void stop() {
    off();
    remaining_ = 0;
  }

  void update(unsigned long now) {
    if (!busy()) return;
    if (on_) {
      if ((unsigned long)(now - changedAt_) >= pat_.onMs) {
        off();
        changedAt_ = now;
      }
    } else if (remaining_ > 0 && (unsigned long)(now - changedAt_) >= pat_.offMs) {
      startBeep(now);
    }
  }

private:
  uint8_t pin_ = BUZZER_PIN;
  bool passive_ = true;
  bool enabled_ = true;
  bool on_ = false;
  uint8_t remaining_ = 0;
  unsigned long changedAt_ = 0;
  BeepPattern pat_ = {0, 0, 0, 0, 0};

  void startBeep(unsigned long now) {
    if (remaining_ == 0) return;
    remaining_--;
    on_ = true;
    changedAt_ = now;
    if (passive_) tone(pin_, pat_.freqHz);
    else digitalWrite(pin_, HIGH);
  }

  void off() {
    if (passive_) noTone(pin_);
    digitalWrite(pin_, LOW);
    on_ = false;
  }
};

// =====================================================================
//  SOFTWARE CLOCK  (seconds since 2000-01-01 00:00, driven by millis())
//  Starts at the sketch's compile time; set it from Settings > Set date.
// =====================================================================
struct DateTime {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
};

namespace TimeUtil {
static const char *const MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char *const WEEKDAYS[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                        "Thursday", "Friday", "Saturday"};
static const int32_t DAYS_1970_TO_2000 = 10957;

inline bool isLeap(uint16_t y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }

inline uint8_t daysInMonth(uint16_t y, uint8_t m) {
  static const uint8_t dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && isLeap(y)) return 29;
  return dim[(m - 1) % 12];
}

// Howard Hinnant's civil-date algorithms (days since 1970-01-01).
inline int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= (m <= 2);
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

inline void civilFromDays(int32_t z, uint16_t &y, uint8_t &m, uint8_t &d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = (uint32_t)(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t yy = (int32_t)yoe + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  d = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
  m = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
  y = (uint16_t)(yy + (m <= 2));
}

inline uint32_t toEpoch(const DateTime &t) {
  const int32_t days = daysFromCivil(t.year, t.month, t.day) - DAYS_1970_TO_2000;
  return (uint32_t)days * 86400UL + t.hour * 3600UL + t.minute * 60UL + t.second;
}

inline DateTime fromEpoch(uint32_t e) {
  DateTime t;
  civilFromDays((int32_t)(e / 86400UL) + DAYS_1970_TO_2000, t.year, t.month, t.day);
  const uint32_t s = e % 86400UL;
  t.hour = s / 3600UL;
  t.minute = (s / 60UL) % 60UL;
  t.second = s % 60UL;
  return t;
}

inline uint8_t weekday(uint32_t e) { return (uint8_t)(((e / 86400UL) + 6UL) % 7UL); }  // 2000-01-01 = Sat
}  // namespace TimeUtil

class SoftClock {
public:
  void begin(unsigned long now) { set(TimeUtil::toEpoch(compileTime()), now); }

  void set(uint32_t epoch, unsigned long now) {
    epoch_ = epoch;
    baseMillis_ = now;
  }

  // Rebases every second, so millis() overflow (49 days) is harmless.
  void update(unsigned long now) {
    const unsigned long elapsed = now - baseMillis_;
    if (elapsed >= 1000UL) {
      const uint32_t secs = elapsed / 1000UL;
      epoch_ += secs;
      baseMillis_ += secs * 1000UL;
    }
  }

  uint32_t epoch() const { return epoch_; }

  static DateTime compileTime() {
    const char *d = __DATE__;  // "Nov  5 2026"
    const char *t = __TIME__;  // "14:05:33"
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    DateTime dt;
    dt.month = 1;
    for (uint8_t i = 0; i < 12; ++i)
      if (strncmp(d, months + 3 * i, 3) == 0) dt.month = i + 1;
    dt.day = (uint8_t)(((d[4] == ' ') ? 0 : (d[4] - '0')) * 10 + (d[5] - '0'));
    dt.year = (uint16_t)atoi(d + 7);
    dt.hour = (uint8_t)((t[0] - '0') * 10 + (t[1] - '0'));
    dt.minute = (uint8_t)((t[3] - '0') * 10 + (t[4] - '0'));
    dt.second = (uint8_t)((t[6] - '0') * 10 + (t[7] - '0'));
    return dt;
  }

private:
  uint32_t epoch_ = 0;
  unsigned long baseMillis_ = 0;
};

// =====================================================================
//  BREAK TIMER  (Pomodoro / 90-minute / custom)
//  focus -> break -> focus -> ... -> focus N -> long break -> reset (idle)
// =====================================================================
enum TimerStyle : uint8_t { STYLE_POMODORO = 0, STYLE_DEEP90, STYLE_CUSTOM, STYLE_COUNT };
enum TimerPhase : uint8_t { PHASE_IDLE = 0, PHASE_FOCUS, PHASE_SHORT_BREAK, PHASE_LONG_BREAK };
enum TimerEvent : uint8_t {
  TEV_NONE = 0, TEV_FOCUS_STARTED, TEV_BREAK_STARTED, TEV_LONG_BREAK_STARTED, TEV_CYCLE_COMPLETE
};
enum TimerField : uint8_t { TF_FOCUS = 0, TF_SHORT, TF_LONG, TF_CYCLES };

struct TimerConfig {
  uint8_t focusMin;
  uint8_t shortMin;
  uint8_t longMin;
  uint8_t cycles;  // focus blocks before the long break
};

static const TimerConfig TIMER_PRESETS[2] = {
  {25, 5, 15, 4},   // Pomodoro: 4 x (25 + 5), then 15 min long break
  {90, 20, 30, 2},  // 90-minute deep-work blocks
};
static const char *const STYLE_NAMES[STYLE_COUNT] = {"Pomodoro", "90-min", "Custom"};

class BreakTimer {
public:
  void begin(unsigned long now) {
    custom_ = TIMER_PRESETS[STYLE_POMODORO];
    setStyle(STYLE_POMODORO);
    reset();
    lastTick_ = now;
  }

  void setStyle(TimerStyle s) {
    style_ = s;
    cfg_ = (s == STYLE_CUSTOM) ? custom_ : TIMER_PRESETS[s];
  }
  void cycleStyle() { setStyle((TimerStyle)((style_ + 1) % STYLE_COUNT)); }

  // Editing any value switches to the Custom style (keeps the other values).
  void setField(TimerField f, uint8_t v) {
    custom_ = cfg_;
    switch (f) {
      case TF_FOCUS: custom_.focusMin = v; break;
      case TF_SHORT: custom_.shortMin = v; break;
      case TF_LONG: custom_.longMin = v; break;
      case TF_CYCLES: custom_.cycles = v; break;
    }
    setStyle(STYLE_CUSTOM);
  }

  void start(unsigned long now) {
    if (phase_ == PHASE_IDLE) {
      cycle_ = 1;
      enter(PHASE_FOCUS);
    }
    userPaused_ = false;
    lastTick_ = now;
  }
  void pause() { if (phase_ != PHASE_IDLE) userPaused_ = true; }
  void toggle(unsigned long now) {
    if (phase_ == PHASE_IDLE || userPaused_) start(now);
    else pause();
  }
  void reset() {
    phase_ = PHASE_IDLE;
    cycle_ = 1;
    userPaused_ = false;
    remainingMs_ = 0;
  }
  TimerEvent skip() { return (phase_ == PHASE_IDLE) ? TEV_NONE : advance(); }

  // Set by the IR presence logic. Only holds FOCUS time - breaks keep
  // counting down while you are away (that's what breaks are for).
  void setAutoPaused(bool p) { autoPaused_ = p; }

  TimerEvent update(unsigned long now) {
    const unsigned long elapsed = now - lastTick_;
    lastTick_ = now;
    if (!counting()) return TEV_NONE;
    if (elapsed >= remainingMs_) return advance();
    remainingMs_ -= elapsed;
    return TEV_NONE;
  }

  bool counting() const {
    return phase_ != PHASE_IDLE && !userPaused_ && !(autoPaused_ && phase_ == PHASE_FOCUS);
  }
  bool awayHeld() const { return phase_ == PHASE_FOCUS && !userPaused_ && autoPaused_; }
  bool userPaused() const { return userPaused_; }
  bool onBreak() const { return phase_ == PHASE_SHORT_BREAK || phase_ == PHASE_LONG_BREAK; }
  TimerPhase phase() const { return phase_; }
  TimerStyle style() const { return style_; }
  uint8_t cycle() const { return cycle_; }
  const TimerConfig &config() const { return cfg_; }
  uint32_t remainingSeconds() const { return (remainingMs_ + 999UL) / 1000UL; }

private:
  TimerStyle style_ = STYLE_POMODORO;
  TimerConfig cfg_ = {25, 5, 15, 4};
  TimerConfig custom_ = {25, 5, 15, 4};
  TimerPhase phase_ = PHASE_IDLE;
  uint8_t cycle_ = 1;
  bool userPaused_ = false;
  bool autoPaused_ = false;
  uint32_t remainingMs_ = 0;
  unsigned long lastTick_ = 0;

  void enter(TimerPhase p) {
    phase_ = p;
    uint8_t mins = 0;
    if (p == PHASE_FOCUS) mins = cfg_.focusMin;
    else if (p == PHASE_SHORT_BREAK) mins = cfg_.shortMin;
    else if (p == PHASE_LONG_BREAK) mins = cfg_.longMin;
    remainingMs_ = (uint32_t)mins * 60000UL;
  }

  TimerEvent advance() {
    switch (phase_) {
      case PHASE_FOCUS:
        if (cycle_ >= cfg_.cycles) {
          enter(PHASE_LONG_BREAK);
          return TEV_LONG_BREAK_STARTED;
        }
        enter(PHASE_SHORT_BREAK);
        return TEV_BREAK_STARTED;
      case PHASE_SHORT_BREAK:
        cycle_++;
        enter(PHASE_FOCUS);
        return TEV_FOCUS_STARTED;
      case PHASE_LONG_BREAK:
        reset();  // full cycle finished -> back to idle, cycle 1
        return TEV_CYCLE_COMPLETE;
      default:
        return TEV_NONE;
    }
  }
};

// =====================================================================
//  STUDY SESSION TRACKER  (per-subject totals, IR auto-pause)
// =====================================================================
class StudyTracker {
public:
  void begin(unsigned long now) {
    resetTotals();
    active_ = false;
    lastTick_ = now;
  }

  void start(uint8_t subject, unsigned long now) {
    if (subject >= SUBJECT_COUNT) return;
    if (active_) stop(now);
    subject_ = subject;
    lastSubject_ = subject;
    sessionSec_ = 0;
    carryMs_ = 0;
    active_ = true;
    lastTick_ = now;
  }

  void stop(unsigned long now) {
    update(now);
    active_ = false;
  }

  void setPresent(bool p) { present_ = p; }

  void update(unsigned long now) {
    const unsigned long elapsed = now - lastTick_;
    lastTick_ = now;
    if (!active_ || !present_) return;  // away -> time is not counted
    carryMs_ += elapsed;
    if (carryMs_ >= 1000UL) {
      const uint32_t s = carryMs_ / 1000UL;
      carryMs_ -= s * 1000UL;
      totals_[subject_] += s;
      sessionSec_ += s;
    }
  }

  void resetTotals() {
    for (uint8_t i = 0; i < SUBJECT_COUNT; ++i) totals_[i] = 0;
    sessionSec_ = 0;
  }

  bool active() const { return active_; }
  bool awayPaused() const { return active_ && !present_; }
  uint8_t subject() const { return subject_; }
  uint8_t lastSubject() const { return lastSubject_; }
  uint32_t sessionSeconds() const { return sessionSec_; }
  uint32_t totalSeconds(uint8_t i) const { return i < SUBJECT_COUNT ? totals_[i] : 0; }
  uint32_t grandTotal() const {
    uint32_t t = 0;
    for (uint8_t i = 0; i < SUBJECT_COUNT; ++i) t += totals_[i];
    return t;
  }

private:
  uint32_t totals_[SUBJECT_COUNT];
  bool active_ = false;
  bool present_ = true;
  uint8_t subject_ = 0;
  uint8_t lastSubject_ = 0;
  uint32_t sessionSec_ = 0;
  uint32_t carryMs_ = 0;
  unsigned long lastTick_ = 0;
};

// =====================================================================
//  DEADLINES  (CRUD storage + reminder logic)
// =====================================================================
enum DeadlineStatus : uint8_t { DL_UPCOMING = 0, DL_SOON = 1, DL_OVERDUE = 2 };

struct Deadline {
  bool used;
  uint8_t subject;
  uint8_t kind;
  uint8_t lead;       // index into LEAD_MINUTES
  uint32_t due;       // SoftClock epoch seconds
  uint8_t stage;      // highest status already alerted
  bool acked;         // user dismissed the pop-up for this stage
  bool popup;         // pop-up waiting to be shown
  uint32_t lastAlert;
};

class DeadlineManager {
public:
  static const uint8_t MAX = 8;

  void begin() { memset(items_, 0, sizeof(items_)); }

  uint8_t count() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX; ++i) n += items_[i].used ? 1 : 0;
    return n;
  }
  bool full() const { return count() >= MAX; }
  const Deadline &get(uint8_t i) const { return items_[i % MAX]; }

  // CREATE
  int8_t add(const Deadline &d, uint32_t now) {
    for (uint8_t i = 0; i < MAX; ++i) {
      if (!items_[i].used) {
        items_[i] = d;
        items_[i].used = true;
        settle(i, now);
        return (int8_t)i;
      }
    }
    return -1;
  }
  // UPDATE
  void replace(uint8_t i, const Deadline &d, uint32_t now) {
    if (i >= MAX || !items_[i].used) return;
    items_[i] = d;
    items_[i].used = true;
    settle(i, now);
  }
  // DELETE
  void remove(uint8_t i) {
    if (i < MAX) memset(&items_[i], 0, sizeof(Deadline));
  }

  DeadlineStatus status(uint8_t i, uint32_t now) const {
    const Deadline &d = items_[i];
    if (now >= d.due) return DL_OVERDUE;
    if (d.due - now <= (uint32_t)LEAD_MINUTES[d.lead] * 60UL) return DL_SOON;
    return DL_UPCOMING;
  }

  // READ helper: indices of used slots sorted by due time. Returns count.
  uint8_t sorted(uint8_t *out) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX; ++i) {
      if (!items_[i].used) continue;
      uint8_t j = n++;
      while (j > 0 && items_[out[j - 1]].due > items_[i].due) {
        out[j] = out[j - 1];
        --j;
      }
      out[j] = i;
    }
    return n;
  }

  // Overdue first, else the soonest deadline inside its reminder window.
  int8_t mostUrgent(uint32_t now) const {
    int8_t best = -1;
    uint8_t bestSt = 0;
    for (uint8_t i = 0; i < MAX; ++i) {
      if (!items_[i].used) continue;
      const uint8_t st = status(i, now);
      if (st == DL_UPCOMING) continue;
      if (best < 0 || st > bestSt || (st == bestSt && items_[i].due < items_[best].due)) {
        best = (int8_t)i;
        bestSt = st;
      }
    }
    return best;
  }

  // Call about once a second. Returns the loudest NEW alert (0 = none).
  uint8_t update(uint32_t now, uint16_t repeatMin) {
    uint8_t loudest = 0;
    for (uint8_t i = 0; i < MAX; ++i) {
      Deadline &d = items_[i];
      if (!d.used) continue;
      const uint8_t st = status(i, now);
      if (st > d.stage) {
        d.stage = st;
        d.acked = false;
        d.popup = true;
        d.lastAlert = now;
        if (st > loudest) loudest = st;
      } else if (st < d.stage) {
        d.stage = st;  // clock moved back / due date pushed later
      } else if (st != DL_UPCOMING && !d.acked && repeatMin > 0 &&
                 now - d.lastAlert >= (uint32_t)repeatMin * 60UL) {
        d.lastAlert = now;
        d.popup = true;
        if (st > loudest) loudest = st;
      }
    }
    return loudest;
  }

  int8_t nextPopup() const {
    for (uint8_t i = 0; i < MAX; ++i)
      if (items_[i].used && items_[i].popup) return (int8_t)i;
    return -1;
  }

  void acknowledge(uint8_t i) {
    if (i >= MAX) return;
    items_[i].acked = true;
    items_[i].popup = false;
  }

private:
  Deadline items_[MAX];

  // After a save the user has just seen the entry, so don't pop up
  // immediately. The footer banner still shows if it is already due soon.
  void settle(uint8_t i, uint32_t now) {
    items_[i].stage = status(i, now);
    items_[i].acked = true;
    items_[i].popup = false;
    items_[i].lastAlert = now;
  }
};

// =====================================================================
//  SETTINGS + FEATURE MANAGER (owns every module)
// =====================================================================
struct Settings {
  bool buzzerEnabled;
  bool irEnabled;
  bool irPausesTimer;  // away also holds the FOCUS countdown
  uint8_t awayIdx;     // AWAY_SECONDS
  uint8_t repeatIdx;   // REPEAT_MINUTES
  uint8_t brightness;  // 1 - 15
};

class FeatureManager {
public:
  Buzzer buzzer;
  SoftClock clock;
  BreakTimer timer;
  StudyTracker study;
  DeadlineManager deadlines;
  PresenceSensor presence;
  Settings settings;

  void begin(unsigned long now) {
    settings.buzzerEnabled = true;
    settings.irEnabled = true;
    settings.irPausesTimer = true;
    settings.awayIdx = 2;    // 1 min
    settings.repeatIdx = 2;  // 30 min
    settings.brightness = 10;
#if USE_WIRELING_ADAPTER
    pinMode(WIRELING_POWER_PIN, OUTPUT);
    digitalWrite(WIRELING_POWER_PIN, HIGH);
#endif
    buzzer.begin(BUZZER_PIN, BUZZER_PASSIVE != 0);
    presence.begin(IR_SENSOR_PIN, IR_ACTIVE_LEVEL, IR_USE_PULLUP != 0, now);
    clock.begin(now);
    timer.begin(now);
    study.begin(now);
    deadlines.begin();
    applySettings(now);
    lastDeadlineCheck_ = now;
  }

  void applySettings(unsigned long now) {
    buzzer.setEnabled(settings.buzzerEnabled);
    if (presence.enabled() != settings.irEnabled) presence.setEnabled(settings.irEnabled, now);
    presence.setAbsentTimeout((unsigned long)AWAY_SECONDS[settings.awayIdx] * 1000UL);
  }

  void update(unsigned long now) {
    clock.update(now);

    // IR presence -> pause / resume
    presence.update(now);
    const bool present = presence.present();
    study.setPresent(present);
    timer.setAutoPaused(settings.irPausesTimer && !present);

    const TimerEvent ev = timer.update(now);
    if (ev != TEV_NONE) onTimerEvent(ev, now);
    study.update(now);

    if ((unsigned long)(now - lastDeadlineCheck_) >= 1000UL) {
      lastDeadlineCheck_ = now;
      const uint8_t alert = deadlines.update(clock.epoch(), REPEAT_MINUTES[settings.repeatIdx]);
      if (alert == DL_OVERDUE) buzzer.play(BEEP_DEADLINE_OVERDUE, now);
      else if (alert == DL_SOON) buzzer.play(BEEP_DEADLINE_SOON, now);
    }
    buzzer.update(now);
  }

  void timerSkip(unsigned long now) {
    const TimerEvent ev = timer.skip();
    if (ev != TEV_NONE) onTimerEvent(ev, now);
  }

  // UI pulls this once to show a toast.
  TimerEvent takeTimerEvent() {
    const TimerEvent e = pendingEvent_;
    pendingEvent_ = TEV_NONE;
    return e;
  }

  uint32_t nowEpoch() const { return clock.epoch(); }

private:
  unsigned long lastDeadlineCheck_ = 0;
  TimerEvent pendingEvent_ = TEV_NONE;

  void onTimerEvent(TimerEvent ev, unsigned long now) {
    pendingEvent_ = ev;
    switch (ev) {
      case TEV_BREAK_STARTED:
      case TEV_LONG_BREAK_STARTED: buzzer.play(BEEP_BREAK_START, now); break;
      case TEV_FOCUS_STARTED: buzzer.play(BEEP_FOCUS_START, now); break;
      case TEV_CYCLE_COMPLETE: buzzer.play(BEEP_CYCLE_DONE, now); break;
      default: break;
    }
  }
};

#endif
