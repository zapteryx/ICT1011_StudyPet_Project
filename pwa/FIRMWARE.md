# StudyPet BLE Firmware Integration Guide

This document describes how to add BLE support to the StudyPet TinyCircuit firmware.

## Hardware Requirements

- TinyCircuits TinyScreen+ (ATSAMD21)
- BLE TinyShield or compatible BLE module
- Nordic nRF51822 or similar BLE chip

## Library Dependencies

Add to your Arduino project:

```cpp
#include <BLEPeripheral.h>  // or similar BLE library
```

For TinyCircuits, you may need:
- Adafruit_BluefruitLE library
- Nordic UART Service implementation

## BLE Service Implementation

### 1. Service and Characteristic UUIDs

```cpp
// Nordic UART Service
#define BLE_SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_TX_CHAR_UUID        "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // Phone -> Device
#define BLE_RX_CHAR_UUID        "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // Device -> Phone
```

### 2. Basic BLE Setup

```cpp
#include <BLEPeripheral.h>

BLEPeripheral blePeripheral;
BLEService uartService(BLE_SERVICE_UUID);
BLECharacteristic txCharacteristic(BLE_TX_CHAR_UUID, BLEWrite, 20);
BLECharacteristic rxCharacteristic(BLE_RX_CHAR_UUID, BLENotify, 20);

String rxBuffer = "";

void setupBLE() {
  // Set device name
  blePeripheral.setLocalName("StudyPet");
  blePeripheral.setAdvertisedServiceUuid(uartService.uuid());
  
  // Add service and characteristics
  blePeripheral.addAttribute(uartService);
  blePeripheral.addAttribute(txCharacteristic);
  blePeripheral.addAttribute(rxCharacteristic);
  
  // Set event handlers
  txCharacteristic.setEventHandler(BLEWritten, onBLEWrite);
  
  // Start advertising
  blePeripheral.begin();
}

void onBLEWrite(BLECentral& central, BLECharacteristic& characteristic) {
  const uint8_t* data = characteristic.value();
  int length = characteristic.valueLength();
  
  // Accumulate data
  for (int i = 0; i < length; i++) {
    char c = (char)data[i];
    if (c == '\n' || c == '\0') {
      processCommand(rxBuffer);
      rxBuffer = "";
    } else {
      rxBuffer += c;
    }
  }
}
```

### 3. Command Processing

