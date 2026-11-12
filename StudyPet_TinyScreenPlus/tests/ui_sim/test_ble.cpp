#include "../../BleProtocol.h"
#include <cassert>
#include <string>
static FeatureManager f;
static Pet pet;
static BleProtocol protocol;
static unsigned long now = 100;
static unsigned checks = 0;
#define CHECK(v) do { assert(v); ++checks; } while (0)
static std::string drain() {
  std::string result; uint8_t bytes[20];
  while (protocol.pending()) {
    const uint8_t n = protocol.chunk(bytes); CHECK(n && n <= 20);
    // Reading a chunk does not dequeue it: retries must send exactly the same bytes.
    uint8_t retry[20]; CHECK(protocol.chunk(retry) == n && !memcmp(bytes, retry, n));
    result.append(reinterpret_cast<char *>(bytes), n); protocol.sent(n);
  }
  return result;
}
static void feed(const std::string &request) {
  for (size_t offset = 0; offset < request.size(); offset += 20) {
    const size_t n = request.size() - offset < 20 ? request.size() - offset : 20;
    protocol.receive(reinterpret_cast<const uint8_t *>(request.data() + offset), n, now);
  }
}
static std::string command(const std::string &request) {
  feed(request + "\n"); protocol.tick(f, pet, now); now += 10;
  return drain();
}
static bool type(const std::string &reply, const char *expected) {
  StaticJsonDocument<4096> doc;
  CHECK(!deserializeJson(doc, reply));
  return doc["type"] == expected;
}
int main() {
  f.begin(now); pet.begin(now); f.settings.irEnabled = false; f.applySettings(now); f.update(now);
  CHECK(type(command("{}"), "ERROR"));
  CHECK(type(command("[]"), "ERROR"));
  CHECK(type(command("{broken}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"TIMER_START\"}garbage"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"TIMER_START\"}{\"cmd\":\"TIMER_PAUSE\"}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"TIMER_START\\u0000extra\"}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"TIMER_\rSTART\"}"), "ERROR"));
  CHECK(type(command("{\"cmd\":null}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"NO_SUCH_COMMAND\"}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"TIMER_START\",\"data\":2}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"TIMER_START\",\"id\":{}}"), "ERROR"));
  CHECK(f.timer.phase() == PHASE_IDLE);
  // Split commands, NUL terminators, CRLF, multiple commands, and correlation IDs.
  feed("{\"cmd\":\"TIMER_"); protocol.tick(f, pet, now); CHECK(!protocol.pending());
  feed("START\",\"id\":\"test-1\"}\r\n"); protocol.tick(f, pet, now);
  auto reply = drain(); CHECK(type(reply, "ACK") && reply.find("test-1") != std::string::npos);
  CHECK(f.timer.phase() == PHASE_FOCUS);
  const uint8_t nul[] = "{\"cmd\":\"TIMER_PAUSE\"}";
  protocol.receive(nul, sizeof nul, now); protocol.tick(f, pet, now); CHECK(type(drain(), "ACK"));
  CHECK(f.timer.userPaused());
  feed("{\"cmd\":\"TIMER_START\"}\n{\"cmd\":\"TIMER_PAUSE\"}\n");
  protocol.tick(f, pet, now); CHECK(!f.timer.userPaused()); drain();
  protocol.tick(f, pet, now); CHECK(f.timer.userPaused()); drain();
  CHECK(type(command("{\"cmd\":\"TIMER_CONFIG\",\"data\":{\"style\":200}}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"TIMER_CONFIG\",\"data\":{\"style\":2,\"focusMin\":30,\"shortMin\":4,\"longMin\":0,\"cycles\":4}}"), "ERROR"));
  CHECK(f.timer.style() == STYLE_POMODORO && f.timer.config().focusMin == 25);
  CHECK(type(command("{\"cmd\":\"TIMER_CONFIG\",\"data\":{\"style\":2,\"focusMin\":30,\"shortMin\":4,\"longMin\":20,\"cycles\":3}}"), "ACK"));
  CHECK(f.timer.style() == STYLE_CUSTOM && f.timer.config().focusMin == 30 && f.timer.config().cycles == 3);
  CHECK(type(command("{\"cmd\":\"TIMER_SKIP\"}"), "ACK")); CHECK(f.timer.onBreak());
  CHECK(type(command("{\"cmd\":\"TIMER_RESET\"}"), "ACK")); CHECK(f.timer.phase() == PHASE_IDLE);
  CHECK(type(command("{\"cmd\":\"STUDY_START\",\"data\":{\"subject\":99}}"), "ERROR")); CHECK(!f.study.active());
  CHECK(type(command("{\"cmd\":\"STUDY_START\",\"data\":{\"subject\":2}}"), "ACK")); CHECK(f.study.subject() == 2);
  CHECK(type(command("{\"cmd\":\"STUDY_STOP\"}"), "ACK")); CHECK(!f.study.active());
  CHECK(type(command("{\"cmd\":\"STUDY_GET_HOURS\"}"), "HOURS"));
  CHECK(type(command("{\"cmd\":\"STUDY_RESET_HOURS\"}"), "ACK"));
  CHECK(type(command("{\"cmd\":\"PET_SET\",\"data\":{\"species\":4,\"palette\":2}}"), "ACK"));
  CHECK(pet.species() == PET_CAPYBARA && pet.palette() == 2);
  CHECK(type(command("{\"cmd\":\"PET_SET\",\"data\":{\"species\":0,\"palette\":100}}"), "ERROR"));
  CHECK(pet.species() == PET_CAPYBARA);
  // Atomic settings: one invalid field must not apply other valid fields.
  CHECK(type(command("{\"cmd\":\"SETTINGS_SET\",\"data\":{\"brightness\":3,\"awayIdx\":255}}"), "ERROR"));
  CHECK(f.settings.brightness == 10);
  CHECK(type(command("{\"cmd\":\"SETTINGS_SET\",\"data\":{\"brightness\":3,\"buzzerEnabled\":false}}"), "ACK"));
  CHECK(f.settings.brightness == 3 && !f.settings.buzzerEnabled && !f.settings.irEnabled);
  CHECK(type(command("{\"cmd\":\"SETTINGS_SET\",\"data\":{\"buzzerEnabled\":1}}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"SETTINGS_SET\",\"data\":{\"unknown\":true}}"), "ERROR"));
  CHECK(type(command("{\"cmd\":\"SETTINGS_SET\",\"data\":{\"tofPort\":3}}"), "ACK")); CHECK(f.presence.distance.port() == 3);
  CHECK(type(command("{\"cmd\":\"BUZZER_TEST\"}"), "ACK"));
  // Wire timestamps are Unix UTC; the software clock and stored deadlines are local.
  CHECK(type(command("{\"cmd\":\"SET_DATETIME\",\"data\":{\"timestamp\":1704067200,\"utcOffsetMinutes\":480}}"), "ACK"));
  CHECK(f.nowEpoch() == 757411200UL); // 2024-01-01 08:00 local
  const auto epoch = f.nowEpoch();
  CHECK(type(command("{\"cmd\":\"SET_DATETIME\",\"data\":{\"timestamp\":1704067200,\"utcOffsetMinutes\":9999}}"), "ERROR"));
  CHECK(f.nowEpoch() == epoch);
  CHECK(type(command("{\"cmd\":\"SET_DATETIME\",\"data\":{\"timestamp\":0}}"), "ERROR"));
  reply = command("{\"cmd\":\"GET_STATE\",\"id\":7}"); CHECK(type(reply, "STATE"));
  StaticJsonDocument<4096> state; CHECK(!deserializeJson(state, reply));
  CHECK(state["timestamp"] == 1704067200UL && state["utcOffsetMinutes"] == 480 && state["id"] == 7);
  CHECK(state["range"]["millimeters"].isNull() && state["range"]["cutoffMm"] == 1000);
  // Persistent deadline CRUD, stable indices even though the reply is sorted.
  const std::string entry = "{\"subject\":1,\"kind\":2,\"lead\":5,\"due\":1800000000}";
  CHECK(type(command("{\"cmd\":\"DEADLINE_ADD\",\"data\":" + entry + "}"), "ACK"));
  CHECK(f.deadlines.count() == 1);
  CHECK(f.deadlines.get(0).due == 1800000000UL - 946684800UL + 28800UL);
  DeadlineManager reboot; reboot.begin(f.nowEpoch()); CHECK(reboot.count() == 1);
  CHECK(type(command("{\"cmd\":\"DEADLINE_UPDATE\",\"data\":{\"index\":0,\"deadline\":{\"subject\":2,\"kind\":1,\"lead\":0,\"due\":1800001000}}}"), "ACK"));
  CHECK(f.deadlines.get(0).subject == 2);
  CHECK(type(command("{\"cmd\":\"DEADLINE_UPDATE\",\"data\":{\"index\":0,\"deadline\":{}}}"), "ERROR"));
  Wire.eepromReadOnly = true;
  CHECK(type(command("{\"cmd\":\"DEADLINE_DELETE\",\"data\":{\"index\":0}}"), "ERROR")); CHECK(f.deadlines.count() == 1);
  Wire.eepromReadOnly = false;
  CHECK(type(command("{\"cmd\":\"DEADLINE_DELETE\",\"data\":{\"index\":0}}"), "ACK")); CHECK(f.deadlines.count() == 0);
  CHECK(type(command("{\"cmd\":\"DEADLINE_DELETE\",\"data\":{\"index\":100}}"), "ERROR"));
  for (int i = 0; i < 8; ++i) CHECK(type(command("{\"cmd\":\"DEADLINE_ADD\",\"data\":" + entry + "}"), "ACK"));
  CHECK(type(command("{\"cmd\":\"DEADLINE_ADD\",\"data\":" + entry + "}"), "ERROR"));
  reply = command("{\"cmd\":\"DEADLINES_GET\"}"); CHECK(type(reply, "DEADLINES"));
  CHECK(!deserializeJson(state, reply)); CHECK(state["deadlines"].size() == 8);
  CHECK(state["deadlines"][7]["index"] == 7 && state["deadlines"][0]["due"] == 1800000000UL);
  // Overlong/partial commands cannot execute their tails; reset drops old sessions.
  feed(std::string(400, 'x') + "\n"); protocol.tick(f, pet, now); CHECK(type(drain(), "ERROR"));
  feed("{\"cmd\":\"TIMER_START\""); now += 5001; protocol.tick(f, pet, now); CHECK(type(drain(), "ERROR"));
  feed("{\"cmd\":\"TIMER_"); protocol.reset(); feed("START\"}\n"); protocol.tick(f, pet, now); CHECK(type(drain(), "ERROR"));
  CHECK(f.timer.phase() == PHASE_IDLE);
  // Backpressure: a fourth response cannot silently drop its ACK after mutation.
  for (int i = 0; i < 3; ++i) { feed("{\"cmd\":\"GET_STATE\"}\n"); protocol.tick(f, pet, now); }
  feed("{\"cmd\":\"TIMER_START\"}\n"); protocol.tick(f, pet, now); CHECK(f.timer.phase() == PHASE_IDLE);
  drain(); protocol.tick(f, pet, now); CHECK(type(drain(), "ACK")); CHECK(f.timer.phase() == PHASE_FOCUS);
  protocol.reset(); CHECK(!protocol.pending() && protocol.idle());
  protocol.telemetry(f, pet, false); reply = drain();
  const auto split = reply.find('\n'); CHECK(split != std::string::npos);
  CHECK(type(reply.substr(0, split + 1), "TIMER"));
  CHECK(type(reply.substr(split + 1), "STUDY"));
  f.settings.irEnabled = true; f.applySettings(now); f.update(now);
  CHECK(type(command("{\"cmd\":\"TIMER_START\"}"), "ACK")); f.update(now);
  CHECK(f.timer.awayHeld()); // BLE start cannot bypass TOF absence
  printf("PASS: %u BLE JSON/framing/validation/persistence checks.\n", checks);
}
