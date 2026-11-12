#ifndef STUDYPET_UI_H
#define STUDYPET_UI_H

#include <Arduino.h>
#include <TinyScreen.h>
#include <string.h>
#include <stdio.h>
#include "Input.h"
#include "Pet.h"
#include "Features.h"

// =====================================================================
//  UI STATE MACHINE
//
//   HOME (pet + timer header + status footer)
//     short press            -> pet interaction (<= 5 s)
//     hold SELECT            -> MAIN menu (full screen)
//     hold BACK              -> start / pause break timer
//     hold UP                -> start / stop study session
//     hold DOWN              -> hours dashboard
//   MENU STACK (three rows + persistent physical button labels)
//     UP / DOWN move, SELECT open, BACK return, hold BACK = home
//   ALERT pop-up (deadline) is pushed on top of ANY screen and
//     returns to exactly where you were when dismissed.
// =====================================================================
enum Screen : uint8_t {
  SCR_HOME = 0, SCR_MAIN, SCR_TIMER, SCR_STUDY, SCR_SUBJECTS, SCR_DASH,
  SCR_DEADLINES, SCR_DL_EDIT, SCR_PET, SCR_PET_PICK, SCR_SETTINGS,
  SCR_VALUE, SCR_DATETIME, SCR_CONFIRM, SCR_ALERT, SCR_GUIDE
};

enum ValueTarget : uint8_t {
  VT_NONE = 0, VT_FOCUS, VT_SHORT, VT_LONG, VT_CYCLES,
  VT_DL_SUBJECT, VT_DL_KIND, VT_DL_LEAD, VT_AWAY, VT_REPEAT, VT_BRIGHT, VT_SENSOR_PORT
};
enum DateTarget : uint8_t { DT_CLOCK = 0, DT_DEADLINE };
enum ConfirmTarget : uint8_t { CF_NONE = 0, CF_DELETE_DEADLINE, CF_RESET_TOTALS };

struct MenuFrame {
  Screen screen;
  uint8_t cursor;
  uint8_t top;
};

// ---- colours ----
#define UI_BG       TS_8b_Black
#define UI_TEXT     TS_8b_White
#define UI_DIM      C8(1, 3, 3)
#define UI_TITLE    TS_8b_DarkBlue
#define UI_SEL      C8(0, 3, 6)
#define UI_FOCUS    C8(0, 1, 4)
#define UI_BREAK    C8(0, 3, 0)
#define UI_LONG     C8(0, 3, 3)
#define UI_PAUSE    C8(1, 2, 2)
#define UI_WARN     C8(3, 3, 0)
#define UI_DANGER   C8(2, 0, 0)
#define UI_GOOD     C8(0, 2, 1)

class StudyPetUI {
public:
  void begin(TinyScreen &d, unsigned long /*now*/) {
    depth_ = 0;
    needClear_ = true;
    appliedBrightness_ = 255;
    toastAt_ = 0;
    toastText_[0] = 0;
    d.clearScreen();
  }

  // ------------------------------------------------------------------
  //  INPUT
  // ------------------------------------------------------------------
  void handleInput(const InputEvent &ev, Pet &pet, FeatureManager &f, unsigned long now) {
    // A hold that opened a screen must not keep auto-repeating into it.
    if (ev.type != PRESS_REPEAT) swallowKey_ = KEY_NONE;
    else if (ev.key == swallowKey_) return;
    switch (current()) {
      case SCR_HOME: homeInput(ev, pet, f, now); break;
      case SCR_VALUE: valueInput(ev, f, now); break;
      case SCR_DATETIME: dateInput(ev, f, now); break;
      case SCR_PET_PICK: petPickInput(ev, pet); break;
      case SCR_DASH: dashInput(ev); break;
      case SCR_GUIDE: guideInput(ev); break;
      case SCR_ALERT:
        if ((ev.key == KEY_SELECT || ev.key == KEY_BACK) && ev.type == PRESS_SHORT) {
          f.deadlines.acknowledge(alertIndex_);
          f.buzzer.stop();
          pop();
        }
        break;
      default: listInput(ev, pet, f, now); break;
    }
  }

  // ------------------------------------------------------------------
  //  DRAW (call every loop; throttled internally)
  // ------------------------------------------------------------------
  void update(TinyScreen &d, Pet &pet, FeatureManager &f, unsigned long now) {
    // Deadline pop-ups interrupt any screen.
    const int8_t pop = f.deadlines.nextPopup();
    if (pop >= 0 && current() != SCR_ALERT) {
      alertIndex_ = (uint8_t)pop;
      push(SCR_ALERT);
    }
    const TimerEvent tev = f.takeTimerEvent();
    if (tev != TEV_NONE) timerToast(tev, f, now);

    // Brightness (live preview while editing it).
    const uint8_t want = (current() == SCR_VALUE && vt_ == VT_BRIGHT) ? (uint8_t)value_
                                                                       : f.settings.brightness;
    if (want != appliedBrightness_) {
      d.setBrightness(want);
      appliedBrightness_ = want;
    }

    if (!needClear_ && (unsigned long)(now - lastDraw_) < 25UL) return;
    lastDraw_ = now;

    if (needClear_) {
      d.clearScreen();
      invalidate();
      needClear_ = false;
    }

    switch (current()) {
      case SCR_HOME: drawHome(d, pet, f, now); break;
      case SCR_DASH: drawDash(d, f); break;
      case SCR_VALUE: drawValue(d); break;
      case SCR_DATETIME: drawDate(d); break;
      case SCR_PET_PICK: drawPetPick(d, pet, now); break;
      case SCR_ALERT: drawAlert(d, f); break;
      case SCR_GUIDE: drawGuide(d); break;
      default: drawList(d, pet, f); break;
    }
  }

private:
  // ---- layout ----
  static const uint8_t ROWS = 3;  // reserve the bottom 11 px for button labels
  static const uint8_t TEXT_MAX = 32;
  static const uint8_t SLOTS = 8;
  static const uint8_t PET_X = 26;
  static const uint8_t PET_Y = 12;
  static const uint8_t FRAME_W = 44;
  static const uint8_t FRAME_H = 41;

  struct LineCache {
    char text[TEXT_MAX];
    uint8_t fg, bg;
    bool valid;
  };

  // ---- state ----
  MenuFrame stack_[8];
  uint8_t depth_ = 0;
  bool needClear_ = true;
  unsigned long lastDraw_ = 0;
  uint8_t appliedBrightness_ = 255;
  LineCache cache_[SLOTS];
  uint16_t sigLeft_ = 0xFFFF, sigRight_ = 0xFFFF, sigScroll_ = 0xFFFF;
  bool petDrawn_ = false;
  PetPose lastPose_;
  uint8_t frame_[FRAME_W * FRAME_H];  // 1,804 bytes off-screen sprite buffer

  char toastText_[TEXT_MAX];
  uint8_t toastBg_ = UI_GOOD;
  unsigned long toastAt_ = 0;

  // value editor
  ValueTarget vt_ = VT_NONE;
  int16_t value_ = 0, vMin_ = 0, vMax_ = 0, vStep_ = 1;
  const char *const *vLabels_ = nullptr;
  const char *vUnit_ = "";
  const char *vTitle_ = "";

  // date editor
  DateTarget dtTarget_ = DT_CLOCK;
  DateTime dt_;
  uint8_t dtField_ = 0;

  // deadline editor
  int8_t dlEditIndex_ = -1;  // -1 = creating a new deadline
  Deadline draft_;

  ConfirmTarget confirm_ = CF_NONE;
  const char *confirmTitle_ = "";
  uint8_t alertIndex_ = 0;
  uint8_t alertBg_ = 0;
  bool pickPalette_ = false;
  uint8_t pickValue_ = 0;
  uint8_t dashTop_ = 0;
  uint8_t guidePage_ = 0;
  NavKey swallowKey_ = KEY_NONE;

  // ------------------------------------------------------------------
  //  stack helpers
  // ------------------------------------------------------------------
  Screen current() const { return depth_ ? stack_[depth_ - 1].screen : SCR_HOME; }
  MenuFrame &top() { return stack_[depth_ - 1]; }

  void push(Screen s) {
    if (depth_ >= sizeof(stack_) / sizeof(stack_[0])) depth_--;  // never overflow
    stack_[depth_].screen = s;
    stack_[depth_].cursor = 0;
    stack_[depth_].top = 0;
    depth_++;
    if (s == SCR_DASH) dashTop_ = 0;
    if (s == SCR_GUIDE) guidePage_ = 0;
    needClear_ = true;
  }
  void pop() {
    if (depth_) depth_--;
    needClear_ = true;
  }
  void popToHome() {
    depth_ = 0;
    needClear_ = true;
  }

  void invalidate() {
    for (uint8_t i = 0; i < SLOTS; ++i) cache_[i].valid = false;
    sigLeft_ = sigRight_ = sigScroll_ = 0xFFFF;
    petDrawn_ = false;
  }

  void toast(const char *text, uint8_t bg, unsigned long now) {
    strncpy(toastText_, text, TEXT_MAX - 1);
    toastText_[TEXT_MAX - 1] = 0;
    toastBg_ = bg;
    toastAt_ = now;
  }

  void timerToast(TimerEvent ev, FeatureManager &f, unsigned long now) {
    char b[TEXT_MAX];
    const TimerConfig &c = f.timer.config();
    switch (ev) {
      case TEV_BREAK_STARTED: snprintf(b, sizeof b, "Break! %u min", c.shortMin); toast(b, UI_BREAK, now); break;
      case TEV_LONG_BREAK_STARTED: snprintf(b, sizeof b, "Long break! %u min", c.longMin); toast(b, UI_LONG, now); break;
      case TEV_FOCUS_STARTED: snprintf(b, sizeof b, "Focus %u/%u - go!", f.timer.cycle(), c.cycles); toast(b, UI_FOCUS, now); break;
      case TEV_CYCLE_COMPLETE: toast("Cycle complete :)", UI_GOOD, now); break;
      default: break;
    }
  }

  // ------------------------------------------------------------------
  //  text helpers
  // ------------------------------------------------------------------
  void drawText(TinyScreen &d, uint8_t slot, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                const char *text, uint8_t fg, uint8_t bg) {
    LineCache &c = cache_[slot % SLOTS];
    if (c.valid && c.fg == fg && c.bg == bg && strncmp(c.text, text, TEXT_MAX - 1) == 0) return;
    d.drawRect(x, y, w, h, TSRectangleFilled, bg);
    d.fontColor(fg, bg);
    d.setCursor(x + 2, y + (h > 8 ? (h - 8) / 2 : 0));
    char clipped[TEXT_MAX];
    strncpy(clipped, text, TEXT_MAX - 1);
    clipped[TEXT_MAX - 1] = 0;
    // Clip by pixels too: character count alone can overwrite adjacent panels.
    size_t len = strlen(clipped);
    while (len && d.getPrintWidth(clipped) > w - 4) clipped[--len] = 0;
    d.print(clipped);
    strncpy(c.text, text, TEXT_MAX - 1);
    c.text[TEXT_MAX - 1] = 0;
    c.fg = fg;
    c.bg = bg;
    c.valid = true;
  }

