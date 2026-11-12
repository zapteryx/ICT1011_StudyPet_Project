#include "../../Features.h"
#include <cassert>
static unsigned checks = 0;
#define CHECK(v) do { assert(v); ++checks; } while (0)
static uint32_t epoch() { return TimeUtil::toEpoch({2027, 1, 1, 12, 0, 0}); }
static Deadline draft(uint8_t subject = 0) {
  Deadline d = {}; d.used = true; d.subject = subject; d.kind = 2; d.lead = 5;
  d.due = epoch() + 86400UL; return d;
}
int main() {
  Wire.begin();
  DeadlineManager m; m.begin(epoch());
  CHECK(m.count() == 0 && m.storageState() == DeadlineStorage::EMPTY);
  CHECK(Wire.eepromWrites == 0); // no format/erase writes at boot
  CHECK(m.add(draft(), epoch()) == 0);
  DeadlineManager reboot; reboot.begin(epoch());
  CHECK(reboot.count() == 1 && reboot.get(0).due == draft().due && reboot.get(0).kind == 2);
  Deadline edited = reboot.get(0); edited.subject = 1; edited.lead = 2; edited.due += 3600;
  CHECK(reboot.replace(0, edited, epoch()));
  m.begin(epoch()); CHECK(m.get(0).subject == 1 && m.get(0).lead == 2 && m.get(0).due == edited.due);
  const unsigned writes = Wire.eepromWrites;
  CHECK(m.replace(0, edited, epoch())); CHECK(Wire.eepromWrites == writes);
  m.update(edited.due, 15); m.acknowledge(0); CHECK(Wire.eepromWrites == writes);
  CHECK(m.remove(0, epoch())); reboot.begin(epoch()); CHECK(reboot.count() == 0);
  for (uint8_t i = 0; i < 8; ++i) CHECK(m.add(draft(i % SUBJECT_COUNT), epoch()) == i);
  CHECK(m.add(draft(), epoch()) == -1);
  reboot.begin(epoch()); CHECK(reboot.count() == 8);

  // Save failure leaves the prior in-RAM entry intact and cannot report success.
  Wire.eepromReadOnly = true; edited = m.get(0); edited.kind = 1;
  CHECK(!m.replace(0, edited, epoch())); CHECK(m.get(0).kind == 2);
  CHECK(m.storageState() == DeadlineStorage::IO_ERROR);
  Wire.eepromReadOnly = false; CHECK(m.replace(0, edited, epoch()));
  Wire.eepromConnected = false;
  CHECK(!m.remove(0, epoch())); CHECK(m.get(0).used);
  Wire.eepromConnected = true;

  // A missing shield at boot must not cause an empty RAM list to overwrite
  // already stored entries when it reconnects.
  Wire.eepromConnected = false; DeadlineManager missing; missing.begin(epoch());
  CHECK(missing.count() == 0 && missing.storageState() == DeadlineStorage::IO_ERROR);
  Wire.eepromConnected = true; CHECK(missing.add(draft(), epoch()) == -1);
  CHECK(missing.count() == 8);
  Wire.eepromShortRead = true; CHECK(!missing.reload(epoch()));
  CHECK(missing.count() == 8);
  Wire.eepromShortRead = false;

  // Simulate power cuts after every byte of a save, including commit marker.
  // Recovery must be a complete old/new record, never a mixture of fields.
  Wire.eeprom.fill(0xFF); Wire.busy = 0;
  m.begin(epoch()); CHECK(m.add(draft(), epoch()) == 0);
  const auto baseline = Wire.eeprom;
  for (int cut = 0; cut <= 81; ++cut) {
    Wire.eeprom = baseline; Wire.busy = 0; Wire.bytesWritten = 0; Wire.failAfterBytes = cut;
    DeadlineManager before; before.begin(epoch());
    Deadline changed = draft(1); changed.kind = 4; changed.lead = 3; changed.due += 7200;
    const bool saved = before.replace(0, changed, epoch());
    Wire.failAfterBytes = -1; Wire.busy = 0;
    DeadlineManager after; after.begin(epoch());
    CHECK(after.count() == 1);
    const Deadline &d = after.get(0);
    CHECK((d.subject == 0 && d.kind == 2 && d.lead == 5 && d.due == draft().due) ||
          (d.subject == 1 && d.kind == 4 && d.lead == 3 && d.due == changed.due));
    if (saved) CHECK(d.subject == 1);
  }
  CHECK(Wire.pageCrossings == 0);

  // CRC failure falls back to the other snapshot; two invalid snapshots warn.
  Wire.eeprom = baseline; Wire.busy = 0; m.begin(epoch());
  CHECK(m.replace(0, draft(1), epoch()));
  Wire.eeprom[128 + 20] ^= 1;
  reboot.begin(epoch()); CHECK(reboot.get(0).subject == 0);
  CHECK(reboot.storageState() == DeadlineStorage::RECOVERED);
  Wire.eeprom[20] ^= 1;
  reboot.begin(epoch()); CHECK(reboot.count() == 0 && reboot.storageState() == DeadlineStorage::CORRUPT);
  CHECK(reboot.add(draft(2), epoch()) == 0); // explicit save can replace bad data
  m.begin(epoch()); CHECK(m.count() == 1 && m.get(0).subject == 2);
  // First-use memory is not guaranteed to be erased FF (e.g. example data).
  for (uint8_t pattern = 0; pattern < 3; ++pattern) {
    Wire.eeprom.fill(pattern == 0 ? 0 : pattern == 1 ? 0xA5 : 0x37);
    if (pattern == 2) for (unsigned i = 0; i < 256; ++i) Wire.eeprom[i] = i;
    Wire.busy = 0;
    const auto untouched = Wire.eeprom;
    const unsigned writesBefore = Wire.eepromWrites;
    m.begin(epoch());
    CHECK(m.count() == 0 && m.storageState() == DeadlineStorage::UNINITIALIZED);
    CHECK(Wire.eepromWrites == writesBefore && Wire.eeprom == untouched);
    CHECK(m.add(draft(1), epoch()) == 0);
    reboot.begin(epoch());
    CHECK(reboot.count() == 1 && reboot.get(0).subject == 1);
    CHECK(reboot.storageState() == DeadlineStorage::SAVED);
  }
  // Do not relabel a known StudyPet record with a damaged magic as first use.
  Wire.eeprom.fill(0xFF); Wire.busy = 0;
  m.begin(epoch()); CHECK(m.add(draft(), epoch()) == 0);
  Wire.eeprom[1] ^= 1;
  reboot.begin(epoch()); CHECK(reboot.storageState() == DeadlineStorage::CORRUPT);
  puts("EEPROM persistence checks complete.");
  printf("PASS: %u create/edit/delete, reboot, corruption and power-cut checks.\n", checks);
}
