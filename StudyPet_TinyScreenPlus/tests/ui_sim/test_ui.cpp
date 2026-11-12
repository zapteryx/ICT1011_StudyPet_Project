// Desktop UI regression + pixel preview harness. No hardware upload occurs.
// Compile with this directory first on the include path, then the installed
// TinyScreen library directory (for its actual font.h bitmap data).
#define IR_USE_THERMAL_CAMERA 0 // legacy digital presence; thermal tests are separate
#define PRESENCE_USE_TOF 0
#include "Arduino.h"
#include "TinyScreen.h"
#define private public
#include "../../Input.h"
#include "../../Pet.h"
#include "../../Features.h"
#include "../../UI.h"
#undef private

static TinyScreen display;
static FeatureManager features;
static Pet pet;
static StudyPetUI ui;
static unsigned long now = 100;
static std::string outputDir;
static int checks = 0;
#define CHECK(condition) do { assert(condition); ++checks; } while (0)

static void tick(unsigned long ms = 50) {
  now += ms;
  features.update(now);
  pet.setAway(features.settings.irEnabled && !features.presence.present());
  pet.setOnBreak(features.timer.onBreak()); pet.update(now);
  ui.update(display, pet, features, now);
}
static void press(NavKey key, PressType type = PRESS_SHORT) {
  ui.handleInput({key, type}, pet, features, now); tick();
}
static void screen(Screen s, uint8_t cursor = 0) {
  ui.popToHome(); if (s != SCR_HOME) { ui.push(s); ui.top().cursor = cursor; }
  tick();
}
static void snapshot(const char *name) {
  // Cached drawing must produce exactly the same pixels as a full redraw.
  const auto cached = display.pixels;
  ui.needClear_ = true; ui.update(display, pet, features, now);
  CHECK(cached == display.pixels);
  display.save(outputDir + "/" + name + ".ppm");
}