  // Fixed 3x5 capitals keep physical button labels readable in eleven pixels.
  // The main content keeps the larger TinyScreen font.
  static uint16_t smallGlyph(char ch) {
#define GLYPH(a,b,c,d,e) ((a << 12) | (b << 9) | (c << 6) | (d << 3) | e)
    static const uint16_t glyphs[] = {
      GLYPH(2,5,7,5,5), GLYPH(6,5,6,5,6), GLYPH(3,4,4,4,3), // ABC
      GLYPH(6,5,5,5,6), GLYPH(7,4,6,4,7), GLYPH(7,4,6,4,4), // DEF
      GLYPH(3,4,5,5,3), GLYPH(5,5,7,5,5), GLYPH(7,2,2,2,7), // GHI
      GLYPH(1,1,1,5,2), GLYPH(5,5,6,5,5), GLYPH(4,4,4,4,7), // JKL
      GLYPH(5,7,7,5,5), GLYPH(5,7,7,7,5), GLYPH(2,5,5,5,2), // MNO
      GLYPH(6,5,6,4,4), GLYPH(2,5,5,7,3), GLYPH(6,5,6,5,5), // PQR
      GLYPH(3,4,2,1,6), GLYPH(7,2,2,2,2), GLYPH(5,5,5,5,7), // STU
      GLYPH(5,5,5,5,2), GLYPH(5,5,7,7,5), GLYPH(5,5,2,5,5), // VWX
      GLYPH(5,5,2,2,2), GLYPH(7,1,2,4,7),                   // YZ
      GLYPH(7,5,5,5,7), GLYPH(2,6,2,2,7), GLYPH(6,1,2,4,7), // 012
      GLYPH(6,1,2,1,6), GLYPH(5,5,7,1,1), GLYPH(7,4,6,1,6), // 345
      GLYPH(3,4,6,5,2), GLYPH(7,1,2,2,2), GLYPH(2,5,2,5,2), // 678
      GLYPH(2,5,3,1,6)                                      // 9
    };
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    if (ch >= 'A' && ch <= 'Z') return glyphs[ch - 'A'];
    if (ch >= '0' && ch <= '9') return glyphs[26 + ch - '0'];
    switch (ch) {
      case ':': return GLYPH(0,2,0,2,0);
      case '/': return GLYPH(1,1,2,4,4);
      case '-': return GLYPH(0,0,7,0,0);
      case '+': return GLYPH(0,2,7,2,0);
      case '=': return GLYPH(0,7,0,7,0);
      case '<': return GLYPH(1,2,4,2,1);
      case '>': return GLYPH(4,2,1,2,4);
      case '!': return GLYPH(2,2,2,0,2);
      case '.': return GLYPH(0,0,0,0,2);
      case ' ': return 0;
      default: return GLYPH(6,1,2,0,2);
    }
#undef GLYPH
  }

  static void smallText(TinyScreen &d, uint8_t x, uint8_t y, const char *text,
                        uint8_t color, uint8_t maxWidth = 96) {
    const uint16_t end = (uint16_t)x + maxWidth;
    for (; *text && x + 3 <= end && x + 3 <= 96; ++text, x += 4) {
      const uint16_t bits = smallGlyph(*text);
      for (uint8_t row = 0; row < 5; ++row)
        for (uint8_t col = 0; col < 3; ++col)
          if (bits & (1U << (14 - row * 3 - col))) d.drawPixel(x + col, y + row, color);
    }
  }

  void smallLine(TinyScreen &d, uint8_t slot, uint8_t y, const char *text,
                 uint8_t color = UI_TEXT) {
    LineCache &c = cache_[slot];
    if (c.valid && c.fg == color && strcmp(c.text, text) == 0) return;
    d.drawRect(0, y, 96, 6, TSRectangleFilled, UI_BG);
    smallText(d, 2, y, text, color, 92);
    strncpy(c.text, text, TEXT_MAX - 1); c.text[TEXT_MAX - 1] = 0;
    c.fg = color; c.valid = true;
  }

  void hintRow(TinyScreen &d, uint8_t slot, uint8_t y, const char *left, const char *right) {
    char key[TEXT_MAX];
    snprintf(key, sizeof key, "%s|%s", left, right);
    LineCache &c = cache_[slot];
    if (c.valid && strcmp(c.text, key) == 0) return;
    d.drawRect(0, y, 96, 5, TSRectangleFilled, UI_TITLE);
    smallText(d, 1, y, left, UI_TEXT, 46);
    const uint8_t len = (uint8_t)strlen(right);
    smallText(d, 96 - len * 4, y, right, UI_TEXT, 46);
    strncpy(c.text, key, TEXT_MAX - 1); c.text[TEXT_MAX - 1] = 0; c.valid = true;
  }

  // A two-row map mirrors the four physical buttons. TL/TR = top left/right;
  // BL/BR = bottom left/right. Each action label follows the current screen.
  void controls(TinyScreen &d, const char *back, const char *up,
                const char *select, const char *down) {
    char tl[12], tr[12], bl[12], br[12];
    snprintf(tl, sizeof tl, "TL %.8s", back); snprintf(tr, sizeof tr, "TR %.8s", up);
    snprintf(bl, sizeof bl, "BL %.8s", select); snprintf(br, sizeof br, "BR %.8s", down);
    hintRow(d, 6, 53, tl, tr);
    hintRow(d, 7, 59, bl, br);
  }

#if PRESENCE_USE_TOF
  static void formatDistance(char *b, size_t n, const FeatureManager &f) {
    if (!f.settings.irEnabled) snprintf(b, n, "Range: off");
    else if (!f.presence.distance.valid()) snprintf(b, n, "Range: -- cm");
    else {
      const uint16_t mm = f.presence.distance.millimeters();
      snprintf(b, n, "Range: %u.%u cm", mm / 10, mm % 10);
    }
  }
#endif

  static void fmtClock(char *b, size_t n, uint32_t epoch) {
    const DateTime t = TimeUtil::fromEpoch(epoch);
    snprintf(b, n, "%02u:%02u", t.hour, t.minute);
  }
  static void fmtDate(char *b, size_t n, uint32_t epoch) {
    const DateTime t = TimeUtil::fromEpoch(epoch);
    snprintf(b, n, "%02u %s %02u:%02u", t.day, TimeUtil::MONTHS[t.month - 1], t.hour, t.minute);
  }
  static void fmtMMSS(char *b, size_t n, uint32_t secs) {
    snprintf(b, n, "%02lu:%02lu", (unsigned long)(secs / 60UL), (unsigned long)(secs % 60UL));
  }
  static void fmtHours(char *b, size_t n, uint32_t secs) {
    snprintf(b, n, "%luh%02lum", (unsigned long)(secs / 3600UL), (unsigned long)((secs / 60UL) % 60UL));
  }
  // compact: "45m", "5h", "2d", "-3h"
  static void fmtRelShort(char *b, size_t n, int32_t diff) {
    const bool late = diff < 0;
    const uint32_t a = late ? (uint32_t)(-diff) : (uint32_t)diff;
    const char *sign = late ? "-" : "";
    if (a < 3600UL) snprintf(b, n, "%s%lum", sign, (unsigned long)(a / 60UL));
    else if (a < 86400UL) snprintf(b, n, "%s%luh", sign, (unsigned long)(a / 3600UL));
    else snprintf(b, n, "%s%lud", sign, (unsigned long)(a / 86400UL));
  }
  // long: "in 1d 4h", "in 3h 20m", "2h 5m late"
  static void fmtRelLong(char *b, size_t n, int32_t diff) {
    const bool late = diff < 0;
    const uint32_t a = late ? (uint32_t)(-diff) : (uint32_t)diff;
    char core[14];
    if (a < 60UL) snprintf(core, sizeof core, "<1m");
    else if (a < 3600UL) snprintf(core, sizeof core, "%lum", (unsigned long)(a / 60UL));
    else if (a < 86400UL) snprintf(core, sizeof core, "%luh %lum", (unsigned long)(a / 3600UL), (unsigned long)((a / 60UL) % 60UL));
    else snprintf(core, sizeof core, "%lud %luh", (unsigned long)(a / 86400UL), (unsigned long)((a / 3600UL) % 24UL));
    if (late) snprintf(b, n, "%s late", core);
    else snprintf(b, n, "in %s", core);
  }

  void deadlineName(char *b, size_t n, const Deadline &dl) const {
    snprintf(b, n, "%s %s", SUBJECT_NAMES[dl.subject % SUBJECT_COUNT], KIND_SHORT[dl.kind % KIND_COUNT]);
  }

  // ------------------------------------------------------------------
  //  HOME SCREEN
  // ------------------------------------------------------------------
  void homeInput(const InputEvent &ev, Pet &pet, FeatureManager &f, unsigned long now) {
    if (ev.type == PRESS_SHORT) {
      pet.trigger(ev.key, now);
      return;
    }
    if (ev.type != PRESS_LONG) return;
    swallowKey_ = ev.key;
    char b[TEXT_MAX];
    switch (ev.key) {
      case KEY_SELECT: push(SCR_MAIN); break;
      case KEY_BACK: {
        const bool wasIdle = f.timer.phase() == PHASE_IDLE;
        const bool wasPaused = f.timer.userPaused();
        f.timer.toggle(now);
        toast(wasIdle ? "Timer started" : wasPaused ? "Timer resumed" : "Timer paused", UI_FOCUS, now);
        break;
      }
      case KEY_UP:
        if (f.study.active()) {
          f.study.stop(now);
          toast("Session saved", UI_GOOD, now);
        } else {
          f.study.start(f.study.lastSubject(), now);
          snprintf(b, sizeof b, "Studying %s", SUBJECT_NAMES[f.study.subject()]);
          toast(b, UI_GOOD, now);
        }
        break;
      case KEY_DOWN: push(SCR_DASH); break;
      default: break;
    }
  }

