# ASD2116 StudyPet BLE interface

This implementation targets the **ASD2116 ST BlueNRG-MS / SPBTLE-RF** TinyShield.
It uses TinyCircuits **STBLE 0.1.0**, not the Nordic BLEPeripheral library shown
as a placeholder in the supplied FIRMWARE.md. It exposes the same UART UUIDs
and command names, with validated inputs and EEPROM-aware responses.

Hardware reference and official STBLE library download:
https://learn.tinycircuits.com/Communication/Bluetooth-Low-Energy_TinyShield_Tutorial/

## Install and connect

1. Stack ASD2116 using the 32-pin TinyShield connectors. Its factory SPI/IRQ pins
   are CS=10, RESET=9, IRQ=2, MOSI=11, MISO=12, SCK=13. These are separate from
   buzzer A0 and the I2C TOF/EEPROM devices. This build targets factory IRQ wiring.
2. Install the official STBLE.zip with Arduino IDE > Include Library > Add .ZIP
   Library. Also install **ArduinoJson 6.21.5**, **TinyScreen**, **Wireling**, and
   **VL53L0X by Pololu**. STBLE and ArduinoJson were installed on this computer
   during implementation; no changes were made to their source code.
3. Keep all sketch headers plus **BleLink.cpp** together, then upload
   StudyPet_TinyScreenPlus.ino. It starts without waiting for a USB serial monitor.
4. In a BLE GATT client, scan for **StudyPet** (the name is in scan response).
   The UART service UUID is in its advertising data. Connect and discover it.
5. Enable notifications on **...0003** before sending commands to **...0002**.
   Settings > BLE shows `ready`, `subscribe` (connected but notifications are
   disabled), `connected`, or an error such as `E02:FF`. Select that row to Retry/reset the radio;
   retry disconnects an existing client. A radio error leaves the local UI usable.

Startup probes the SPI module before enabling the driver, allowing up to 500 ms
for the module to respond. Keep the factory IRQ=2 wiring: the STBLE interrupt
handler receives controller replies, including synchronous startup commands.
The foreground loop processes received events and command mutations. The
application does not wait for USB Serial.

Errors use `E<stage>:<hex status>`. `E01:FF` means the SPI probe received no ready
header (check shield seating, power and factory CS/reset wiring). Other stages:
02 GATT init; 03 GAP init; 04 random address; 05 name; 06 UART service;
07 write characteristic; 08 notify characteristic; 09 transmit power;
10 scan response; 11 advertising; 12 disconnect. `FF` from a controller command
can indicate a transport timeout; `42` indicates invalid parameters. Report the
complete code if retry still fails.

The radio uses a static-random address. It uses the controller's own address when
available, with a SAMD21 silicon-ID fallback. It does not assign every board the
same hard-coded address from the example sketch.

Hardware options now belong in **StudyPetConfig.h**, shared by the sketch and
the BLE .cpp file. Do not put per-file hardware #defines in the .ino. To build
without enabling the radio, set STUDYPET_ENABLE_BLE to 0 in the shared config.

## GATT and framing

| Role | UUID | Properties |
|---|---|---|
| UART service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | Primary service |
| Phone to StudyPet | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | Write / Write Without Response |
| StudyPet to phone | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | Notify |

- Send one UTF-8 JSON object terminated by **LF (0x0A)** or NUL (0x00). CRLF is
  accepted. Split the bytes into separate writes of **at most 20 bytes**, each
  starting at characteristic offset zero. Maximum command size is 384 bytes,
  excluding its terminator. A partial command expires after five seconds.
- Prefer acknowledged ATT writes, awaiting each write, and then wait for the
  command's application-level ACK/ERROR before sending the next command.
- Responses are JSON objects terminated by **LF**, split into notifications of
  at most 20 bytes. Reassemble until LF before parsing. Notification boundaries
  do not correspond to JSON messages. Use a streaming UTF-8 decoder.
- Optional `id` is a nonnegative uint32 or string up to 32 bytes, echoed in its
  reply. IDs correlate replies; they do not deduplicate repeated commands.
- Notification enablement sends an initial STATE. TIMER and STUDY events follow
  every five seconds, including idle/stopped states. Handle these unsolicited
  events separately from command replies. No delay() loop is used for chunks.
- Buffers and queues are bounded. Busy/oversized/partial/invalid messages return
  ERROR without executing their command. Disconnect/unsubscribe clears partial
  commands and queued replies. A stalled outgoing stream disconnects after 15 s.

The UART characteristics use the example's unpaired, unencrypted access model:
a connected, subscribed client can control StudyPet. This is not a bonded-only
or authenticated companion protocol.

## Time representation

BLE `timestamp` and deadline `due` values are **Unix UTC seconds since 1970**.
This removes the ambiguity in the supplied guide, whose direct assignment used
the firmware's internal local seconds since 2000. Phone clients must use Unix
seconds, not JavaScript milliseconds and not raw internal EEPROM timestamps.

The firmware converts to its local software clock using `utcOffsetMinutes`,
which defaults to **480 (Singapore UTC+08:00)**. SET_DATETIME may supply an offset
from -720 through 840 minutes. Use a consistent offset for the stored local due
dates; changing the offset does not migrate previously stored deadlines.
Accepted local dates are 2024 through 2099, matching the on-device date editor.

