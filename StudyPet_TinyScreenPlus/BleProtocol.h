#ifndef STUDYPET_BLE_PROTOCOL_H
#define STUDYPET_BLE_PROTOCOL_H
#if defined(ARDUINO)
// The TinyCircuits SAMD core defines min/max macros that conflict with the
// C++ iostream headers. Firmware uses only fixed char buffers, never streams.
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#define ARDUINOJSON_ENABLE_STD_STRING 0
#endif
#include <ArduinoJson.h>
#include "Features.h"
#include "Pet.h"

// Hardware-independent, bounded NDJSON command protocol. Neither the receiver
// callback nor notifications perform sleeps; tick executes at most one command.
class BleProtocol {
public:
  enum Change : uint8_t { OTHER = 1, DEADLINES = 2 };
  static const uint16_t MAX_COMMAND = 384, MAX_REPLY = 1152;
  void reset() {
    lineLength_ = commandHead_ = commandCount_ = replyHead_ = replyCount_ = 0;
    replyOffset_ = 0; discarding_ = false; pendingError_ = nullptr; changes_ = 0;
  }
  void receive(const uint8_t *data, uint8_t length, unsigned long now) {
    for (uint8_t i = 0; i < length; ++i) {
      const char c = data[i];
      lastRx_ = now;
      if (c == '\n' || c == '\0') {
        if (discarding_) pendingError_ = "Command too long";
        else if (lineLength_) {
          line_[lineLength_] = 0;
          if (commandCount_ < 2) {
            strcpy(commands_[(commandHead_ + commandCount_) % 2], line_); ++commandCount_;
          } else pendingError_ = "Busy: wait for a reply";
        }
        lineLength_ = 0; discarding_ = false;
      } else if (!discarding_) {
        if (lineLength_ < MAX_COMMAND) line_[lineLength_++] = c;
        else { discarding_ = true; lineLength_ = 0; }
      }
    }
  }
  void tick(FeatureManager &f, Pet &pet, unsigned long now) {
    if ((lineLength_ || discarding_) && (unsigned long)(now - lastRx_) >= 5000UL) {
      lineLength_ = 0; discarding_ = false; pendingError_ = "Incomplete command timed out";
    }
    if (replyCount_ >= 3) return; // never execute a mutation without room for its reply
    if (commandCount_) {
      process(commands_[commandHead_], f, pet, now);
      commandHead_ = (commandHead_ + 1) % 2; --commandCount_;
    } else if (pendingError_) {
      response_.clear(); error(pendingError_); pendingError_ = nullptr; enqueue();
    }
  }
  bool pending() const { return replyCount_ != 0; }
  bool idle() const { return commandCount_ == 0 && replyCount_ == 0 && !lineLength_ && !discarding_ && !pendingError_; }
  uint8_t chunk(uint8_t *out) const {
    if (!replyCount_) return 0;
    uint16_t n = replyLength_[replyHead_] - replyOffset_;
    if (n > 20) n = 20;
    memcpy(out, replies_[replyHead_] + replyOffset_, n); return n;
  }
  void sent(uint8_t count) {
    if (!replyCount_) return;
    replyOffset_ += count;
    if (replyOffset_ >= replyLength_[replyHead_]) {
      replyOffset_ = 0; replyHead_ = (replyHead_ + 1) % 3; --replyCount_;
    }
  }
  uint8_t takeChanges() { const uint8_t changes = changes_; changes_ = 0; return changes; }
  void telemetry(const FeatureManager &f, const Pet &pet, bool full) {
    if (replyCount_ >= 3) return;
    response_.clear();
    if (full) fullState(f, pet);
    else {
      response_["type"] = "TIMER";
      timerState(response_.as<JsonObject>(), f);
    }
    enqueue();
    if (!full && replyCount_ < 3) {
      response_.clear(); response_["type"] = "STUDY";
      studyState(response_.as<JsonObject>(), f); enqueue();
    }
  }
private:
  char line_[MAX_COMMAND + 1] = {};
  char commands_[2][MAX_COMMAND + 1] = {};
  char replies_[3][MAX_REPLY] = {};
  uint16_t replyLength_[3] = {}, lineLength_ = 0, replyOffset_ = 0;
  uint8_t commandHead_ = 0, commandCount_ = 0, replyHead_ = 0, replyCount_ = 0, changes_ = 0;
  bool discarding_ = false;
  unsigned long lastRx_ = 0;
  const char *pendingError_ = nullptr;
  int16_t utcOffsetMinutes_ = 480; // StudyPet display/due dates are local Singapore time
  // Capacity follows ArduinoJson's architecture-specific slot size so native
  // tests and the 32-bit SAMD21 can both fit all eight deadline objects.
  StaticJsonDocument<JSON_OBJECT_SIZE(4) + JSON_ARRAY_SIZE(8) + 8 * JSON_OBJECT_SIZE(7) + 128> response_;