  void drawHome(TinyScreen &d, Pet &pet, FeatureManager &f, unsigned long now) {
    char b[TEXT_MAX], t[10];
    const BreakTimer &tm = f.timer;
    uint8_t hbg = UI_TITLE;
    if (tm.phase() == PHASE_IDLE) {
      drawText(d, 0, 0, 0, 64, 12, "STUDYPET", UI_TEXT, hbg);
      cache_[5].valid = false;
    } else {
      fmtMMSS(t, sizeof t, tm.remainingSeconds());
      const char *state;
      if (tm.userPaused()) { state = "PAUSE"; hbg = UI_PAUSE; }
      else if (tm.awayHeld()) { state = "AWAY"; hbg = UI_PAUSE; }
      else if (tm.phase() == PHASE_FOCUS) { state = "FOCUS"; hbg = UI_FOCUS; }
      else if (tm.phase() == PHASE_SHORT_BREAK) { state = "BREAK"; hbg = UI_BREAK; }
      else { state = "LONG"; hbg = UI_LONG; }
      // Separate the small phase caption from the large digits so even a
      // 180-minute custom countdown fits beside the wall clock in full.
      LineCache &c = cache_[5];
      if (!c.valid || c.bg != hbg || strcmp(c.text, state) != 0) {
        d.drawRect(0, 0, 26, 12, TSRectangleFilled, hbg);
        smallText(d, 2, 4, state, UI_TEXT, 23);
        strncpy(c.text, state, TEXT_MAX); c.bg = hbg; c.valid = true;
      }
      drawText(d, 0, 26, 0, 38, 12, t, UI_TEXT, hbg);
    }
    fmtClock(t, sizeof t, f.nowEpoch());
    drawText(d, 1, 64, 0, 32, 12, t, UI_TEXT, hbg);

    // Pet: rendered into RAM, sent in one SPI burst only when the pose changes.
    const PetPose p = pet.pose(now);
    if (!petDrawn_ || !samePose(p, lastPose_)) {
      renderPet(p);
      blitPet(d);
      lastPose_ = p;
      petDrawn_ = true;
    }

    drawSideColumns(d, f);

    uint8_t fbg = UI_BG, ffg = UI_TEXT;
    homeFooter(b, sizeof b, ffg, fbg, pet, f, now);
    drawText(d, 2, 0, 53, 96, 11, b, ffg, fbg);
  }

  void homeFooter(char *b, size_t n, uint8_t &fg, uint8_t &bg, Pet &pet, FeatureManager &f,
                  unsigned long now) {
    fg = UI_TEXT;
    if (toastText_[0] && (unsigned long)(now - toastAt_) < 5000UL) {
      strncpy(b, toastText_, n - 1); b[n - 1] = 0;
      bg = toastBg_;
      return;
    }
    if (pet.actionActive()) {
      strncpy(b, pet.actionLabel(), n - 1); b[n - 1] = 0;
      bg = UI_BG;
      return;
    }
    // Rotate between the deadline reminder and the session status.
    const uint32_t e = f.nowEpoch();
    const int8_t urgent = f.deadlines.mostUrgent(e);
    const bool session = f.study.active();
    const bool showDeadline = urgent >= 0 && (!session || ((now / 4000UL) % 2UL) == 0);
    if (showDeadline) {
      const Deadline &dl = f.deadlines.get(urgent);
      char nm[14], rel[8];
      deadlineName(nm, sizeof nm, dl);
      fmtRelShort(rel, sizeof rel, (int32_t)(dl.due - e));
      if (f.deadlines.status(urgent, e) == DL_OVERDUE) { snprintf(b, n, "LATE %s", nm); bg = UI_DANGER; }
      else { snprintf(b, n, "!%s %s", nm, rel); bg = UI_WARN; }
      return;
    }
    // Display real range on home too; active session time alternates with it.
#if PRESENCE_USE_TOF
    if (f.settings.irEnabled && (!session || ((now / 4000UL) % 2UL) == 0)) {
      formatDistance(b, n, f);
      bg = f.presence.present() ? UI_GOOD : UI_PAUSE;
      return;
    }
#endif
    if (session) {
      if (f.study.awayPaused()) { snprintf(b, n, "Away: paused"); bg = UI_PAUSE; }
      else {
        const uint32_t s = f.study.sessionSeconds();
        snprintf(b, n, "%s %lu:%02lu:%02lu", SUBJECT_NAMES[f.study.subject()], (unsigned long)(s / 3600UL),
                 (unsigned long)((s / 60UL) % 60UL), (unsigned long)(s % 60UL));
        bg = UI_GOOD;
      }
      return;
    }
    if (f.timer.awayHeld()) {
      snprintf(b, n, "Check sensor range"); bg = UI_PAUSE;
      return;
    }
    bg = UI_BG;
    switch ((now / 4000UL) % 3UL) {
      case 0: snprintf(b, n, "TR Jump / BR Nap"); break;
      case 1: snprintf(b, n, "TL Wiggle BL Love"); break;
      default: snprintf(b, n, "Menu: Button guide"); break;
    }
  }

  // Hold shortcuts sit beside the actual button positions, outside the sprite.
  // Existing progress / presence / session indicators occupy the middle gap.
  void drawSideColumns(TinyScreen &d, FeatureManager &f) {
    const BreakTimer &tm = f.timer;
    const uint16_t sigL = (tm.phase() == PHASE_IDLE) ? 0
                          : (uint16_t)(1 + (tm.config().cycles << 8) + (tm.cycle() << 3) + tm.phase());
    if (sigL != sigLeft_) {
      d.drawRect(0, 12, 26, 41, TSRectangleFilled, UI_BG);
      smallText(d, 2, 14, "HOLD", UI_DIM, 23);
      smallText(d, 2, 21, "TIMER", UI_TEXT, 23);
      smallText(d, 2, 36, "HOLD", UI_DIM, 23);
      smallText(d, 2, 43, "MENU", C8(0, 7, 7), 23);
      if (tm.phase() != PHASE_IDLE) {
        const uint8_t n = tm.config().cycles > 8 ? 8 : tm.config().cycles;
        for (uint8_t i = 0; i < n; ++i) {
          const uint8_t num = i + 1;
          uint8_t c = C8(1, 1, 1);                                     // still to do
          if (num < tm.cycle() || (num == tm.cycle() && tm.onBreak())) c = C8(3, 1, 0);  // done
          else if (num == tm.cycle()) c = C8(3, 7, 0);                 // current focus
          d.drawRect(2 + (i % 8) * 3, 29, 2, 3, TSRectangleFilled, c);
        }
      }
      sigLeft_ = sigL;
    }

    const bool ir = f.settings.irEnabled, present = f.presence.present();
    const bool sess = f.study.active(), paused = f.study.awayPaused();
    const uint16_t sigR = (uint16_t)(1 | (ir << 1) | (present << 2) | (sess << 3) | (paused << 4));
    if (sigR != sigRight_) {
      d.drawRect(70, 12, 26, 41, TSRectangleFilled, UI_BG);
      smallText(d, 74, 14, "HOLD", UI_DIM, 22);
      smallText(d, 74, 21, "STUDY", UI_TEXT, 22);
      smallText(d, 74, 36, "HOLD", UI_DIM, 22);
      smallText(d, 74, 43, "HOURS", UI_TEXT, 22);
      if (ir) d.drawRect(75, 29, 3, 3, TSRectangleFilled, present ? C8(0, 7, 0) : UI_DIM);
      if (sess) {
        const uint8_t c = paused ? C8(1, 2, 2) : C8(2, 6, 7);
        d.drawRect(83, 28, 9, 5, TSRectangleNoFill, c);  // open book
        d.drawLine(87, 28, 87, 32, c);
        if (paused) {  // pause bars
          d.drawRect(84, 29, 1, 3, TSRectangleFilled, UI_TEXT);
          d.drawRect(89, 29, 1, 3, TSRectangleFilled, UI_TEXT);
        }
      }
      sigRight_ = sigR;
    }
  }

  // ------------------------------------------------------------------
  //  GENERIC LIST MENUS
  // ------------------------------------------------------------------
  uint8_t itemCount(Screen s, FeatureManager &f) const {
    switch (s) {
      case SCR_MAIN: return 6;
      case SCR_TIMER: return 8;
      case SCR_STUDY: return 3;
      case SCR_SUBJECTS: return SUBJECT_COUNT;
      case SCR_DEADLINES: return 1 + f.deadlines.count();
      case SCR_DL_EDIT: return dlEditIndex_ < 0 ? 5 : 6;
      case SCR_PET: return 2;
      case SCR_SETTINGS: return (PRESENCE_USE_TOF || IR_USE_THERMAL_CAMERA) ? 12 : 9;
      case SCR_CONFIRM: return 2;
      default: return 0;
    }
  }

  void titleFor(Screen s, char *b, size_t n, FeatureManager &f) const {
    char t[10];
    switch (s) {
      case SCR_MAIN: fmtClock(t, sizeof t, f.nowEpoch()); snprintf(b, n, "MENU      %.5s", t); break;
      case SCR_TIMER:
        if (f.timer.phase() == PHASE_IDLE) snprintf(b, n, "TIMER  idle");
        else {
          fmtMMSS(t, sizeof t, f.timer.remainingSeconds());
          const char *ph = f.timer.phase() == PHASE_FOCUS ? "F" : f.timer.phase() == PHASE_SHORT_BREAK ? "B" : "L";
          if (f.timer.userPaused()) snprintf(b, n, "PAUSED %s", t);
          else if (f.timer.awayHeld()) snprintf(b, n, "AWAY %s", t);
          else snprintf(b, n, "%s %u/%u %s", ph, f.timer.cycle(), f.timer.config().cycles, t);
        }
        break;
      case SCR_STUDY:
        if (f.study.active()) {
          fmtHours(t, sizeof t, f.study.sessionSeconds());
          snprintf(b, n, "STUDY %s %s", f.study.awayPaused() ? "away" : "on", t);
        } else snprintf(b, n, "STUDY  idle");
        break;
      case SCR_SUBJECTS: snprintf(b, n, "PICK SUBJECT"); break;
      case SCR_DEADLINES: snprintf(b, n, "DEADLINES %u/%u", f.deadlines.count(), DeadlineManager::MAX); break;
      case SCR_DL_EDIT: snprintf(b, n, "%s", dlEditIndex_ < 0 ? "NEW DEADLINE" : "EDIT DEADLINE"); break;
      case SCR_PET: snprintf(b, n, "PET"); break;
      case SCR_SETTINGS: snprintf(b, n, "SETTINGS"); break;
      case SCR_CONFIRM: snprintf(b, n, "%s", confirmTitle_); break;
      default: b[0] = 0; break;
    }
  }

