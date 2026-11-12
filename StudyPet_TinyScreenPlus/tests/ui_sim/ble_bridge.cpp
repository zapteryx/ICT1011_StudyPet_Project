// Native stdin/stdout adapter for testing the real firmware protocol with pwa/ble.js.
// The Wire mock models EEPROM; no physical radio is involved.
#include <iostream>
#include <string>
#include "../../BleProtocol.h"
static FeatureManager features;
static Pet pet;
static BleProtocol protocol;
static void begin() {
  features.begin(simulatedMillis); pet.begin(simulatedMillis);
  features.settings.irEnabled = false; features.applySettings(simulatedMillis);
}
int main() {
  begin();
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "@REBOOT") {
      protocol.reset(); features = FeatureManager(); pet = Pet(); begin();
      std::cout << "{\"type\":\"HOST\",\"event\":\"rebooted\"}\n" << std::flush;
      continue;
    }
    if (line == "@LOCAL") {
      pet.setSpecies(4); pet.setPalette(2); features.settings.brightness = 3;
      features.timer.start(simulatedMillis);
      std::cout << "{\"type\":\"HOST\",\"event\":\"local\"}\n" << std::flush;
      continue;
    }
    line += '\n';
    for (size_t offset = 0; offset < line.size(); offset += 20) {
      const uint8_t size = line.size() - offset < 20 ? line.size() - offset : 20;
      protocol.receive(reinterpret_cast<const uint8_t *>(line.data() + offset), size, simulatedMillis);
    }
    protocol.tick(features, pet, simulatedMillis);
    while (protocol.pending()) {
      uint8_t bytes[20]; const uint8_t size = protocol.chunk(bytes);
      std::cout.write(reinterpret_cast<char *>(bytes), size); protocol.sent(size);
    }
    std::cout << std::flush;
    simulatedMillis += 100;
    features.update(simulatedMillis);
  }
}
