# StudyPet — TinyScreen+ study companion

**Target:** TinyCircuits TinyScreen+ (ATSAMD21), 96 x 64 OLED, 8-bit colour.

## Files
| File | Role |
|---|---|
| `StudyPet_TinyScreenPlus.ino` | setup/loop; wires the state machine (input → features → pet → draw) |
| `Input.h` | **pin definitions**, button → Up/Down/Back/Select mapping (short / long / repeat), IR presence sensor |
| `ThermalCamera.h` | AST1045/AMG8833 I2C frames, temperatures, and warm-target heuristic |
| `DistanceSensor.h` | AST1024/VL53L0X initialization, checked distance frames, status and recovery |
| `Features.h` | buzzer patterns, software clock, break timer, study tracker, deadline CRUD + reminders, settings |
| `Pet.h` | 6 species × 3 colours, idle + interactive animation state (no drawing) |
| `UI.h` | home screen, three-row menus with button labels, button guide, editors, dashboard, alerts, pet sprite renderer |

## Wiring (defaults — change at the top of `Input.h`)
| Part | Pin | Notes |
|---|---|---|
| Buzzer / piezo | `A0` | `BUZZER_PASSIVE 1` = passive piezo via `tone()`; `0` = active buzzer |
| AST1024 TOF sensor | Adapter port 1 | I2C through `Wireling.selectPort(1)`; replaces the thermal camera |
| Optional digital IR/PIR | `A1` | Only with `PRESENCE_USE_TOF 0` and `IR_USE_THERMAL_CAMERA 0` |

TinyScreen+ GPIO is **3.3 V**. Power sensors from 3.3 V (AM312 is fine) or level-shift a 5 V output.
A small piezo can be driven directly; a magnetic buzzer needs a transistor.
Wireling Adapter support is now enabled by default (`USE_WIRELING_ADAPTER 1`).
Install **VL53L0X by Pololu**, **Wireling**, and **TinyScreen** in Library Manager. Startup calls
`Wireling.begin()` before initializing the buzzer. For directly wired parts without
an adapter, override `USE_WIRELING_ADAPTER` to `0` above the includes in the sketch.

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
  fixed **Cutoff: 100 cm** instead. Change `TOF_PRESENCE_MAX_MM` in `Input.h` only
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
- Data (deadlines, totals, settings) lives in RAM and resets on power-off; the clock restarts at the sketch's
  compile time. Set it in Settings > Set date & time. Persisting to flash (e.g. the FlashStorage library) is the next step.
- The updated UI was compiled for `TinyCircuits:samd:tinyscreen` using TinyCircuits SAMD 1.1.0.
  Desktop checks in `tests/ui_sim` use mocked hardware plus the installed TinyScreen library's
  real font data to exercise navigation, timers, editing, alerts, pixel bounds, and redraw caching.
  It has not been uploaded to or visually checked on a physical board in this session.