  void itemLabel(Screen s, uint8_t i, char *b, size_t n, Pet &pet, FeatureManager &f) const {
    const uint32_t e = f.nowEpoch();
    switch (s) {
      case SCR_MAIN: {
        static const char *const items[6] = {"Break timer", "Study session", "Deadlines", "Pet", "Settings", "Button guide"};
        snprintf(b, n, "%s", items[i]);
        break;
      }
      case SCR_TIMER: {
        const TimerConfig &c = f.timer.config();
        switch (i) {
          case 0: snprintf(b, n, "%s", f.timer.phase() == PHASE_IDLE ? "Start focus"
                                     : f.timer.userPaused() ? "Resume" : "Pause"); break;
          case 1: snprintf(b, n, "Skip phase"); break;
          case 2: snprintf(b, n, "Reset cycle"); break;
          case 3: snprintf(b, n, "Style: %s", STYLE_NAMES[f.timer.style()]); break;
          case 4: snprintf(b, n, "Focus: %u min", c.focusMin); break;
          case 5: snprintf(b, n, "Break: %u min", c.shortMin); break;
          case 6: snprintf(b, n, "Long brk: %u min", c.longMin); break;
          default: snprintf(b, n, "Cycles: %u", c.cycles); break;
        }
        break;
      }
      case SCR_STUDY:
        if (i == 0) snprintf(b, n, "%s", f.study.active() ? "Stop session" : "Start session");
        else if (i == 1) snprintf(b, n, "Hours dashboard");
        else snprintf(b, n, "Reset all hours");
        break;
      case SCR_SUBJECTS: snprintf(b, n, "%s", SUBJECT_NAMES[i]); break;
      case SCR_DEADLINES: {
        if (i == 0) { snprintf(b, n, "%s", f.deadlines.full() ? "(list is full)" : "+ New deadline"); break; }
        uint8_t order[DeadlineManager::MAX];
        const uint8_t cnt = f.deadlines.sorted(order);
        if (i - 1 >= cnt) { b[0] = 0; break; }
        const uint8_t idx = order[i - 1];
        const Deadline &dl = f.deadlines.get(idx);
        const uint8_t st = f.deadlines.status(idx, e);
        char nm[14], rel[8];
        deadlineName(nm, sizeof nm, dl);
        fmtRelShort(rel, sizeof rel, (int32_t)(dl.due - e));
        snprintf(b, n, "%c%s %s", st == DL_OVERDUE ? 'X' : st == DL_SOON ? '!' : ' ', nm, rel);
        break;
      }
      case SCR_DL_EDIT: {
        const bool isNew = dlEditIndex_ < 0;
        if (isNew && i == 0) { snprintf(b, n, "> Save deadline"); break; }
        const uint8_t fld = isNew ? i - 1 : i;
        char t[18];
        switch (fld) {
          case 0: snprintf(b, n, "Subj: %s", SUBJECT_NAMES[draft_.subject]); break;
          case 1: snprintf(b, n, "Type: %s", KIND_NAMES[draft_.kind]); break;
          case 2: fmtDate(t, sizeof t, draft_.due); snprintf(b, n, "Due %s", t); break;
          case 3: snprintf(b, n, "Remind %s", LEAD_LABELS[draft_.lead]); break;
          case 4: fmtRelLong(t, sizeof t, (int32_t)(draft_.due - e)); snprintf(b, n, "Due %s", t); break;
          default: snprintf(b, n, "Delete deadline"); break;
        }
        break;
      }
      case SCR_PET:
        if (i == 0) snprintf(b, n, "Pet: %s", Pet::speciesName(pet.species()));
        else snprintf(b, n, "Colour: %s", Pet::paletteName(pet.species(), pet.palette()));
        break;
      case SCR_SETTINGS: {
        const Settings &st = f.settings;
        switch (i) {
          case 0: snprintf(b, n, "Set date & time"); break;
          case 1: snprintf(b, n, "Buzzer: %s", st.buzzerEnabled ? "On" : "Off"); break;
          case 2: snprintf(b, n, "%s: %s", PRESENCE_USE_TOF ? "TOF sensor" : IR_USE_THERMAL_CAMERA ? "Camera" : "IR sensor", st.irEnabled ? "On" : "Off"); break;
          case 3:
#if PRESENCE_USE_TOF
            snprintf(b, n, "Cutoff: %u cm", TOF_PRESENCE_MAX_MM / 10);
#else
            snprintf(b, n, "Away after %s", AWAY_LABELS[st.awayIdx]);
#endif
            break;
          case 4: snprintf(b, n, "Away->timer: %s", st.irPausesTimer ? "On" : "Off"); break;
          case 5: snprintf(b, n, "Re-alert: %s", REPEAT_LABELS[st.repeatIdx]); break;
          case 6: snprintf(b, n, "Brightness: %u", st.brightness); break;
          case 7: snprintf(b, n, "Test buzzer"); break;
          default:
#if PRESENCE_USE_TOF
            if (i == 8) {
              const DistanceSensor &sensor = f.presence.distance;
              snprintf(b, n, "TOF: %s", !st.irEnabled ? "off" : sensor.state() == DistanceSensor::STARTING ? "starting" : sensor.state() == DistanceSensor::SENSOR_ERROR ? "error" : sensor.valid() ? "online" : "no target");
            } else if (i == 9) {
              formatDistance(b, n, f);
            } else if (i == 10) {
              snprintf(b, n, "Person: %s", !st.irEnabled ? "off" : f.presence.present() ? "detected" : "not seen");
            } else snprintf(b, n, "TOF port: %u", f.presence.distance.port());
#elif IR_USE_THERMAL_CAMERA
            if (i == 8) {
              snprintf(b, n, "Camera: %s", !st.irEnabled ? "off" : !f.presence.camera.valid() ? "no data" : f.presence.camera.warming() ? "warming" : "online");
            } else if (i == 9) {
              if (!st.irEnabled || !f.presence.camera.valid()) snprintf(b, n, "Peak: -- C");
              else {
                const int16_t q = f.presence.camera.peakQuarterC();
                const uint16_t a = q < 0 ? -q : q;
                snprintf(b, n, "Peak:%s%u.%02u C", q < 0 ? "-" : "", a / 4, (a % 4) * 25);
              }
            } else if (i == 10) {
              snprintf(b, n, "Target: %s", !st.irEnabled || !f.presence.camera.valid() || f.presence.camera.warming() ? "unknown" : f.presence.rawDetect() ? "warm" : "none");
            } else snprintf(b, n, "Camera port: %u", f.presence.camera.port());
#else
            snprintf(b, n, "IR now: %s", !st.irEnabled ? "off" : f.presence.rawDetect() ? "SEEN" : "clear");
#endif
            break;
        }
        break;
      }
      case SCR_CONFIRM: snprintf(b, n, "%s", i == 0 ? "No, keep it" : "Yes, do it"); break;
      default: b[0] = 0; break;
    }
  }

  void listInput(const InputEvent &ev, Pet &pet, FeatureManager &f, unsigned long now) {
    MenuFrame &fr = top();
    const uint8_t n = itemCount(fr.screen, f);
    if (n == 0) { pop(); return; }
    if (fr.cursor >= n) fr.cursor = n - 1;
    switch (ev.key) {
      case KEY_UP:
        if (fr.cursor > 0) fr.cursor--;
        else if (ev.type == PRESS_SHORT) fr.cursor = n - 1;  // wrap only on single taps
        break;
      case KEY_DOWN:
        if (fr.cursor + 1 < n) fr.cursor++;
        else if (ev.type == PRESS_SHORT) fr.cursor = 0;
        break;
      case KEY_BACK:
        if (ev.type == PRESS_SHORT) pop();
        else if (ev.type == PRESS_LONG) popToHome();
        break;
      case KEY_SELECT:
        if (ev.type == PRESS_SHORT) activate(fr.screen, fr.cursor, pet, f, now);
        break;
      default: break;
    }
  }

  void drawList(TinyScreen &d, Pet &pet, FeatureManager &f) {
    if (!depth_) return;
    MenuFrame &fr = top();
    const uint8_t n = itemCount(fr.screen, f);
    if (n && fr.cursor >= n) fr.cursor = n - 1;
    if (fr.cursor < fr.top) fr.top = fr.cursor;
    if (fr.cursor >= fr.top + ROWS) fr.top = fr.cursor - ROWS + 1;

    char b[TEXT_MAX];
    titleFor(fr.screen, b, sizeof b, f);
    drawText(d, 0, 0, 0, 96, 12, b, UI_TEXT, UI_TITLE);
    for (uint8_t r = 0; r < ROWS; ++r) {
      const uint8_t idx = fr.top + r;
      if (idx < n) {
        itemLabel(fr.screen, idx, b, sizeof b, pet, f);
        const bool sel = idx == fr.cursor;
        drawText(d, 1 + r, 0, 12 + r * 13, 93, 13, b, UI_TEXT, sel ? UI_SEL : UI_BG);
      } else {
        drawText(d, 1 + r, 0, 12 + r * 13, 93, 13, "", UI_TEXT, UI_BG);
      }
    }
    drawScrollbar(d, n, fr.top);
    const char *action = "Open";
    if (fr.screen == SCR_CONFIRM) action = fr.cursor ? "Confirm" : "Cancel";
    else if (fr.screen == SCR_SUBJECTS) action = "Start";
    else if (fr.screen == SCR_SETTINGS) {
      if (fr.cursor == 1 || fr.cursor == 2 || fr.cursor == 4) action = "Toggle";
      else if (fr.cursor == 7) action = "Test";
      else if ((fr.cursor >= 8 && fr.cursor <= 10) || (PRESENCE_USE_TOF && fr.cursor == 3)) action = "--";
      else action = "Edit";
    } else if (fr.screen == SCR_TIMER) {
      if (fr.cursor == 0) action = f.timer.phase() == PHASE_IDLE ? "Start" : f.timer.userPaused() ? "Resume" : "Pause";
      else if (fr.cursor == 1) action = "Skip";
      else if (fr.cursor == 2) action = "Reset";
      else if (fr.cursor == 3) action = "Change";
      else action = "Edit";
    } else if (fr.screen == SCR_DL_EDIT) {
      if (dlEditIndex_ < 0) action = fr.cursor == 0 ? "Save" : "Edit";
      else action = fr.cursor == 4 ? "--" : fr.cursor == 5 ? "Delete" : "Edit";
    } else if (fr.screen == SCR_STUDY && fr.cursor == 0 && f.study.active()) action = "Stop";
    controls(d, "Back", "Up", action, "Down");
  }

  void drawScrollbar(TinyScreen &d, uint8_t count, uint8_t topIdx) {
    const uint16_t sig = (uint16_t)((count << 8) | topIdx);
    if (sig == sigScroll_) return;
    sigScroll_ = sig;
    d.drawRect(93, 12, 3, 39, TSRectangleFilled, UI_BG);
    if (count <= ROWS) return;
    const uint8_t h = (uint8_t)(39 * ROWS / count);
    const uint8_t y = (uint8_t)(12 + 39 * topIdx / count);
    d.drawRect(94, y, 2, h, TSRectangleFilled, UI_DIM);
  }

