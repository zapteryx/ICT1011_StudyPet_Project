# StudyPet — TinyScreen+ study companion

The phone companion is in `pwa/`. See [PWA synchronization and deployment](pwa/SYNC_FIX.md)
for the ASD2116-compatible app and cache update steps. These PWA fixes work with
the existing BLE IRQ-fixed firmware; they do not require another board upload.

**Target:** TinyCircuits TinyScreen+ (ATSAMD21), 96 x 64 OLED, 8-bit colour.

## Files
| File | Role |
|---|---|
| `StudyPet_TinyScreenPlus.ino` | setup/loop; wires the state machine (input → features → pet → draw) |
| `StudyPetConfig.h` | shared hardware options for both the sketch and BLE source file |
| `Input.h` | button → Up/Down/Back/Select mapping (short / long / repeat), presence sensor |
| `ThermalCamera.h` | AST1045/AMG8833 I2C frames, temperatures, and warm-target heuristic |
| `DistanceSensor.h` | AST1024/VL53L0X initialization, checked distance frames, status and recovery |
| `DeadlineStorage.h` | ASD2202-R-E EEPROM journal, versioned records, CRC and verified commits |
| `Features.h` | buzzer patterns, software clock, break timer, study tracker, deadline CRUD + reminders, settings |
| `Pet.h` | 6 species × 3 colours, idle + interactive animation state (no drawing) |
| `UI.h` | home screen, three-row menus with button labels, button guide, editors, dashboard, alerts, pet sprite renderer |
| `BleLink.h` / `BleLink.cpp` | ASD2116 STBLE radio, advertising, subscription, reconnect and notification handling |
| `BleProtocol.h` | bounded JSON framing, validated commands, state replies and telemetry |

## Wiring (defaults — change in `StudyPetConfig.h`)
| Part | Pin | Notes |
|---|---|---|
| Buzzer / piezo | `A0` | `BUZZER_PASSIVE 1` = passive piezo via `tone()`; `0` = active buzzer |
| AST1024 TOF sensor | Adapter port 1 | I2C through `Wireling.selectPort(1)`; replaces the thermal camera |
| ASD2202-R-E EEPROM | 32-pin TinyShield stack | Main I2C bus, default 7-bit address 0x50; not a Wireling port |
| ASD2116 BLE TinyShield | 32-pin TinyShield stack | ST BlueNRG-MS; IRQ 2, RESET 9, CS 10, external SPI 11/12/13 |
| Optional digital IR/PIR | `A1` | Only with `PRESENCE_USE_TOF 0` and `IR_USE_THERMAL_CAMERA 0` |

TinyScreen+ GPIO is **3.3 V**. Power sensors from 3.3 V (AM312 is fine) or level-shift a 5 V output.
A small piezo can be driven directly; a magnetic buzzer needs a transistor.
Wireling Adapter support is now enabled by default (`USE_WIRELING_ADAPTER 1`).
Install **VL53L0X by Pololu**, **Wireling**, and **TinyScreen** in Library Manager. Startup calls
`Wireling.begin()` before initializing the buzzer. For directly wired parts without
an adapter, set `USE_WIRELING_ADAPTER` to `0` in `StudyPetConfig.h`.

## BLE companion support
ASD2116 support uses **TinyCircuits STBLE 0.1.0** and **ArduinoJson 6.21.5**.
Both were installed during implementation. Install STBLE from the official
TinyCircuits ZIP using Arduino IDE > Include Library > Add .ZIP Library, and
ArduinoJson from Library Manager when building on another computer.

Upload the sketch with all files, including `BleLink.cpp`. Scan for **StudyPet**,
connect to the Nordic UART-compatible service, enable notifications on UUID
`6e400003-b5a3-f393-e0a9-e50e24dcca9e`, and send commands to
`6e400002-b5a3-f393-e0a9-e50e24dcca9e`. Settings > BLE shows radio/link status;
select it to Retry. Standalone buttons, TOF auto-pauses and EEPROM deadlines
continue using the same managers while the phone is connected or disconnected.

Commands are JSON plus an actual LF/NUL terminator, split into at most 20-byte
writes. Replies use LF-terminated JSON notifications. Deadline mutations only
ACK after verified persistence. The phone can synchronize the clock using Unix
UTC seconds; the default local offset is +480 minutes (Singapore).

See **[docs/BLE.md](docs/BLE.md)** for installation, UUIDs, framing, command fields,
timestamp conversion, limits, error behavior and hardware smoke tests. It is the
implemented interface; the supplied external FIRMWARE.md was a starting guide.