  static bool number(JsonVariantConst v, uint32_t lo, uint32_t hi, uint32_t &value) {
    if (!v.is<uint32_t>()) return false;
    value = v.as<uint32_t>(); return value >= lo && value <= hi;
  }
  static bool singleObject(const char *text) {
    // ArduinoJson permits a second root after an enclosed value. A BLE line
    // must instead contain exactly one complete object, with no trailing text.
    while (*text == ' ' || *text == '\t' || *text == '\r') ++text;
    if (*text != '{') return false;
    uint16_t depth = 0; bool quoted = false, escape = false;
    for (; *text; ++text) {
      const char c = *text;
      if (quoted) {
        if (escape) escape = false;
        else if (c == '\\') escape = true;
        else if (c == '"') quoted = false;
      } else if (c == '"') quoted = true;
      else if (c == '{' || c == '[') ++depth;
      else if (c == '}' || c == ']') {
        if (!depth || --depth == 0) {
          ++text;
          while (*text == ' ' || *text == '\t' || *text == '\r') ++text;
          return *text == 0;
        }
      }
    }
    return false;
  }
  void ack(const char *message) { response_["type"] = "ACK"; response_["message"] = message; }
  void error(const char *message) { response_["type"] = "ERROR"; response_["message"] = message; }
  void enqueue() {
    if (replyCount_ >= 3) return;
    const uint8_t index = (replyHead_ + replyCount_) % 3;
    size_t size = measureJson(response_);
    if (response_.overflowed() || size + 2 > MAX_REPLY) {
      response_.clear(); error("Response too large"); size = measureJson(response_);
    }
    const size_t written = serializeJson(response_, replies_[index], MAX_REPLY - 1);
    replies_[index][written] = '\n'; replies_[index][written + 1] = 0;
    replyLength_[index] = written + 1; ++replyCount_;
  }
  uint32_t toUnix(uint32_t local) const {
    return uint32_t(int64_t(local) + 946684800LL - int32_t(utcOffsetMinutes_) * 60L);
  }
  bool fromUnix(JsonVariantConst v, uint32_t &local, int16_t offset) const {
    uint32_t timestamp;
    if (!number(v, 0, 0xFFFFFFFFUL, timestamp)) return false;
    const int64_t t = int64_t(timestamp) - 946684800LL + int32_t(offset) * 60L;
    if (t < 757382400LL || t > 3155759999LL) return false; // local years 2024..2099
    local = uint32_t(t); return true;
  }
  bool deadline(JsonObjectConst data, Deadline &d) const {
    uint32_t subject, kind, lead, due;
    if (!number(data["subject"], 0, SUBJECT_COUNT - 1, subject) ||
        !number(data["kind"], 0, KIND_COUNT - 1, kind) ||
        !number(data["lead"], 0, LEAD_COUNT - 1, lead) ||
        !fromUnix(data["due"], due, utcOffsetMinutes_)) return false;
    d = Deadline{}; d.used = true; d.subject = subject; d.kind = kind; d.lead = lead; d.due = due;
    return true;
  }
  static void timerState(JsonObject o, const FeatureManager &f) {
    o["phase"] = f.timer.phase(); o["remaining"] = f.timer.remainingSeconds();
    o["cycle"] = f.timer.cycle(); o["cycles"] = f.timer.config().cycles;
    o["paused"] = f.timer.userPaused(); o["awayPaused"] = f.timer.awayHeld();
    o["counting"] = f.timer.counting(); o["style"] = f.timer.style();
    o["focusMin"] = f.timer.config().focusMin; o["shortMin"] = f.timer.config().shortMin;
    o["longMin"] = f.timer.config().longMin;
  }
  static void studyState(JsonObject o, const FeatureManager &f) {
    o["active"] = f.study.active(); o["subject"] = f.study.subject();
    o["sessionTime"] = f.study.sessionSeconds(); o["awayPaused"] = f.study.awayPaused();
  }
  void fullState(const FeatureManager &f, const Pet &pet) {
    response_["type"] = "STATE"; response_["protocol"] = 1;
    response_["timestamp"] = toUnix(f.nowEpoch()); response_["epoch"] = "unix";
    response_["utcOffsetMinutes"] = utcOffsetMinutes_;
    timerState(response_.createNestedObject("timer"), f);
    studyState(response_.createNestedObject("study"), f);
    JsonObject p = response_.createNestedObject("pet"); p["species"] = pet.species(); p["palette"] = pet.palette();
    JsonObject s = response_.createNestedObject("settings");
    s["buzzerEnabled"] = f.settings.buzzerEnabled; s["irEnabled"] = f.settings.irEnabled;
    s["irPausesTimer"] = f.settings.irPausesTimer; s["awayIdx"] = f.settings.awayIdx;
    s["repeatIdx"] = f.settings.repeatIdx; s["brightness"] = f.settings.brightness;
#if PRESENCE_USE_TOF
    s["tofPort"] = f.presence.distance.port();
    JsonObject range = response_.createNestedObject("range");
    const bool validRange = f.settings.irEnabled && f.presence.distance.valid();
    range["enabled"] = f.settings.irEnabled;
    range["valid"] = validRange; range["present"] = f.presence.present();
    if (validRange) range["millimeters"] = f.presence.distance.millimeters();
    else range["millimeters"] = nullptr;
    range["cutoffMm"] = TOF_PRESENCE_MAX_MM;
#endif
    response_["deadlineCount"] = f.deadlines.count(); response_["eepromState"] = f.deadlines.storageState();
  }
  void process(char *json, FeatureManager &f, Pet &pet, unsigned long now) {
    StaticJsonDocument<1024> input;
    response_.clear();
    if (!singleObject(json)) { error("Expected one JSON object per line"); enqueue(); return; }
    const DeserializationError parsed = deserializeJson(input, json, DeserializationOption::NestingLimit(5));
    if (parsed || !input.is<JsonObject>()) { error("Invalid JSON object"); enqueue(); return; }
    const JsonVariantConst id = input["id"];
    if (!id.isNull()) {
      if (id.is<const char *>() && id.as<JsonString>().size() == strlen(id.as<const char *>()) && strlen(id.as<const char *>()) <= 32) response_["id"] = id;
      else if (id.is<uint32_t>()) response_["id"] = id;
      else { error("Invalid request id"); enqueue(); return; }
    }
    if (!input["cmd"].is<const char *>() ||
        input["cmd"].as<JsonString>().size() != strlen(input["cmd"].as<const char *>()) ||
        (!input["data"].isNull() && !input["data"].is<JsonObject>())) {
      error("Missing cmd or invalid data"); enqueue(); return;
    }
    const char *cmd = input["cmd"];
    JsonObjectConst data = input["data"].as<JsonObjectConst>();
    uint32_t value;
    if (!strcmp(cmd, "GET_STATE")) fullState(f, pet);
    else if (!strcmp(cmd, "TIMER_START")) { f.timer.start(now); changes_ |= OTHER; ack("Timer started"); }
    else if (!strcmp(cmd, "TIMER_PAUSE")) { f.timer.pause(); changes_ |= OTHER; ack("Timer paused"); }
    else if (!strcmp(cmd, "TIMER_SKIP")) { f.timerSkip(now); changes_ |= OTHER; ack("Phase skipped"); }
    else if (!strcmp(cmd, "TIMER_RESET")) { f.timer.reset(); changes_ |= OTHER; ack("Timer reset"); }
    else if (!strcmp(cmd, "TIMER_CONFIG")) {
      uint32_t focus = 0, shortBreak = 0, longBreak = 0, cycles = 0;
      if (!number(data["style"], 0, STYLE_COUNT - 1, value) ||
          (value == STYLE_CUSTOM &&
          (!number(data["focusMin"], 5, 180, focus) || focus % 5 ||
           !number(data["shortMin"], 1, 60, shortBreak) ||
           !number(data["longMin"], 5, 90, longBreak) || longBreak % 5 ||
           !number(data["cycles"], 1, 8, cycles)))) error("Invalid timer configuration");
      else {
        f.timer.setStyle(TimerStyle(value));
        if (value == STYLE_CUSTOM) {
          f.timer.setField(TF_FOCUS, focus); f.timer.setField(TF_SHORT, shortBreak);
          f.timer.setField(TF_LONG, longBreak); f.timer.setField(TF_CYCLES, cycles);
        }
        changes_ |= OTHER; ack("Timer configured");
      }
    } else if (!strcmp(cmd, "STUDY_START")) {
      if (!number(data["subject"], 0, SUBJECT_COUNT - 1, value)) error("Invalid subject");
      else { f.study.start(value, now); changes_ |= OTHER; ack("Study started"); }
    } else if (!strcmp(cmd, "STUDY_STOP")) { f.study.stop(now); changes_ |= OTHER; ack("Study stopped"); }
    else if (!strcmp(cmd, "STUDY_GET_HOURS")) {
      response_["type"] = "HOURS"; JsonArray hours = response_.createNestedArray("hours");
      for (uint8_t i = 0; i < SUBJECT_COUNT; ++i) hours.add(f.study.totalSeconds(i));
    } else if (!strcmp(cmd, "STUDY_RESET_HOURS")) { f.study.resetTotals(); changes_ |= OTHER; ack("Hours reset"); }
    else if (!strcmp(cmd, "DEADLINES_GET")) {
      response_["type"] = "DEADLINES"; response_["epoch"] = "unix";
      response_["eepromState"] = f.deadlines.storageState();
      JsonArray entries = response_.createNestedArray("deadlines"); uint8_t order[DeadlineManager::MAX];
      const uint8_t count = f.deadlines.sorted(order);
      for (uint8_t i = 0; i < count; ++i) {
        const Deadline &d = f.deadlines.get(order[i]); JsonObject item = entries.createNestedObject();
        item["index"] = order[i]; item["subject"] = d.subject; item["kind"] = d.kind;
        item["due"] = toUnix(d.due); item["lead"] = d.lead;
        item["leadSeconds"] = uint32_t(LEAD_MINUTES[d.lead]) * 60UL;
      }
    } else if (!strcmp(cmd, "DEADLINE_ADD")) {
      Deadline d;
      if (!deadline(data, d)) error("Invalid deadline fields or Unix due date");
      else {
        const int8_t index = f.deadlines.add(d, f.nowEpoch());
        if (index == -1) error("Deadline list full");
        else if (index < 0) error("EEPROM save unverified");
        else { changes_ |= DEADLINES; ack("Deadline added"); response_["index"] = index; response_["persistent"] = true; }
      }
    } else if (!strcmp(cmd, "DEADLINE_UPDATE") || !strcmp(cmd, "DEADLINE_DELETE")) {
      if (!number(data["index"], 0, DeadlineManager::MAX - 1, value) || !f.deadlines.get(value).used) error("Deadline not found");
      else if (!strcmp(cmd, "DEADLINE_DELETE")) {
        if (!f.deadlines.remove(value, f.nowEpoch())) error("EEPROM delete unverified");
        else { changes_ |= DEADLINES; ack("Deadline deleted"); response_["index"] = value; response_["persistent"] = true; }
      } else {
        Deadline d;
        if (!data["deadline"].is<JsonObjectConst>() || !deadline(data["deadline"].as<JsonObjectConst>(), d)) error("Invalid deadline fields or Unix due date");
        else if (!f.deadlines.replace(value, d, f.nowEpoch())) error("EEPROM save unverified");
        else { changes_ |= DEADLINES; ack("Deadline updated"); response_["index"] = value; response_["persistent"] = true; }
      }
    } else if (!strcmp(cmd, "PET_SET")) {
      uint32_t palette;
      if (!number(data["species"], 0, PET_COUNT - 1, value) || !number(data["palette"], 0, PALETTES_PER_PET - 1, palette)) error("Invalid pet or palette");
      else { pet.setSpecies(value); pet.setPalette(palette); changes_ |= OTHER; ack("Pet updated"); }
    } else if (!strcmp(cmd, "SET_DATETIME")) {
      int16_t offset = utcOffsetMinutes_;
      bool valid = true;
      if (data.containsKey("utcOffsetMinutes")) {
        if (!data["utcOffsetMinutes"].is<int16_t>()) valid = false;
        else { offset = data["utcOffsetMinutes"].as<int16_t>(); valid = offset >= -720 && offset <= 840; }
      }
      if (!valid || !fromUnix(data["timestamp"], value, offset)) error("Invalid Unix time or UTC offset");
      else { utcOffsetMinutes_ = offset; f.clock.set(value, now); changes_ |= OTHER; ack("Time set"); }
    } else if (!strcmp(cmd, "SETTINGS_SET")) {
      Settings next = f.settings; bool valid = !data.isNull() && data.size() != 0;
      const char *bools[] = {"buzzerEnabled", "irEnabled", "irPausesTimer"};
      bool *targets[] = {&next.buzzerEnabled, &next.irEnabled, &next.irPausesTimer};
      for (uint8_t i = 0; i < 3; ++i) if (data.containsKey(bools[i])) {
        if (!data[bools[i]].is<bool>()) valid = false; else *targets[i] = data[bools[i]].as<bool>();
      }
      const char *numbers[] = {"awayIdx", "repeatIdx", "brightness"};
      const uint8_t lo[] = {0, 0, 1}, hi[] = {AWAY_COUNT - 1, REPEAT_COUNT - 1, 15};
      uint8_t *values[] = {&next.awayIdx, &next.repeatIdx, &next.brightness};
      for (uint8_t i = 0; i < 3; ++i) if (data.containsKey(numbers[i])) {
        if (!number(data[numbers[i]], lo[i], hi[i], value)) valid = false; else *values[i] = value;
      }
#if PRESENCE_USE_TOF
      uint32_t port = f.presence.distance.port();
      if (data.containsKey("tofPort") && !number(data["tofPort"], 0, 3, port)) valid = false;
#endif
      for (JsonPairConst pair : data) {
        const char *key = pair.key().c_str();
        if (strcmp(key, "buzzerEnabled") && strcmp(key, "irEnabled") && strcmp(key, "irPausesTimer") &&
            strcmp(key, "awayIdx") && strcmp(key, "repeatIdx") && strcmp(key, "brightness")
#if PRESENCE_USE_TOF
            && strcmp(key, "tofPort")
#endif
            ) valid = false;
      }
      if (!valid) error("Invalid settings");
      else {
        f.settings = next; f.applySettings(now);
#if PRESENCE_USE_TOF
        if (port != f.presence.distance.port()) f.presence.distance.setPort(port, now);
#endif
        changes_ |= OTHER; ack("Settings applied");
      }
    } else if (!strcmp(cmd, "BUZZER_TEST")) { f.buzzer.play(BEEP_BREAK_START, now); ack("Buzzer test"); }
    else error("Unknown command");
    enqueue();
  }
};
#endif