  void activate(Screen s, uint8_t i, Pet &pet, FeatureManager &f, unsigned long now) {
    switch (s) {
      case SCR_MAIN: {
        static const Screen next[6] = {SCR_TIMER, SCR_STUDY, SCR_DEADLINES, SCR_PET, SCR_SETTINGS, SCR_GUIDE};
        push(next[i]);
        break;
      }
      case SCR_TIMER: {
        const TimerConfig &c = f.timer.config();
        switch (i) {
          case 0: f.timer.toggle(now); break;
          case 1: f.timerSkip(now); break;
          case 2: f.timer.reset(); break;
          case 3: f.timer.cycleStyle(); break;
          case 4: openValue(VT_FOCUS, "FOCUS LENGTH", c.focusMin, 5, 180, 5, nullptr, "min"); break;
          case 5: openValue(VT_SHORT, "SHORT BREAK", c.shortMin, 1, 60, 1, nullptr, "min"); break;
          case 6: openValue(VT_LONG, "LONG BREAK", c.longMin, 5, 90, 5, nullptr, "min"); break;
          default: openValue(VT_CYCLES, "CYCLES", c.cycles, 1, 8, 1, nullptr, "focus blocks"); break;
        }
        break;
      }
      case SCR_STUDY:
        if (i == 0) {
          if (f.study.active()) f.study.stop(now);
          else push(SCR_SUBJECTS);
        } else if (i == 1) push(SCR_DASH);
        else openConfirm(CF_RESET_TOTALS, "RESET ALL HOURS?");
        break;
      case SCR_SUBJECTS:
        f.study.start(i, now);
        pop();
        break;
      case SCR_DEADLINES:
        if (i == 0) {
          if (f.deadlines.full()) break;
          newDraft(f);
          push(SCR_DL_EDIT);
        } else {
          uint8_t order[DeadlineManager::MAX];
          const uint8_t cnt = f.deadlines.sorted(order);
          if (i - 1 < cnt) {
            dlEditIndex_ = (int8_t)order[i - 1];
            draft_ = f.deadlines.get(order[i - 1]);
            push(SCR_DL_EDIT);
          }
        }
        break;
      case SCR_DL_EDIT: {
        const bool isNew = dlEditIndex_ < 0;
        if (isNew && i == 0) {
          f.deadlines.add(draft_, f.nowEpoch());
          pop();
          break;
        }
        switch (isNew ? i - 1 : i) {
          case 0: openValue(VT_DL_SUBJECT, "SUBJECT", draft_.subject, 0, SUBJECT_COUNT - 1, 1, SUBJECT_NAMES, ""); break;
          case 1: openValue(VT_DL_KIND, "TYPE", draft_.kind, 0, KIND_COUNT - 1, 1, KIND_NAMES, ""); break;
          case 2: openDate(DT_DEADLINE, draft_.due); break;
          case 3: openValue(VT_DL_LEAD, "REMIND ME", draft_.lead, 0, LEAD_COUNT - 1, 1, LEAD_LABELS, ""); break;
          case 4: break;  // read-only status row
          default: openConfirm(CF_DELETE_DEADLINE, "DELETE DEADLINE?"); break;
        }
        break;
      }
      case SCR_PET:
        pickPalette_ = (i == 1);
        pickValue_ = pickPalette_ ? pet.palette() : pet.species();
        push(SCR_PET_PICK);
        break;
      case SCR_SETTINGS: {
        Settings &st = f.settings;
        switch (i) {
          case 0: openDate(DT_CLOCK, f.nowEpoch()); break;
          case 1: st.buzzerEnabled = !st.buzzerEnabled; break;
          case 2: st.irEnabled = !st.irEnabled; break;
          case 3:
#if !PRESENCE_USE_TOF
            openValue(VT_AWAY, "AWAY AFTER", st.awayIdx, 0, AWAY_COUNT - 1, 1, AWAY_LABELS, "");
#endif
            break;
          case 4: st.irPausesTimer = !st.irPausesTimer; break;
          case 5: openValue(VT_REPEAT, "RE-ALERT EVERY", st.repeatIdx, 0, REPEAT_COUNT - 1, 1, REPEAT_LABELS, ""); break;
          case 6: openValue(VT_BRIGHT, "BRIGHTNESS", st.brightness, 1, 15, 1, nullptr, ""); break;
          case 7: f.buzzer.play(BEEP_BREAK_START, now); break;
#if PRESENCE_USE_TOF
          case 11: openValue(VT_SENSOR_PORT, "TOF PORT", f.presence.distance.port(), 0, 3, 1, nullptr, ""); break;
#elif IR_USE_THERMAL_CAMERA
          case 11: openValue(VT_SENSOR_PORT, "CAMERA PORT", f.presence.camera.port(), 0, 3, 1, nullptr, ""); break;
#endif
          default: break;
        }
        f.applySettings(now);
        break;
      }
      case SCR_CONFIRM:
        if (i == 1) doConfirm(f);
        else pop();
        break;
      default: break;
    }
  }

  void openConfirm(ConfirmTarget t, const char *title) {
    confirm_ = t;
    confirmTitle_ = title;
    push(SCR_CONFIRM);
  }

  void doConfirm(FeatureManager &f) {
    switch (confirm_) {
      case CF_DELETE_DEADLINE:
        if (dlEditIndex_ >= 0) f.deadlines.remove((uint8_t)dlEditIndex_);
        dlEditIndex_ = -1;
        pop();  // confirm
        pop();  // editor -> back to the list
        break;
      case CF_RESET_TOTALS:
        f.study.resetTotals();
        pop();
        break;
      default: pop(); break;
    }
    confirm_ = CF_NONE;
  }

  void newDraft(FeatureManager &f) {
    memset(&draft_, 0, sizeof(draft_));
    dlEditIndex_ = -1;
    // Default: tomorrow 23:59, remind 1 day before.
    DateTime t = TimeUtil::fromEpoch(f.nowEpoch() + 86400UL);
    t.hour = 23; t.minute = 59; t.second = 0;
    draft_.due = TimeUtil::toEpoch(t);
    draft_.lead = 5;
  }

  void applyDraft(FeatureManager &f) {
    if (dlEditIndex_ >= 0) f.deadlines.replace((uint8_t)dlEditIndex_, draft_, f.nowEpoch());
  }

  // ------------------------------------------------------------------
  //  VALUE EDITOR (numbers or option lists)
  // ------------------------------------------------------------------
  void openValue(ValueTarget t, const char *title, int16_t v, int16_t lo, int16_t hi, int16_t step,
                 const char *const *labels, const char *unit) {
    vt_ = t; vTitle_ = title; value_ = v; vMin_ = lo; vMax_ = hi; vStep_ = step;
    vLabels_ = labels; vUnit_ = unit;
    push(SCR_VALUE);
  }

  void valueInput(const InputEvent &ev, FeatureManager &f, unsigned long now) {
    const bool list = vLabels_ != nullptr;
    if (ev.key == KEY_UP || ev.key == KEY_DOWN) {
      const int16_t dir = (ev.key == KEY_UP) ? 1 : -1;
      int16_t v = value_ + dir * vStep_;
      if (v > vMax_) v = (list && ev.type == PRESS_SHORT) ? vMin_ : vMax_;
      if (v < vMin_) v = (list && ev.type == PRESS_SHORT) ? vMax_ : vMin_;
      value_ = v;
    } else if (ev.key == KEY_SELECT && ev.type == PRESS_SHORT) {
      commitValue(f, now);
      pop();
    } else if (ev.key == KEY_BACK && ev.type == PRESS_SHORT) {
      pop();  // cancel
    }
  }

  void commitValue(FeatureManager &f, unsigned long now) {
    const uint8_t v = (uint8_t)value_;
    switch (vt_) {
      case VT_FOCUS: f.timer.setField(TF_FOCUS, v); break;
      case VT_SHORT: f.timer.setField(TF_SHORT, v); break;
      case VT_LONG: f.timer.setField(TF_LONG, v); break;
      case VT_CYCLES: f.timer.setField(TF_CYCLES, v); break;
      case VT_DL_SUBJECT: draft_.subject = v; applyDraft(f); break;
      case VT_DL_KIND: draft_.kind = v; applyDraft(f); break;
      case VT_DL_LEAD: draft_.lead = v; applyDraft(f); break;
      case VT_AWAY: f.settings.awayIdx = v; f.applySettings(now); break;
      case VT_REPEAT: f.settings.repeatIdx = v; break;
      case VT_BRIGHT: f.settings.brightness = v; break;
#if PRESENCE_USE_TOF
      case VT_SENSOR_PORT: f.presence.distance.setPort(v, now); break;
#elif IR_USE_THERMAL_CAMERA
      case VT_SENSOR_PORT: f.presence.camera.setPort(v, now); break;
#endif
      default: break;
    }
    vt_ = VT_NONE;
  }

  void drawValue(TinyScreen &d) {
    char b[TEXT_MAX];
    drawText(d, 0, 0, 0, 96, 12, vTitle_, UI_TEXT, UI_TITLE);
    drawText(d, 1, 0, 14, 96, 12, "Choose a value", UI_DIM, UI_BG);
    if (vLabels_) snprintf(b, sizeof b, "< %s >", vLabels_[value_]);
    else snprintf(b, sizeof b, "< %d %s >", value_, vUnit_);
    drawText(d, 2, 0, 28, 96, 14, b, UI_TEXT, UI_SEL);
    controls(d, "Cancel", "+", "Save", "-");
  }

  // ------------------------------------------------------------------
  //  DATE / TIME EDITOR  (day, month, year, hour, minute)
  // ------------------------------------------------------------------
  void openDate(DateTarget t, uint32_t epoch) {
    dtTarget_ = t;
    dt_ = TimeUtil::fromEpoch(epoch);
    dt_.second = 0;
    dtField_ = 0;
    push(SCR_DATETIME);
  }

  static int16_t wrap(int16_t v, int16_t lo, int16_t hi) {
    if (v > hi) return lo;
    if (v < lo) return hi;
    return v;
  }