```cpp
#include <ArduinoJson.h>  // For JSON parsing

void processCommand(String jsonStr) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    sendError("Invalid JSON");
    return;
  }
  
  const char* cmd = doc["cmd"];
  JsonObject data = doc["data"];
  
  // Timer commands
  if (strcmp(cmd, "TIMER_START") == 0) {
    features.timer.start(millis());
    sendAck("Timer started");
  }
  else if (strcmp(cmd, "TIMER_PAUSE") == 0) {
    features.timer.pause();
    sendAck("Timer paused");
  }
  else if (strcmp(cmd, "TIMER_SKIP") == 0) {
    features.timerSkip(millis());
    sendAck("Phase skipped");
  }
  else if (strcmp(cmd, "TIMER_RESET") == 0) {
    features.timer.reset();
    sendAck("Timer reset");
  }
  else if (strcmp(cmd, "TIMER_CONFIG") == 0) {
    uint8_t style = data["style"];
    features.timer.setStyle((TimerStyle)style);
    if (style == STYLE_CUSTOM) {
      features.timer.setField(TF_FOCUS, data["focusMin"]);
      features.timer.setField(TF_SHORT, data["shortMin"]);
      features.timer.setField(TF_LONG, data["longMin"]);
      features.timer.setField(TF_CYCLES, data["cycles"]);
    }
    sendAck("Timer configured");
  }
  
  // Study commands
  else if (strcmp(cmd, "STUDY_START") == 0) {
    uint8_t subject = data["subject"];
    features.study.start(subject, millis());
    sendAck("Study started");
  }
  else if (strcmp(cmd, "STUDY_STOP") == 0) {
    features.study.stop(millis());
    sendAck("Study stopped");
  }
  else if (strcmp(cmd, "STUDY_GET_HOURS") == 0) {
    sendStudyHours();
  }
  else if (strcmp(cmd, "STUDY_RESET_HOURS") == 0) {
    features.study.resetTotals();
    sendAck("Hours reset");
  }
  
  // Deadline commands
  else if (strcmp(cmd, "DEADLINES_GET") == 0) {
    sendDeadlines();
  }
  else if (strcmp(cmd, "DEADLINE_ADD") == 0) {
    Deadline dl;
    dl.subject = data["subject"];
    dl.kind = data["kind"];
    dl.due = data["due"];
    dl.lead = data["lead"];
    dl.used = true;
    dl.stage = 0;
    dl.acked = true;
    dl.popup = false;
    dl.lastAlert = features.nowEpoch();
    features.deadlines.add(dl, features.nowEpoch());
    sendAck("Deadline added");
  }
  else if (strcmp(cmd, "DEADLINE_UPDATE") == 0) {
    uint8_t index = data["index"];
    JsonObject deadline = data["deadline"];
    Deadline dl;
    dl.subject = deadline["subject"];
    dl.kind = deadline["kind"];
    dl.due = deadline["due"];
    dl.lead = deadline["lead"];
    features.deadlines.replace(index, dl, features.nowEpoch());
    sendAck("Deadline updated");
  }
  else if (strcmp(cmd, "DEADLINE_DELETE") == 0) {
    uint8_t index = data["index"];
    features.deadlines.remove(index);
    sendAck("Deadline deleted");
  }
  
  // Pet commands
  else if (strcmp(cmd, "PET_SET") == 0) {
    pet.setSpecies(data["species"]);
    pet.setPalette(data["palette"]);
    sendAck("Pet updated");
  }
  
  // Settings commands
  else if (strcmp(cmd, "GET_STATE") == 0) {
    sendFullState();
  }
  else if (strcmp(cmd, "SET_DATETIME") == 0) {
    uint32_t timestamp = data["timestamp"];
    features.clock.set(timestamp, millis());
    sendAck("Time set");
  }
  else if (strcmp(cmd, "SETTINGS_SET") == 0) {
    features.settings.buzzerEnabled = data["buzzerEnabled"];
    features.settings.irEnabled = data["irEnabled"];
    features.settings.irPausesTimer = data["irPausesTimer"];
    features.settings.awayIdx = data["awayIdx"];
    features.settings.repeatIdx = data["repeatIdx"];
    features.settings.brightness = data["brightness"];
    features.applySettings(millis());
    sendAck("Settings saved");
  }
  else if (strcmp(cmd, "BUZZER_TEST") == 0) {
    features.buzzer.play(BEEP_BREAK_START, millis());
    sendAck("Buzzer test");
  }
  
  else {
    sendError("Unknown command");
  }
}
```

### 4. Response Functions

```cpp
void sendBLE(const char* json) {
  int len = strlen(json);
  for (int i = 0; i < len; i += 20) {
    int chunkLen = min(20, len - i);
    rxCharacteristic.setValue((uint8_t*)(json + i), chunkLen);
    delay(10);  // Small delay between chunks
  }
}

void sendAck(const char* message) {
  StaticJsonDocument<128> doc;
  doc["type"] = "ACK";
  doc["message"] = message;
  char json[128];
  serializeJson(doc, json);
  sendBLE(json);
}

void sendError(const char* message) {
  StaticJsonDocument<128> doc;
  doc["type"] = "ERROR";
  doc["message"] = message;
  char json[128];
  serializeJson(doc, json);
  sendBLE(json);
}

void sendFullState() {
  StaticJsonDocument<512> doc;
  doc["type"] = "STATE";
  
  // Timer state
  JsonObject timer = doc.createNestedObject("timer");
  timer["phase"] = features.timer.phase();
  timer["remaining"] = features.timer.remainingSeconds();
  timer["cycle"] = features.timer.cycle();
  timer["cycles"] = features.timer.config().cycles;
  timer["paused"] = features.timer.userPaused();
  
  // Study state
  JsonObject study = doc.createNestedObject("study");
  study["active"] = features.study.active();
  study["subject"] = features.study.subject();
  study["sessionTime"] = features.study.sessionSeconds();
  
  // Pet state
  JsonObject petObj = doc.createNestedObject("pet");
  petObj["species"] = pet.species();
  petObj["palette"] = pet.palette();
  
  // Settings
  JsonObject settings = doc.createNestedObject("settings");
  settings["buzzerEnabled"] = features.settings.buzzerEnabled;
  settings["irEnabled"] = features.settings.irEnabled;
  settings["irPausesTimer"] = features.settings.irPausesTimer;
  settings["awayIdx"] = features.settings.awayIdx;
  settings["repeatIdx"] = features.settings.repeatIdx;
  settings["brightness"] = features.settings.brightness;
  
  char json[512];
  serializeJson(doc, json);
  sendBLE(json);
}

void sendDeadlines() {
  StaticJsonDocument<1024> doc;
  doc["type"] = "DEADLINES";
  JsonArray deadlines = doc.createNestedArray("deadlines");
  
  uint8_t order[DeadlineManager::MAX];
  uint8_t count = features.deadlines.sorted(order);
  
  for (uint8_t i = 0; i < count; i++) {
    const Deadline& dl = features.deadlines.get(order[i]);
    if (!dl.used) continue;
    
    JsonObject item = deadlines.createNestedObject();
    item["subject"] = dl.subject;
    item["kind"] = dl.kind;
    item["due"] = dl.due;
    item["lead"] = dl.lead;
    item["leadSeconds"] = (uint32_t)LEAD_MINUTES[dl.lead] * 60UL;
  }
  
  char json[1024];
  serializeJson(doc, json);
  sendBLE(json);
}

void sendStudyHours() {
  StaticJsonDocument<256> doc;
  doc["type"] = "HOURS";
  JsonArray hours = doc.createNestedArray("hours");
  
  for (uint8_t i = 0; i < SUBJECT_COUNT; i++) {
    hours.add(features.study.totalSeconds(i));
  }
  
  char json[256];
  serializeJson(doc, json);
  sendBLE(json);
}
```

