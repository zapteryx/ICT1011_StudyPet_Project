#include "../../Features.h"
#include <cassert>
static FeatureManager f;
static unsigned long now = 0;
static unsigned checks = 0;
#define CHECK(v) do { assert(v); ++checks; } while (0)
static void tick(unsigned long ms = 100) { now += ms; f.update(now); }
static void frame(uint16_t mm, uint8_t status = 11) {
  Wire.tofReady = true; Wire.tofMm = mm; Wire.tofRangeStatus = status; tick();
}
int main() {
  f.begin(now); f.timer.start(now); f.study.start(0, now); tick(250);
  CHECK(!f.presence.present() && f.timer.awayHeld() && f.study.awayPaused());
  frame(700); frame(700); frame(700);
  CHECK(f.presence.present() && f.presence.distance.millimeters() == 700);
  CHECK(f.timer.counting() && !f.study.awayPaused());
  frame(1000); CHECK(f.presence.present()); // inclusive 1 metre
  frame(1001); CHECK(!f.presence.present() && f.timer.awayHeld());
  const uint32_t remaining = f.timer.remainingSeconds();
  const uint32_t study = f.study.sessionSeconds();
  for (int i = 0; i < 20; ++i) frame(1500);
  CHECK(f.timer.remainingSeconds() == remaining && f.study.sessionSeconds() == study);
  // Return resumes after a short confirmation; time and phase are preserved.
  frame(999); frame(999); frame(999);
  CHECK(f.presence.present() && f.timer.phase() == PHASE_FOCUS);
  // User-requested absence pause also applies during short and long breaks.
  f.timerSkip(now); CHECK(f.timer.onBreak());
  frame(1200); CHECK(f.timer.awayHeld() && !f.timer.counting());
  const auto breakRemaining = f.timer.remainingSeconds();
  frame(1200); frame(1200); CHECK(f.timer.remainingSeconds() == breakRemaining);
  frame(500); frame(500); frame(500); CHECK(f.timer.counting());
  f.timer.pause(); frame(1500); frame(500); frame(500); frame(500);
  CHECK(f.timer.userPaused() && !f.timer.counting()); // manual pause survives return
  f.timer.start(now);
  // Non-range frames and sentinel/too-close values cannot imply a person.
  frame(800, 4); CHECK(!f.presence.present() && !f.presence.distance.valid());
  frame(8190); CHECK(!f.presence.present());
  frame(0); CHECK(!f.presence.present());
  frame(19); CHECK(!f.presence.present());
  frame(20); frame(20); frame(20); CHECK(f.presence.present());
  // Stale data is invalidated instead of retaining a near target forever.
  tick(450); CHECK(!f.presence.present() && !f.presence.distance.valid());
  CHECK(f.presence.distance.state() == DistanceSensor::SENSOR_ERROR);
  tick(2000); frame(800); frame(800); frame(800); CHECK(f.presence.present());
  Wire.tofConnected = false; tick();
  CHECK(!f.presence.present() && f.timer.awayHeld());
  Wire.tofConnected = true; tick(2000); frame(800); frame(800); frame(800);
  CHECK(f.presence.present());
  Wire.tofShortRead = true; tick(); CHECK(!f.presence.present());
  Wire.tofShortRead = false;
  // Port changes must reinitialize and wait for a measurement on the new port.
  Wire.physicalPort = 3; f.presence.distance.setPort(3, now); tick(250);
  CHECK(mockSelectedPort == 3 && !f.presence.present());
  frame(1000); frame(1000); frame(1000); CHECK(f.presence.present());
  // Disabling presence is an explicit override; never cancels a manual pause.
  f.settings.irEnabled = false; f.applySettings(now); tick(); CHECK(f.presence.present());
  f.settings.irEnabled = true; f.applySettings(now); frame(1100); CHECK(!f.presence.present());
  // Clock/deadline time continues to advance while the two study timers hold.
  const auto epoch = f.nowEpoch(); frame(1100); tick(1500); CHECK(f.nowEpoch() > epoch);
  for (uint8_t i = 0; i < 10 && f.timer.phase() != PHASE_LONG_BREAK; ++i) f.timerSkip(now);
  frame(1100); CHECK(f.timer.phase() == PHASE_LONG_BREAK && f.timer.awayHeld());
  const auto longRemaining = f.timer.remainingSeconds();
  frame(1100); CHECK(f.timer.remainingSeconds() == longRemaining);
  // The buzzer remains on A0 while I2C selects sensor port 3.
  f.buzzer.play(BEEP_BREAK_START, now); CHECK(lastTonePin == A0);
  printf("PASS: %u TOF boundary, pause/resume, error/retry, and port checks.\n", checks);
}