  void dateInput(const InputEvent &ev, FeatureManager &f, unsigned long now) {
    if (ev.key == KEY_UP || ev.key == KEY_DOWN) {
      const int16_t dir = (ev.key == KEY_UP) ? 1 : -1;
      const int16_t big = (ev.type == PRESS_REPEAT && dtField_ == 4) ? 5 : 1;
      switch (dtField_) {
        case 0: dt_.day = wrap(dt_.day + dir, 1, TimeUtil::daysInMonth(dt_.year, dt_.month)); break;
        case 1: dt_.month = wrap(dt_.month + dir, 1, 12); break;
        case 2: dt_.year = wrap(dt_.year + dir, 2024, 2099); break;
        case 3: dt_.hour = wrap(dt_.hour + dir, 0, 23); break;
        default: dt_.minute = wrap(dt_.minute + dir * big, 0, 59); break;
      }
      const uint8_t dim = TimeUtil::daysInMonth(dt_.year, dt_.month);
      if (dt_.day > dim) dt_.day = dim;
    } else if (ev.key == KEY_SELECT && ev.type == PRESS_SHORT) {
      if (dtField_ < 4) dtField_++;
      else {
        const uint32_t e = TimeUtil::toEpoch(dt_);
        if (dtTarget_ == DT_CLOCK) f.clock.set(e, now);
        else { draft_.due = e; applyDraft(f); }
        pop();
      }
    } else if (ev.key == KEY_BACK && ev.type == PRESS_SHORT) {
      if (dtField_ > 0) dtField_--;
      else pop();  // cancel
    }
  }

  void drawDate(TinyScreen &d) {
    char b[TEXT_MAX], dd[6], mm[6], yy[8], hh[6], mi[6];
    snprintf(dd, sizeof dd, dtField_ == 0 ? "[%02u]" : "%02u", dt_.day);
    snprintf(mm, sizeof mm, dtField_ == 1 ? "[%s]" : "%s", TimeUtil::MONTHS[dt_.month - 1]);
    snprintf(yy, sizeof yy, dtField_ == 2 ? "[%u]" : "%u", dt_.year);
    snprintf(hh, sizeof hh, dtField_ == 3 ? "[%02u]" : "%02u", dt_.hour);
    snprintf(mi, sizeof mi, dtField_ == 4 ? "[%02u]" : "%02u", dt_.minute);
    drawText(d, 0, 0, 0, 96, 12, dtTarget_ == DT_CLOCK ? "SET DATE & TIME" : "DUE DATE", UI_TEXT, UI_TITLE);
    static const char *const fields[5] = {"Day", "Month", "Year", "Hour", "Minute"};
    snprintf(b, sizeof b, "%u/5 %s %.3s", dtField_ + 1, fields[dtField_],
             TimeUtil::WEEKDAYS[TimeUtil::weekday(TimeUtil::toEpoch(dt_))]);
    drawText(d, 1, 0, 13, 96, 12, b, UI_DIM, UI_BG);
    snprintf(b, sizeof b, "%s %s %s", dd, mm, yy);
    drawText(d, 2, 0, 26, 96, 12, b, UI_TEXT, dtField_ <= 2 ? UI_SEL : UI_BG);
    snprintf(b, sizeof b, "%s : %s", hh, mi);
    drawText(d, 3, 0, 39, 96, 12, b, UI_TEXT, dtField_ >= 3 ? UI_SEL : UI_BG);
    controls(d, dtField_ ? "Prev" : "Cancel", "+", dtField_ < 4 ? "Next" : "Save", "-");
  }

  // ------------------------------------------------------------------
  //  PET PICKER (live preview)
  // ------------------------------------------------------------------
  void petPickInput(const InputEvent &ev, Pet &pet) {
    const uint8_t n = pickPalette_ ? PALETTES_PER_PET : (uint8_t)PET_COUNT;
    if (ev.key == KEY_UP && ev.type == PRESS_SHORT) pickValue_ = (pickValue_ + n - 1) % n;
    else if (ev.key == KEY_DOWN && ev.type == PRESS_SHORT) pickValue_ = (pickValue_ + 1) % n;
    else if (ev.key == KEY_SELECT && ev.type == PRESS_SHORT) {
      if (pickPalette_) pet.setPalette(pickValue_);
      else pet.setSpecies(pickValue_);
      pop();
    } else if (ev.key == KEY_BACK && ev.type == PRESS_SHORT) pop();
  }

  void drawPetPick(TinyScreen &d, Pet &pet, unsigned long now) {
    char b[TEXT_MAX];
    const uint8_t n = pickPalette_ ? PALETTES_PER_PET : (uint8_t)PET_COUNT;
    const uint8_t sp = pickPalette_ ? pet.species() : pickValue_;
    const uint8_t pal = pickPalette_ ? pickValue_ : pet.palette();
    const char *name = pickPalette_ ? Pet::paletteName(sp, pal) : Pet::speciesName(sp);
    snprintf(b, sizeof b, "%s %u/%u", name, pickValue_ + 1, n);
    drawText(d, 0, 0, 0, 96, 12, b, UI_TEXT, UI_TITLE);
    PetPose p = pet.pose(now);
    p.species = sp;
    p.palette = pal;
    if (!petDrawn_ || !samePose(p, lastPose_)) {
      renderPet(p);
      blitPet(d);
      lastPose_ = p;
      petDrawn_ = true;
    }
    controls(d, "Cancel", "Prev", "Choose", "Next");
  }

  // ------------------------------------------------------------------
  //  HOURS DASHBOARD
  // ------------------------------------------------------------------
  void dashInput(const InputEvent &ev) {
    if (ev.key == KEY_UP && dashTop_ > 0) dashTop_--;
    else if (ev.key == KEY_DOWN && dashTop_ + ROWS < SUBJECT_COUNT) dashTop_++;
    else if ((ev.key == KEY_BACK || ev.key == KEY_SELECT) && ev.type == PRESS_SHORT) pop();
  }

  void drawDash(TinyScreen &d, FeatureManager &f) {
    char b[TEXT_MAX], h[12];
    fmtHours(h, sizeof h, f.study.grandTotal());
    snprintf(b, sizeof b, "HOURS  %.9s", h);
    drawText(d, 0, 0, 0, 96, 12, b, UI_TEXT, UI_TITLE);

    uint32_t maxS = 1;
    for (uint8_t i = 0; i < SUBJECT_COUNT; ++i)
      if (f.study.totalSeconds(i) > maxS) maxS = f.study.totalSeconds(i);

    for (uint8_t r = 0; r < ROWS; ++r) {
      const uint8_t i = dashTop_ + r;
      const uint8_t y = 12 + r * 13;
      LineCache &c = cache_[1 + r];
      if (i >= SUBJECT_COUNT) {
        if (!c.valid || c.text[0]) { d.drawRect(0, y, 93, 13, TSRectangleFilled, UI_BG); c.text[0] = 0; c.valid = true; }
        continue;
      }
      const uint32_t s = f.study.totalSeconds(i);
      const bool live = f.study.active() && f.study.subject() == i;
      const uint8_t barW = (uint8_t)(90UL * s / maxS);
      fmtHours(h, sizeof h, s);
      char key[TEXT_MAX];
      snprintf(key, sizeof key, "%u%s%u%u", i, h, barW, live);
      if (c.valid && strcmp(c.text, key) == 0) continue;
      d.drawRect(0, y, 93, 13, TSRectangleFilled, UI_BG);
      d.fontColor(live ? C8(2, 7, 2) : UI_TEXT, UI_BG);
      d.setCursor(2, y + 1);
      d.print(SUBJECT_NAMES[i]);
      d.setCursor(56, y + 1);
      d.print(h);
      if (barW) d.drawRect(2, y + 10, barW, 2, TSRectangleFilled, live ? C8(0, 7, 2) : C8(1, 4, 6));
      strncpy(c.text, key, TEXT_MAX);
      c.valid = true;
    }
    drawScrollbar(d, SUBJECT_COUNT, dashTop_);
    controls(d, "Back", "Up", "Back", "Down");
  }

  // ------------------------------------------------------------------
  //  DEADLINE ALERT POP-UP
  // ------------------------------------------------------------------
  void drawAlert(TinyScreen &d, FeatureManager &f) {
    const Deadline &dl = f.deadlines.get(alertIndex_);
    if (!dl.used) { pop(); return; }
    const uint32_t e = f.nowEpoch();
    const bool late = f.deadlines.status(alertIndex_, e) == DL_OVERDUE;
    const uint8_t bg = late ? UI_DANGER : C8(2, 2, 0);
    if (bg != alertBg_ || !cache_[0].valid) {
      d.drawRect(0, 0, 96, 64, TSRectangleFilled, bg);
      invalidate();
      alertBg_ = bg;
    }
    char b[TEXT_MAX], t[16];
    drawText(d, 0, 0, 0, 96, 11, late ? "!! OVERDUE !!" : "DEADLINE SOON", UI_TEXT, bg);
    drawText(d, 1, 0, 11, 96, 10, SUBJECT_NAMES[dl.subject], UI_TEXT, bg);
    drawText(d, 2, 0, 21, 96, 10, KIND_NAMES[dl.kind], UI_TEXT, bg);
    fmtDate(t, sizeof t, dl.due);
    snprintf(b, sizeof b, "Due %s", t);
    drawText(d, 3, 0, 31, 96, 10, b, UI_TEXT, bg);
    fmtRelLong(b, sizeof b, (int32_t)(dl.due - e));
    drawText(d, 4, 0, 41, 96, 11, b, C8(3, 7, 0), bg);
    controls(d, "Dismiss", "--", "Dismiss", "--");
  }

  // ------------------------------------------------------------------
  //  BUTTON GUIDE: available any time, with no effect on background timers.
  // ------------------------------------------------------------------
  void guideInput(const InputEvent &ev) {
    if (ev.key == KEY_UP) { if (guidePage_) --guidePage_; }
    else if (ev.key == KEY_DOWN) { if (guidePage_ < 4) ++guidePage_; }
    else if (ev.key == KEY_BACK && ev.type == PRESS_LONG) popToHome();
    else if ((ev.key == KEY_BACK || ev.key == KEY_SELECT) && ev.type == PRESS_SHORT) pop();
  }

  void drawGuide(TinyScreen &d) {
    static const char *const titles[5] = {"BUTTON MAP", "HOME: TAP", "HOME: HOLD", "MENUS", "EDITORS"};
    static const char *const lines[5][4] = {
      {"TL = TOP LEFT", "TR = TOP RIGHT", "BL = BOTTOM LEFT", "BR = BOTTOM RIGHT"},
      {"TL: WIGGLE", "TR: JUMP", "BL: LOVE / HEARTS", "BR: NAP"},
      {"TL: TIMER START/PAUSE", "TR: STUDY START/STOP", "BL: OPEN MAIN MENU", "BR: HOURS DASHBOARD"},
      {"TR / BR: MOVE UP / DOWN", "BL: USE SELECTED ITEM", "TL: BACK   HOLD: HOME", "HOLD TR/BR: SCROLL"},
      {"TR / BR: CHANGE VALUE", "BL: SAVE OR NEXT FIELD", "TL: CANCEL / PREV FIELD", "HOLD = ABOUT 0.7 SEC"}
    };
    char b[TEXT_MAX];
    snprintf(b, sizeof b, "%s %u/5", titles[guidePage_], guidePage_ + 1);
    drawText(d, 0, 0, 0, 96, 12, b, UI_TEXT, UI_TITLE);
    for (uint8_t i = 0; i < 4; ++i) smallLine(d, 1 + i, 16 + i * 9, lines[guidePage_][i]);
    controls(d, "Back", guidePage_ ? "Prev" : "--", "Done", guidePage_ < 4 ? "Next" : "--");
  }