### 5. Integration in Main Loop

```cpp
void loop() {
  const unsigned long now = millis();
  
  // Poll BLE
  blePeripheral.poll();
  
  // Existing StudyPet loop
  const InputEvent ev = buttons.update(display, now);
  if (ev.key != KEY_NONE) ui.handleInput(ev, pet, features, now);
  
  features.update(now);
  
  pet.setAway(features.settings.irEnabled && !features.presence.present());
  pet.setOnBreak(features.timer.onBreak());
  pet.update(now);
  
  ui.update(display, pet, features, now);
  
  // Periodically send state updates (every 5 seconds)
  static unsigned long lastUpdate = 0;
  if (blePeripheral.connected() && now - lastUpdate > 5000) {
    sendTimerUpdate();
    sendStudyUpdate();
    lastUpdate = now;
  }
}

void sendTimerUpdate() {
  if (features.timer.phase() == PHASE_IDLE) return;
  
  StaticJsonDocument<256> doc;
  doc["type"] = "TIMER";
  doc["phase"] = features.timer.phase();
  doc["remaining"] = features.timer.remainingSeconds();
  doc["cycle"] = features.timer.cycle();
  doc["paused"] = features.timer.userPaused();
  
  char json[256];
  serializeJson(doc, json);
  sendBLE(json);
}

void sendStudyUpdate() {
  if (!features.study.active()) return;
  
  StaticJsonDocument<256> doc;
  doc["type"] = "STUDY";
  doc["active"] = true;
  doc["subject"] = features.study.subject();
  doc["sessionTime"] = features.study.sessionSeconds();
  
  char json[256];
  serializeJson(doc, json);
  sendBLE(json);
}
```

## Memory Considerations

The ATSAMD21 has limited RAM. Consider:

1. **Use StaticJsonDocument** with appropriate sizes
2. **Stream large responses** in chunks
3. **Limit concurrent BLE operations**
4. **Clear buffers** after use

## Testing

1. Upload firmware to TinyCircuit
2. Power on device
3. Open PWA in Chrome
4. Click "Connect Device"
5. Verify commands work both ways

## Troubleshooting

- **Device not found**: Check BLE advertising is enabled
- **Connection drops**: Reduce data transmission rate
- **Commands fail**: Verify JSON format and buffer sizes
- **Memory issues**: Reduce StaticJsonDocument sizes

## Alternative: Simplified Protocol

For tighter memory constraints, use a binary protocol instead of JSON:

```cpp
// Command format: [CMD_BYTE][PARAM1][PARAM2]...
#define CMD_TIMER_START 0x01
#define CMD_TIMER_PAUSE 0x02
// ... etc

void processBinaryCommand(uint8_t* data, int length) {
  switch (data[0]) {
    case CMD_TIMER_START:
      features.timer.start(millis());
      break;
    // ... handle other commands
  }
}
```

This uses less memory but requires updating the PWA's BLE layer.