## AST1024 distance-based presence (default)
Disconnect AST1045 and plug AST1024 into **port 1**. Keep AST1035 on **port 0 / A0**.
AST1024 is an I2C VL53L0X device: its Wireling port is not a digital A1 reading.
Wire uses 7-bit address **0x29** (the datasheet's 0x52 is the shifted address).
The installed Pololu VL53L0X library version used for this build is 1.3.1.

- A valid reading **20–1000 mm**, inclusive, means a nearby target. Return detection
  is confirmed for about 150 ms to reduce flicker.
- A reading **above 1000 mm** means no person detected. It pauses the focus timer,
  short/long break timer, and study-session accumulation on the next measurement.
  Remaining time and totals are preserved. Returning within 1 m resumes automatic
  holds; an explicit manual pause remains paused.
- The old 10 s–5 min away delay does not apply in TOF mode. Settings shows the
  fixed **Cutoff: 100 cm** instead. Change `TOF_PRESENCE_MAX_MM` in `StudyPetConfig.h` only
  if you intentionally want a different threshold.
- Startup, invalid/out-of-range samples, disconnection, and missing frames count
  as not detected. Invalid readings display `-- cm`, never a fabricated distance.
  Stale readings expire within 400 ms; failed devices retry initialization every 2 s.
- Distance appears on the home footer (alternating with session time while active)
  and in **Settings > Range**. Settings also shows **TOF sensor**, **TOF** status,
  **Person**, and **TOF port**. Port changes reset detection until new data arrives.
- If using port 3, set **TOF port: 3**, or change `TOF_SENSOR_PORT` to 3 for a
  permanent default. Runtime settings reset at power-off.
- **TOF sensor: Off** bypasses presence checks. **Away->timer: Off** permits the
  countdown to run while away; the study tracker still pauses. Both default to On.
  The wall clock and deadline reminders keep running regardless of presence.

Mount the sensor facing the seated user's torso, clear of the tabletop. It detects
the distance to an object, not human identity: nearby furniture, hands, or other
objects can also register. Ambient light, target angle and clothing affect readings;
verify the threshold on your actual desk. Accepted numeric range is 2 cm–2 m.

Legacy thermal mode is retained with `PRESENCE_USE_TOF 0` and `IR_USE_THERMAL_CAMERA 1`.
It shows surface temperature and approximate warm-target presence, not distance.
Legacy digital IR/PIR mode uses both flags at 0 and retains the old away delay.

## Persistent deadlines: ASD2202-R-E
Stack the EEPROM TinyShield on the TinyScreen+ / Wireling Adapter using the
32-pin TinyShield connectors. No new Arduino library is needed: the driver uses
Wire directly. Leave factory address jumpers A1=A0=0 for address **0x50**.
For a modified address, change `DEADLINE_EEPROM_ADDRESS` (0x50..0x53) in
`StudyPetConfig.h`. Shared configuration must be used for all firmware files.

- New deadlines are written when you select **Save deadline**. Existing subject,
  type, due date/time, and reminder-window changes save when confirmed. Confirmed
  deletion is also saved. The limit remains eight deadlines.
- Boot reads the saved deadlines automatically. Transient popup/acknowledgment
  state is reset and reminders are evaluated against the current clock again.
- **Settings > EEPROM** shows `saved`, `empty`, `backup`, `corrupt`, or `error`.
  Select it to Reload saved records, for example after reconnecting a shield.
  Unrecognized first-use/example data shows `not set`; a first confirmed Save
  establishes StudyPet records without automatically erasing memory at boot.
- Failed reads/writes open an **EEPROM ERROR** screen. The operation is not
  reported as verified and the in-RAM edit is rolled back. If an acknowledgment
  is lost after the final commit, Reload may reveal that the write completed.
  A missing shield does not silently turn a Save into RAM-only storage.
- Storage reserves **bytes 0..255** by default, as two separate 128-byte pages.
  Do not use this area for other data. `DEADLINE_EEPROM_OFFSET` can move it to
  another 128-byte-aligned pair of pages within the lower 64 KiB bank.
- Records have a format version, explicit little-endian fields, sequence number,
  and CRC-32. The inactive record is invalidated, written in page-safe chunks,
  read back, and committed last. Interrupted saves retain the other valid copy.
  If both copies are invalid, the UI warns instead of treating them as saved data.
  An explicit new Save can replace corrupt records; boot never formats memory.
- Writes occur only for user changes, not on every loop, clock tick, reminder, or
  acknowledgment. Confirming unchanged values does not rewrite EEPROM.

**First upgrade:** previous firmware's live RAM-only deadlines cannot migrate
through an upload/reset. Note them down before uploading and re-enter/save them.
After upgrading, test by saving a deadline, powering off/on, checking its fields,
then editing/deleting it and repeating the power cycle.

**Clock:** EEPROM preserves the deadline's due date, not elapsed time while off.
The software clock still starts at compilation time. Set date & time after each
restart for correct reminders. Study totals, settings, pet choice and timer state
remain volatile. Keep subject/type/reminder lists in the same order when updating
firmware: saved deadlines reference their indices.

### Wireling hardware diagnostic
Open `examples/WirelingHardwareCheck/WirelingHardwareCheck.ino` as a separate sketch
and upload it to TinyScreen+. This temporarily replaces StudyPet on the board.

- Start with the buzzer on **port 0 / A0**. BL plays a 500 ms test tone at 2700 Hz.
  TR cycles the software buzzer pin through A0, A1, A2, and A3; it must match the
  physical socket. BR stops the tone. The buzzer uses its analog/GPIO pin, not the
  I2C-selected camera port.
- TL scans all four I2C ports. The display shows responding address `29` for TOF,
  `68`/`69` for legacy cameras, or `--`. An address ACK alone does not identify the
  device or verify measurements. Serial Monitor at 115200 prints all
  responding device addresses per port, excluding the adapter multiplexer.
- `MUX NOT FOUND` means the default adapter at address 0x70 did not respond.
- After testing, upload the main StudyPet sketch again. On the main sketch,
  keep the buzzer on port 0 and use Settings > Buzzer: On > Test buzzer.

## Buttons
Top Right = **Up** · Bottom Right = **Down** · Top Left = **Back** · Bottom Left = **Select**

**Home (pet) screen**
- Tap Up → jump · Down → nap · Back → wiggle · Select → love (each ≤ 5 s)
- Hold Select → menu · Hold Back → start/pause timer · Hold Up → start/stop study session · Hold Down → hours dashboard

**Menus:** Up/Down move (hold to auto-repeat), Select opens, Back returns, hold Back = straight home.

## On-screen controls
- **Home:** `HOLD TIMER`, `HOLD STUDY`, `HOLD MENU`, and `HOLD HOURS` sit beside the four
  button positions. The footer cycles through tap actions and a pointer to the button guide when idle.
- **Menus and editors:** the bottom two lines mirror the physical buttons: `TL` / `TR` on the
  first line and `BL` / `BR` on the second. These mean top left/right and bottom left/right.
  Labels change with the selected action, such as Open, Start, Pause, Toggle, Edit, Save, or Dismiss.
  Three menu rows remain visible; scroll to reach the rest.
- **Menu > Button guide:** five pages cover the physical layout, home taps, home holds,
  menu navigation, and editors. TR/BR change pages; BL or TL closes the guide. Timers keep running.
- **Date editor:** shows the current field and progress (1/5 to 5/5). BL advances/saves;
  TL returns to the previous field, or cancels from the first field.
- **Timer status:** the home header separates the phase label, full countdown, and clock.
  The timer menu shows `AWAY` for presence pauses and `PAUSED` for a manual pause.

If no sensor is attached, switch **Settings > TOF sensor** off after each restart
(**Camera** or **IR sensor** in legacy modes) to use the timers manually.
The existing timer schedules, buzzer patterns, and original shortcuts are retained.

## Features
- **Break timer:** Pomodoro (25/5 × 4, 15 long, then reset), 90-min (90/20 × 2, 30 long), or Custom.
  3 short beeps when a break starts, 1 chirp when focus resumes. TOF absence pauses
  every countdown phase; legacy sensors pause focus only (toggle in Settings).
- **Deadlines:** create / view / edit / delete up to 8. Per-deadline reminder window (at due … 1 week before).
  Footer banner appears inside the window; full-screen pop-up + **long** beeps when it starts and when overdue,
  re-alerting every 15/30/60 min until acknowledged (Settings).
- **Study tracker:** per-subject totals, live session time, dashboard with bars. Edit `SUBJECT_NAMES` in `Features.h`.
- **Presence:** TOF cutoff is 1 m, with no legacy away delay. Sensor failures pause
  study time and countdowns until a valid nearby reading or a user override.

## Limitations
- Deadlines persist in ASD2202-R-E EEPROM. Study totals, settings and timer state
  reset on power-off; the clock restarts at compilation time. Set it in Settings.
- The updated UI was compiled for `TinyCircuits:samd:tinyscreen` using TinyCircuits SAMD 1.1.0.
  Desktop checks in `tests/ui_sim` use mocked hardware plus the installed TinyScreen library's
  real font data to exercise navigation, timers, editing, alerts, pixel bounds, and redraw caching.
  It has not been uploaded to or visually checked on a physical board in this session.