  // ------------------------------------------------------------------
  //  PET SPRITE RENDERER (RAM frame -> one SPI burst, no flicker)
  // ------------------------------------------------------------------
  void blitPet(TinyScreen &d) {
    d.setX(PET_X, PET_X + FRAME_W - 1);
    d.setY(PET_Y, PET_Y + FRAME_H - 1);
    d.startData();
    d.writeBuffer(frame_, sizeof(frame_));
    d.endTransfer();
  }

  void px(int16_t x, int16_t y, uint8_t c) {
    if (x < 0 || y < 0 || x >= FRAME_W || y >= FRAME_H) return;
    frame_[(uint16_t)y * FRAME_W + x] = c;
  }
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c) {
    for (int16_t dy = 0; dy < h; ++dy)
      for (int16_t dx = 0; dx < w; ++dx) px(x + dx, y + dy, c);
  }
  void hline(int16_t x, int16_t y, int16_t w, uint8_t c) { rect(x, y, w, 1, c); }
  void ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint8_t c) {
    // Half-pixel radii (doubled coordinates) avoid 1-px spikes at the tips.
    const int32_t a = 2 * rx + 1, b = 2 * ry + 1;
    const int32_t a2 = a * a, b2 = b * b, lim = a2 * b2;
    for (int16_t y = -ry; y <= ry; ++y)
      for (int16_t x = -rx; x <= rx; ++x)
        if (4L * x * x * b2 + 4L * y * y * a2 <= lim) px(cx + x, cy + y, c);
  }
  void tri(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t c) {
    int16_t minX = x0, maxX = x0, minY = y0, maxY = y0;
    if (x1 < minX) minX = x1;
    if (x2 < minX) minX = x2;
    if (x1 > maxX) maxX = x1;
    if (x2 > maxX) maxX = x2;
    if (y1 < minY) minY = y1;
    if (y2 < minY) minY = y2;
    if (y1 > maxY) maxY = y1;
    if (y2 > maxY) maxY = y2;
    for (int16_t y = minY; y <= maxY; ++y)
      for (int16_t x = minX; x <= maxX; ++x) {
        const int32_t w0 = (int32_t)(x1 - x0) * (y - y0) - (int32_t)(y1 - y0) * (x - x0);
        const int32_t w1 = (int32_t)(x2 - x1) * (y - y1) - (int32_t)(y2 - y1) * (x - x1);
        const int32_t w2 = (int32_t)(x0 - x2) * (y - y2) - (int32_t)(y0 - y2) * (x - x2);
        if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) px(x, y, c);
      }
  }
  // Mirrored pair helpers (left side given, right side = mirror around cx).
  void triPair(int16_t cx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
               uint8_t c, int16_t twX, int16_t twY) {
    tri(x0, y0, x1, y1, x2, y2, c);
    tri(2 * cx - x0, y0, 2 * cx - x1 + twX, y1 + twY, 2 * cx - x2, y2, c);
  }

  // Puppy: droopy teardrop ears, big sparkly eyes, shiny button nose,
  // tongue peeking out, permanent soft blush, cowlick and a collar tag.
  void renderDog(const PetPose &p, const PetPalette &c, int16_t cx, int16_t cy) {
    const uint8_t BLUSH_SOFT = C8(3, 4, 6), BLUSH = C8(3, 2, 4);
    const uint8_t TONGUE = C8(3, 2, 4), TONGUE_DK = C8(2, 1, 3);
    const uint8_t collar = (p.palette == 2) ? C8(0, 3, 7) : C8(3, 0, 1);
    const int16_t perk = p.earTwitch ? 3 : 0;  // right ear lifts on twitch

    // head (slightly narrower than the others so the ears read clearly)
    ellipse(cx, cy, 12, 10, c.edge);
    ellipse(cx, cy, 11, 9, c.fur);
    px(cx - 1, cy - 10, c.fur);  // cowlick
    px(cx, cy - 11, c.fur);
    px(cx + 1, cy - 12, c.fur);
    px(cx + 1, cy - 11, c.fur);

    // Spotty: patch over the right eye
    if (p.palette == 2) ellipse(cx + 6, cy - 2, 4, 4, C8(3, 5, 3));  // tan patch

    // muzzle
    ellipse(cx, cy + 4, 6, 4, c.muzzle);

    // droopy teardrop ears, drawn over the head edge
    for (int8_t side = -1; side <= 1; side += 2) {
      const int16_t lift = (side > 0) ? perk : 0;
      ellipse(cx + side * 12, cy + 1 - lift, 4, 6, c.edge);
      ellipse(cx + side * 12, cy + 1 - lift, 3, 5, c.mark);
      ellipse(cx + side * 9, cy - 6 - lift / 2, 3, 3, c.mark);  // joins the crown
    }

    // cheeks
    const uint8_t blush = p.blush ? BLUSH : BLUSH_SOFT;
    rect(cx - 9, cy + 2, 3, 2, blush);
    rect(cx + 7, cy + 2, 3, 2, blush);

    // big round eyes: 3 x 4 with a two-pixel sparkle
    const int16_t ey = cy - 3;
    for (int8_t side = -1; side <= 1; side += 2) {
      const int16_t x = (side < 0 ? cx - 7 : cx + 5) + p.lookX;
      switch (p.eyes) {
        case EYES_CLOSED:  // content "u" curve
          px(x - 1, ey + 1, INK);
          hline(x, ey + 2, 3, INK);
          px(x + 3, ey + 1, INK);
          break;
        case EYES_HAPPY:  // "^"
          px(x - 1, ey + 2, INK); px(x, ey + 1, INK); px(x + 1, ey, INK);
          px(x + 2, ey + 1, INK); px(x + 3, ey + 2, INK);
          break;
        default:
          rect(x, ey, 3, 4, INK);
          px(x, ey, WHITE8);          // main sparkle
          px(x + 1, ey, WHITE8);
          px(x + 2, ey + 2, C8(2, 5, 7));  // small reflection
          break;
      }
    }

    // shiny button nose
    rect(cx - 2, cy + 1, 5, 2, INK);
    hline(cx - 1, cy + 3, 3, INK);
    px(cx - 1, cy + 1, C8(2, 5, 6));

    // mouth + tongue
    switch (p.mouth) {
      case MOUTH_OPEN:
        rect(cx - 2, cy + 4, 5, 3, C8(1, 0, 0));
        rect(cx - 1, cy + 6, 3, 2, TONGUE);
        break;
      case MOUTH_FLAT:
        hline(cx - 2, cy + 5, 5, INK);
        break;
      default: {  // "w" mouth with a tongue peeking out (longer when smiling)
        px(cx, cy + 4, INK);
        px(cx - 1, cy + 5, INK); px(cx - 2, cy + 5, INK); px(cx - 3, cy + 4, INK);
        px(cx + 1, cy + 5, INK); px(cx + 2, cy + 5, INK); px(cx + 3, cy + 4, INK);
        const int16_t th = (p.mouth == MOUTH_SMILE) ? 3 : 2;
        rect(cx - 1, cy + 6, 3, th, TONGUE);
        px(cx, cy + 6 + th - 1, TONGUE_DK);
        break;
      }
    }

    // collar + tag
    rect(cx - 6, cy + 9, 13, 2, collar);
    rect(cx - 1, cy + 11, 3, 2, C8(3, 6, 0));
  }

  // Rounded rectangle (half-pixel corner radius test keeps corners smooth).
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t col) {
    for (int16_t j = 0; j < h; ++j)
      for (int16_t i = 0; i < w; ++i) {
        int16_t qx = -1, qy = -1;  // corner circle centre (if in a corner zone)
        if (i < r) qx = r; else if (i >= w - r) qx = w - 1 - r;
        if (j < r) qy = r; else if (j >= h - r) qy = h - 1 - r;
        if (qx >= 0 && qy >= 0) {
          const int32_t dx = 2 * (i - qx), dy = 2 * (j - qy), rr = 2 * r + 1;
          if (dx * dx + dy * dy > rr * rr) continue;
        }
        px(x + i, y + j, col);
      }
  }

  // Capybara (kawaii style): tall soft head with full cheeks, small round
  // ears with darker insides, plain oval dot eyes set wide, pink blush, and
  // a darker rounded muzzle patch holding a small nose and "w" mouth.
  void renderCapybara(const PetPose &p, const PetPalette &c, int16_t cx, int16_t cy) {
    const uint8_t blush = p.blush ? C8(3, 2, 4) : C8(3, 3, 4);
    const int16_t twitch = p.earTwitch ? 1 : 0;
    const uint8_t edge = c.edge;   // head + ear outline
    const uint8_t line = c.nose;   // nose, mouth and closed-eye strokes
    const uint8_t patch = c.mark;  // darker muzzle patch

    // small round ears with darker insides (behind the head)
    ellipse(cx - 8, cy - 12, 3, 3, edge);
    ellipse(cx - 8, cy - 12, 2, 2, c.inner);
    ellipse(cx + 8, cy - 12 - twitch, 3, 3, edge);
    ellipse(cx + 8, cy - 12 - twitch, 2, 2, c.inner);

    // tall head: rounded crown + fuller cheeks
    ellipse(cx, cy - 4, 11, 10, edge);
    ellipse(cx, cy + 3, 13, 8, edge);
    ellipse(cx, cy - 4, 10, 9, c.fur);
    ellipse(cx, cy + 3, 12, 7, c.fur);

    // darker rounded muzzle patch, centred low on the face
    ellipse(cx, cy + 2, 6, 5, patch);

    // small nose at the top of the patch
    hline(cx - 1, cy - 1, 3, line);
    px(cx, cy, line);

    // mouth inside the patch
    switch (p.mouth) {
      case MOUTH_OPEN:
        rect(cx - 2, cy + 2, 5, 3, C8(1, 0, 0));
        hline(cx - 1, cy + 4, 3, C8(3, 2, 3));
        break;
      case MOUTH_FLAT:
        hline(cx - 2, cy + 3, 5, line);
        break;
      case MOUTH_SMILE:  // wider, happier "w"
        px(cx, cy + 1, line); px(cx, cy + 2, line);
        px(cx - 4, cy + 2, line); px(cx - 3, cy + 3, line); px(cx - 2, cy + 3, line); px(cx - 1, cy + 3, line);
        px(cx + 4, cy + 2, line); px(cx + 3, cy + 3, line); px(cx + 2, cy + 3, line); px(cx + 1, cy + 3, line);
        break;
      default:  // calm "w" under a short stem
        px(cx, cy + 1, line);
        px(cx - 3, cy + 2, line); px(cx - 2, cy + 3, line); px(cx - 1, cy + 3, line);
        px(cx + 3, cy + 2, line); px(cx + 2, cy + 3, line); px(cx + 1, cy + 3, line);
        break;
    }

    // plain oval dot eyes set wide, level with the top of the patch
    const int16_t ey = cy - 4;
    for (int8_t side = -1; side <= 1; side += 2) {
      const int16_t x = (side < 0 ? cx - 8 : cx + 7) + p.lookX;
      switch (p.eyes) {
        case EYES_CLOSED:  // sleepy
          hline(x - 1, ey + 2, 4, line);
          break;
        case EYES_HAPPY:   // "^ ^"
          px(x - 1, ey + 2, line); px(x, ey + 1, line); px(x + 1, ey + 1, line); px(x + 2, ey + 2, line);
          break;
        default:
          rect(x, ey, 2, 3, c.eye);
          break;
      }
    }

    // pink blush under the eyes
    rect(cx - 11, cy, 3, 2, blush);
    rect(cx + 9, cy, 3, 2, blush);

#if CAPY_MANDARIN
    // optional mandarin on the head (set CAPY_MANDARIN 1 in Pet.h)
    const int16_t mx = cx + 3 + twitch, my = cy - 16;
    ellipse(mx, my, 2, 2, C8(3, 4, 0));
    px(mx - 1, my - 1, C8(3, 6, 2));
    px(mx, my - 3, C8(0, 5, 0)); px(mx + 1, my - 3, C8(0, 5, 0));
#endif
  }

  void renderPet(const PetPose &p) {
    memset(frame_, UI_BG, sizeof(frame_));
    const PetPalette &c = Pet::paletteOf(p.species, p.palette);
    const EarStyle ears = PET_SPECIES[p.species].ears;
    const int16_t cx = 22 + p.dx;
    const int16_t cy = (ears == EAR_LONG ? 27 : 25) + p.dy;
    if (p.species == PET_DOG) {
      renderDog(p, c, cx, cy);
      drawExtras(p, cx, cy);
      return;
    }
    if (p.species == PET_CAPYBARA) {
      renderCapybara(p, c, cx, cy + 1);  // taller head sits 1 px lower
      drawExtras(p, cx, cy + 1);
      return;
    }
    const bool capy = p.species == PET_CAPYBARA;
    const int16_t rx = capy ? 15 : 13, ry = 10;
    const int16_t twX = p.earTwitch ? 2 : 0, twY = p.earTwitch ? 2 : 0;

    // --- ears behind the head ---
    switch (ears) {
      case EAR_POINTY:
        triPair(cx, cx - 12, cy - 3, cx - 10, cy - 15, cx - 3, cy - 8, c.fur, twX, twY);
        triPair(cx, cx - 10, cy - 5, cx - 9, cy - 12, cx - 5, cy - 8, c.inner, twX / 2, twY / 2);
        break;
      case EAR_TALL_POINTY:
        triPair(cx, cx - 13, cy - 2, cx - 11, cy - 18, cx - 2, cy - 8, c.fur, twX, twY);
        triPair(cx, cx - 11, cy - 5, cx - 10, cy - 14, cx - 5, cy - 8, c.inner, twX / 2, twY / 2);
        triPair(cx, cx - 12, cy - 14, cx - 11, cy - 18, cx - 8, cy - 14, c.mark, twX, twY);
        break;
      case EAR_ROUND:
        ellipse(cx - 10, cy - 8, 4, 4, c.mark);
        ellipse(cx + 10 + twX / 2, cy - 8 + twY / 2, 4, 4, c.mark);
        break;
      case EAR_TINY:
        ellipse(cx - 11, cy - 8, 2, 2, c.edge);
        ellipse(cx + 11, cy - 8 + twY / 2, 2, 2, c.edge);
        break;
      case EAR_LONG:
        ellipse(cx - 5, cy - 14, 3, 7, c.fur);
        ellipse(cx - 5, cy - 13, 1, 5, c.inner);
        ellipse(cx + 5 + twX, cy - 14 + twY, 3, 7, c.fur);
        ellipse(cx + 5 + twX, cy - 13 + twY, 1, 5, c.inner);
        break;
      default: break;
    }

    // --- head ---
    ellipse(cx, cy, rx, ry, c.edge);
    ellipse(cx, cy, rx - 1, ry - 1, c.fur);

    // --- face markings ---
    switch (p.species) {
      case PET_CAT:
        ellipse(cx, cy + 4, 5, 3, c.muzzle);
        hline(cx - 12, cy + 2, 4, c.edge);  // whiskers
        hline(cx + 9, cy + 2, 4, c.edge);
        hline(cx - 12, cy + 5, 4, c.edge);
        hline(cx + 9, cy + 5, 4, c.edge);
        break;
      case PET_DOG:
        ellipse(cx, cy + 4, 6, 4, c.muzzle);
        break;
      case PET_FOX:
        ellipse(cx, cy + 5, 9, 4, c.muzzle);
        break;
      case PET_PANDA:
        ellipse(cx - 6, cy - 1, 3, 4, c.mark);
        ellipse(cx + 6, cy - 1, 3, 4, c.mark);
        ellipse(cx, cy + 5, 5, 3, c.muzzle);
        break;
      case PET_CAPYBARA:
        ellipse(cx, cy + 4, 9, 5, c.muzzle);
        break;
      case PET_RABBIT:
        ellipse(cx, cy + 4, 4, 3, c.muzzle);
        break;
      default: break;
    }

    // --- floppy ears go in front of the head ---
    if (ears == EAR_FLOPPY) {
      ellipse(cx - 12, cy - 1, 3, 7, c.mark);
      ellipse(cx + 12, cy - 1 - (p.earTwitch ? 3 : 0), 3, 7, c.mark);
    }

    // --- eyes ---
    const int16_t ex = capy ? 7 : 6;
    const int16_t ey = capy ? cy - 3 : cy - 2;
    const int16_t eh = capy ? 2 : 3;
    const int16_t lx = cx - ex - 1 + p.lookX, rxx = cx + ex + p.lookX;
    const bool lightEye = (c.eye == WHITE8);
    for (uint8_t side = 0; side < 2; ++side) {
      const int16_t x = side ? rxx : lx;
      switch (p.eyes) {
        case EYES_CLOSED:
          hline(x - 1, ey + eh - 1, 4, lightEye ? WHITE8 : INK);
          break;
        case EYES_HAPPY:
          px(x - 1, ey + 2, lightEye ? WHITE8 : INK);
          px(x, ey + 1, lightEye ? WHITE8 : INK);
          px(x + 1, ey + 1, lightEye ? WHITE8 : INK);
          px(x + 2, ey + 2, lightEye ? WHITE8 : INK);
          break;
        default:
          rect(x, ey, 2, eh, c.eye);
          if (lightEye) px(x + (p.lookX >= 0 ? 1 : 0), ey + 1, INK);  // pupil
          else px(x, ey, WHITE8);                                     // sparkle
          break;
      }
    }

    // --- nose + mouth ---
    if (capy) {
      rect(cx - 4, cy + 2, 2, 2, c.nose);  // nostrils
      rect(cx + 3, cy + 2, 2, 2, c.nose);
    } else {
      rect(cx - 1, cy + 2, 3, 2, c.nose);
    }
    const int16_t my = capy ? cy + 6 : cy + 4;
    switch (p.mouth) {
      case MOUTH_SMILE:
        px(cx - 3, my, INK); px(cx - 2, my + 1, INK);
        hline(cx - 1, my + 2, 3, INK);
        px(cx + 2, my + 1, INK); px(cx + 3, my, INK);
        break;
      case MOUTH_OPEN:
        rect(cx - 2, my, 5, 3, C8(1, 0, 0));
        hline(cx - 1, my + 2, 3, C8(3, 2, 3));  // tongue
        break;
      case MOUTH_FLAT:
        hline(cx - 2, my + 1, 5, INK);
        break;
      default:  // little "w"
        px(cx - 2, my, INK); px(cx - 1, my + 1, INK); px(cx, my, INK);
        px(cx + 1, my + 1, INK); px(cx + 2, my, INK);
        if (p.species == PET_RABBIT) rect(cx - 1, my + 2, 3, 2, WHITE8);  // buck teeth
        break;
    }

    if (p.blush) {
      rect(cx - 11, cy + 2, 3, 2, C8(3, 2, 3));
      rect(cx + 9, cy + 2, 3, 2, C8(3, 2, 3));
    }

    drawExtras(p, cx, cy);
  }

  // Floating hearts / Zzz / sparkles, shared by every species.
  void drawExtras(const PetPose &p, int16_t cx, int16_t cy) {
    switch (p.extra) {
      case EXTRA_HEART: {
        static const char *const heart[4] = {".X.X.", "XXXXX", ".XXX.", "..X.."};
        const int16_t hx = cx + 12, hy = cy - 14 - (p.extraPhase % 8);
        for (uint8_t r = 0; r < 4; ++r)
          for (uint8_t q = 0; q < 5; ++q)
            if (heart[r][q] == 'X') px(hx + q, hy + r, C8(3, 1, 3));
        break;
      }
      case EXTRA_ZZZ: {
        static const char *const z[3] = {"XXX", ".X.", "XXX"};
        const int16_t rise = p.extraPhase / 2;
        for (uint8_t k = 0; k < 2; ++k) {
          const int16_t zx = cx + 12 + k * 4, zy = cy - 12 - k * 5 - rise;
          for (uint8_t r = 0; r < 3; ++r)
            for (uint8_t q = 0; q < 3; ++q)
              if (z[r][q] == 'X') px(zx + q, zy + r, C8(2, 6, 7));
        }
        break;
      }
      case EXTRA_SPARKLE: {
        const bool alt = p.extraPhase % 2;
        const int16_t sx = alt ? cx - 16 : cx + 16, sy = alt ? cy - 12 : cy - 9;
        px(sx, sy, C8(3, 7, 0)); px(sx - 1, sy, C8(3, 7, 0)); px(sx + 1, sy, C8(3, 7, 0));
        px(sx, sy - 1, C8(3, 7, 0)); px(sx, sy + 1, C8(3, 7, 0));
        break;
      }
      default: break;
    }
  }
};

#endif