int main(int argc, char **argv) {
  assert(argc == 2); outputDir = argv[1];
  features.begin(now); pet.begin(now); ui.begin(display, now);
#if USE_WIRELING_ADAPTER
  CHECK(Wireling.beginCalls == 1);
#endif
  features.settings.irEnabled = false; features.applySettings(now); tick();
  snapshot("01_home");
  press(KEY_UP); CHECK(pet.actionActive());
  press(KEY_SELECT, PRESS_LONG); CHECK(ui.current() == SCR_MAIN);
  snapshot("02_menu");
  // Scroll to the newly added guide without losing any original menu entries.
  for (int i = 0; i < 5; ++i) press(KEY_DOWN);
  CHECK(ui.top().cursor == 5); press(KEY_SELECT); CHECK(ui.current() == SCR_GUIDE);
  for (int i = 0; i < 5; ++i) {
    const std::string name = "guide_" + std::to_string(i + 1);
    snapshot(name.c_str()); press(KEY_DOWN);
  }
  CHECK(ui.guidePage_ == 4); press(KEY_UP); CHECK(ui.guidePage_ == 3);
  press(KEY_SELECT); CHECK(ui.current() == SCR_MAIN);
  press(KEY_BACK, PRESS_LONG); CHECK(ui.current() == SCR_HOME);

  // Existing home shortcuts and independent background timekeeping.
  press(KEY_BACK, PRESS_LONG); CHECK(features.timer.phase() == PHASE_FOCUS);
  auto remaining = features.timer.remainingSeconds(); tick(1500);
  CHECK(features.timer.remainingSeconds() < remaining);
  press(KEY_UP, PRESS_LONG); CHECK(features.study.active());
  tick(2100); CHECK(features.study.sessionSeconds() >= 2);
  screen(SCR_TIMER); snapshot("03_timer");
  // Cancel versus save in value editors.
  ui.top().cursor = 4; press(KEY_SELECT); CHECK(ui.current() == SCR_VALUE);
  snapshot("04_value");
  press(KEY_UP); press(KEY_BACK); CHECK(features.timer.config().focusMin == 25);
  press(KEY_SELECT); press(KEY_UP); press(KEY_SELECT);
  CHECK(features.timer.config().focusMin == 30);
  CHECK(features.timer.style() == STYLE_CUSTOM);
  screen(SCR_SETTINGS); press(KEY_SELECT); CHECK(ui.current() == SCR_DATETIME);
  snapshot("05_date");
  const uint32_t beforeDate = features.nowEpoch(); press(KEY_UP);
  for (int i = 0; i < 5; ++i) press(KEY_SELECT);
  CHECK(ui.current() == SCR_SETTINGS); CHECK(features.nowEpoch() > beforeDate + 86000UL);
  snapshot("06_settings");

  // Every species and palette still supports preview, cancel, and choose.
  screen(SCR_PET); press(KEY_SELECT); CHECK(ui.current() == SCR_PET_PICK);
  for (int i = 0; i < PET_COUNT; ++i) {
    tick(); const std::string name = "pet_" + std::to_string(i);
    snapshot(name.c_str()); press(KEY_DOWN);
  }
  CHECK(pet.species() == PET_CAT); press(KEY_BACK);
  press(KEY_SELECT); for (int i = 0; i < PET_CAPYBARA; ++i) press(KEY_DOWN);
  press(KEY_SELECT); CHECK(pet.species() == PET_CAPYBARA);
  press(KEY_DOWN); press(KEY_SELECT); press(KEY_DOWN); press(KEY_SELECT);
  CHECK(pet.palette() == 1);
  screen(SCR_HOME); snapshot("07_capybara_home");

  // Deadline create/edit/delete, and alerts returning to the interrupted editor.
  screen(SCR_DEADLINES); press(KEY_SELECT); CHECK(ui.current() == SCR_DL_EDIT);
  snapshot("08_deadline_new"); press(KEY_SELECT); CHECK(features.deadlines.count() == 1);
  press(KEY_DOWN); press(KEY_SELECT); CHECK(ui.dlEditIndex_ == 0);
  ui.top().cursor = 1; press(KEY_SELECT); press(KEY_UP); press(KEY_SELECT);
  CHECK(features.deadlines.get(0).kind == 1);
  snapshot("09_deadline_edit");
  ui.top().cursor = 2; press(KEY_SELECT); CHECK(ui.current() == SCR_DATETIME);
  const auto interruptedDepth = ui.depth_;
  features.deadlines.items_[0].due = features.nowEpoch() + 1800;
  features.deadlines.items_[0].stage = DL_UPCOMING;
  features.deadlines.items_[0].acked = false;
  tick(1100); CHECK(ui.current() == SCR_ALERT); snapshot("10_alert_soon");
  press(KEY_SELECT); CHECK(ui.current() == SCR_DATETIME); CHECK(ui.depth_ == interruptedDepth);
  features.deadlines.items_[0].due = features.nowEpoch();
  tick(1100); CHECK(ui.current() == SCR_ALERT); snapshot("11_alert_overdue");
  press(KEY_BACK); CHECK(ui.current() == SCR_DATETIME);
  screen(SCR_DL_EDIT, 5); press(KEY_SELECT); CHECK(ui.current() == SCR_CONFIRM);
  snapshot("12_confirm"); press(KEY_DOWN); press(KEY_SELECT);
  CHECK(features.deadlines.count() == 0);

  // Away hold remains distinct from a user pause; breaks keep advancing.
  features.settings.irEnabled = true; features.applySettings(now);
  tick(61000); CHECK(features.timer.awayHeld()); CHECK(features.study.awayPaused());
  screen(SCR_TIMER); snapshot("13_timer_away");
  screen(SCR_HOME); snapshot("14_home_away");
  remaining = features.timer.remainingSeconds(); tick(2000);
  CHECK(features.timer.remainingSeconds() == remaining);
  features.timerSkip(now); CHECK(features.timer.onBreak());
  remaining = features.timer.remainingSeconds(); tick(1500);
  CHECK(features.timer.remainingSeconds() < remaining);
  sensorLevel = HIGH; tick(); tick(200); CHECK(features.presence.present());

  features.study.totals_[0] = 7500; features.study.totals_[1] = 2100;
  screen(SCR_DASH); snapshot("15_dashboard"); press(KEY_DOWN); press(KEY_DOWN);
  CHECK(ui.dashTop_ == 2); snapshot("16_dashboard_scrolled");
  press(KEY_SELECT); CHECK(ui.current() == SCR_HOME);
  screen(SCR_STUDY, 2); press(KEY_SELECT); press(KEY_DOWN); press(KEY_SELECT);
  CHECK(features.study.grandTotal() == 0);
  screen(SCR_SUBJECTS); press(KEY_DOWN); press(KEY_DOWN); press(KEY_SELECT);
  CHECK(features.study.subject() == 2);

  // Longest countdown must retain all digits next to the clock.
  features.settings.irEnabled = false; features.applySettings(now);
  features.timer.reset(); features.timer.setField(TF_FOCUS, 180); features.timer.start(now);
  screen(SCR_HOME); snapshot("17_long_countdown");
  CHECK(std::find(display.printed.begin(), display.printed.end(), "180:00") != display.printed.end());
  press(KEY_BACK, PRESS_LONG); CHECK(features.timer.userPaused()); snapshot("18_user_paused");

  // Render every list selection, including lower rows and read-only actions.
  const Screen lists[] = {SCR_MAIN, SCR_TIMER, SCR_STUDY, SCR_SUBJECTS, SCR_DEADLINES, SCR_PET, SCR_SETTINGS};
  for (Screen list : lists) {
    screen(list);
    const auto count = ui.itemCount(list, features);
    for (uint8_t i = 0; i < count; ++i) {
      CHECK(ui.top().cursor == i);
      const auto cached = display.pixels;
      ui.needClear_ = true; ui.update(display, pet, features, now);
      CHECK(cached == display.pixels);
      press(KEY_DOWN);
    }
  }
  printf("PASS: %d UI/state/cache checks; all draw coordinates within 96x64.\n", checks);
}
