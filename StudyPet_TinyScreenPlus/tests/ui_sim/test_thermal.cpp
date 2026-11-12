#define PRESENCE_USE_TOF 0
#define IR_USE_THERMAL_CAMERA 1
#include "../../Features.h"
#include <cassert>
int main() {
  Wire.fill(100); // 25 C background
  FeatureManager f;
  f.begin(0);
  assert(Wireling.beginCalls == 1);
  f.update(250); f.update(500);
  assert(f.presence.camera.valid() && f.presence.camera.warming());
  assert(f.presence.camera.peakQuarterC() == 100);
  assert(f.presence.present());
  // A warm surface occupies several pixels against a cooler background.
  Wire.pixel(3, 132); Wire.pixel(4, 132); Wire.pixel(11, 132);
  f.update(16000); f.update(16250);
  assert(!f.presence.camera.warming());
  assert(f.presence.rawDetect() && f.presence.present());
  assert(f.presence.camera.peakQuarterC() == 132);
  // Timer and study tracking freeze only after a valid absence timeout.
  f.presence.setAbsentTimeout(1000);
  f.timer.start(16250); f.study.start(0, 16250);
  Wire.fill(100); f.update(16500); f.update(17750);
  assert(!f.presence.present() && f.timer.awayHeld() && f.study.awayPaused());
  // Disconnection must report unknown and release the away hold.
  Wire.cameraConnected = false; f.update(18000);
  assert(!f.presence.camera.valid() && f.presence.present());
  assert(!f.timer.awayHeld() && !f.study.awayPaused());
  // Retry / re-stabilization and signed quarter-degree conversion.
  Wire.cameraConnected = true; Wire.fill(-1);
  f.update(20000); f.update(20250);
  assert(f.presence.camera.valid() && f.presence.camera.peakQuarterC() == -1);
  Wire.shortRead = true; f.update(20500);
  assert(!f.presence.camera.valid() && f.presence.present());
  Wire.shortRead = false; Wire.physicalPort = 3;
  f.presence.camera.setPort(3, 21000);
  f.update(21250); f.update(21500);
  assert(mockSelectedPort == 3 && f.presence.camera.valid());
  Wire.fill(100); Wire.pixel(0, 132); f.update(37000); f.update(37250);
  assert(!f.presence.rawDetect()); // one hot pixel does not satisfy the rule
  // AST1035 stays on its GPIO even while the mux selects camera port 3.
  f.buzzer.play(BEEP_BREAK_START, 38000);
  assert(lastTonePin == A0 && lastToneHz == 2600);
  const unsigned count = toneCalls;
  f.buzzer.update(38120); f.buzzer.update(38230);
  assert(toneCalls == count + 1);
  f.buzzer.setEnabled(false); f.buzzer.play(BEEP_BREAK_START, 39000);
  assert(toneCalls == count + 1);
  puts("PASS: thermal frames, warmup, presence, sensor loss/retry, signed temperatures, ports, and buzzer patterns.");
}