On connection the phone should send SET_DATETIME with its current Unix time.
The clock/offset are not battery-backed or persisted, while deadlines are.

## Commands

Envelope: `{"cmd":"COMMAND","data":{...},"id":1}`. Omit `data` where not needed.
Numeric fields require integers; booleans require true/false. Invalid settings
are rejected atomically, so valid fields in the same request are not applied.

| Command | Data / behavior |
|---|---|
| GET_STATE | Full STATE including timer/config, study, pet, settings, range, Unix clock, EEPROM state |
| TIMER_START | Start/resume; does not bypass TOF absence |
| TIMER_PAUSE | Manual pause |
| TIMER_SKIP | Skip phase using normal timer/buzzer events |
| TIMER_RESET | Return to idle |
| TIMER_CONFIG | `style`: 0 Pomodoro, 1 90-min, 2 Custom. Custom also requires `focusMin` 5..180 in steps of 5; `shortMin` 1..60; `longMin` 5..90 in steps of 5; `cycles` 1..8 |
| STUDY_START | `subject`: 0..SUBJECT_COUNT-1 |
| STUDY_STOP | Stop active session |
| STUDY_GET_HOURS | HOURS reply with per-subject total seconds |
| STUDY_RESET_HOURS | Clear totals, same as local reset |
| DEADLINES_GET | DEADLINES reply sorted by due date; each entry includes its stable `index` |
| DEADLINE_ADD | `subject`, `kind`, `lead`, `due` |
| DEADLINE_UPDATE | `index`, `deadline`: complete subject/kind/lead/due object |
| DEADLINE_DELETE | `index` from DEADLINES_GET, not its position in the sorted array |
| PET_SET | `species`: 0..5; `palette`: 0..2 |
| SET_DATETIME | `timestamp`: Unix seconds; optional `utcOffsetMinutes` |
| SETTINGS_SET | Any subset of `buzzerEnabled`, `irEnabled`, `irPausesTimer`, `awayIdx`, `repeatIdx`, `brightness`; TOF mode also accepts `tofPort` 0..3 |
| BUZZER_TEST | Play the normal break-start pattern if buzzer is enabled |

Settings bounds: awayIdx 0..4, repeatIdx 0..3, brightness 1..15. Unknown settings
keys are rejected. `irEnabled` is the presence-sensor toggle, including TOF.
The one-metre TOF cutoff remains fixed by shared configuration; awayIdx does
not add a delay in TOF mode. Time and deadline reminders continue while away.

Default indices: subjects INF1102=0, INF1103=1, INF1104=2, Revision=3, Other=4;
types Assignment=0, Quiz=1, Exam=2, Project=3, Lab=4, Other=5; pets Cat=0, Dog=1,
Fox=2, Panda=3, Capybara=4, Rabbit=5. Lead indices 0..8 correspond to at due,
1h/3h/6h/12h/1d/2d/3d/1w before. The source lists remain authoritative.

Normal mutations return `{"type":"ACK","message":"...","id":1}`. Reads return
STATE, DEADLINES, or HOURS with the same id. Invalid/missing fields, nonexistent
indices, a full list, or unverified EEPROM operations return ERROR instead.
Deadline success replies additionally include `index` and `persistent:true`.

Deadlines are committed using the same EEPROM manager as the local UI. Settings,
study hours, and pet choices remain volatile. Reminder acknowledgments are not
stored. A phone deadline mutation closes an existing local deadline edit/alert
so a stale editor cannot overwrite the new record. Unrelated screens retain
their navigation state, and new unsaved local drafts remain editable.

After a disconnected/unverified deadline mutation, DEADLINES_GET and local
Settings > EEPROM > Reload can resolve the actual saved state. Do not blindly
resend DEADLINE_ADD after losing its ACK: the original save might have completed.
EEPROM states: empty=0, saved=1, backup=2, corrupt=3, error=4, not-set=5.

## Smoke test examples

The following lines each require an actual LF terminator:

```json
{"cmd":"GET_STATE"}
{"cmd":"SET_DATETIME","id":1,"data":{"timestamp":1704067200,"utcOffsetMinutes":480}}
{"cmd":"TIMER_START","id":2}
{"cmd":"TIMER_PAUSE","id":3}
{"cmd":"DEADLINE_ADD","id":4,"data":{"subject":0,"kind":0,"lead":5,"due":1800000000}}
{"cmd":"DEADLINES_GET","id":5}
```

The example dates above are fixed test values, not the current time. In a phone
app use `Math.floor(Date.now()/1000)` for synchronization and Unix due times for
deadlines. A single-packet GET_STATE smoke test can also be sent as these hex
bytes (the final byte is LF):

```text
7B 22 63 6D 64 22 3A 22 47 45 54 5F 53 54 41 54 45 22 7D 0A
```

Verify that STATE reports cutoffMm=1000, and that TIMER_START remains awayPaused
when the target is farther than 1 m. For persistence, add a deadline, check its
ACK/persistent field, power-cycle, reconnect/synchronize time and read it back.
Test unsubscribe/reconnect and local button changes as well. Physical BLE radio
operation has not been verified on an attached ASD2116 in this session.
