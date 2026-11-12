// Compile with -I tests/ui_sim/ble_radio BEFORE other library include paths.
#include "../../BleLink.cpp"
#include <cassert>
#include <stdio.h>
#include <string.h>
int main() {
  BleLink link;
  assert(link.begin());
  assert(mockBleIrqAttached);
  assert(strcmp(link.statusText(), "ready") == 0);
  // Retry must also leave STBLE's IRQ attached for synchronous commands.
  assert(link.begin());
  assert(mockBleIrqAttached);
  for (unsigned stage = 2; stage <= 11; ++stage) {
    mockBleFailStage = stage;
    assert(!link.begin());
    char expected[8]; snprintf(expected, sizeof expected, "E%02u:42", stage);
    assert(strcmp(link.statusText(), expected) == 0);
    mockBleFailStage = 0;
    assert(link.begin());
    assert(strcmp(link.statusText(), "ready") == 0);
  }
  mockBlePresent = false;
  assert(!link.begin());
  assert(!mockBleIrqAttached);
  assert(strcmp(link.statusText(), "E01:FF") == 0);
  mockBlePresent = true;
  assert(link.begin());
  assert(mockBleIrqAttached);
  puts("BLE startup/IRQ/retry/error-stage checks passed");
}
