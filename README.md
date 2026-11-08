# StudyPet — TinyScreen+ study companion

**Target:** TinyCircuits TinyScreen+ (ATSAMD21), 96 x 64 OLED, 8-bit colour.

## Files
| File | Role |
|---|---|
| `StudyPet_TinyScreenPlus.ino` | setup/loop; wires the state machine (input → features → pet → draw) |
| `Input.h` | **pin definitions**, button → Up/Down/Back/Select mapping (short / long / repeat), IR presence sensor |
| `Features.h` | buzzer patterns, software clock, break timer, study tracker, deadline CRUD + reminders, settings |
| `Pet.h` | 6 species × 3 colours, idle + interactive animation state (no drawing) |
| `UI.h` | home screen, full-screen menu stack (max 4 rows), editors, dashboard, alert pop-up, pet sprite renderer |

## Wiring (defaults — change at the top of `Input.h`)
| Part | Pin | Notes |
|---|---|---|
| Buzzer / piezo | `A0` | `BUZZER_PASSIVE 1` = passive piezo via `tone()`; `0` = active buzzer |
| IR / PIR sensor OUT | `A1` | `IR_ACTIVE_LEVEL HIGH` for PIR (AM312, HC-SR501); `LOW` for most IR obstacle boards |

TinyScreen+ GPIO is **3.3 V**. Power sensors from 3.3 V (AM312 is fine) or level-shift a 5 V output.
A small piezo can be driven directly; a magnetic buzzer needs a transistor.
If you use a Wireling Adapter, set `USE_WIRELING_ADAPTER 1` and confirm the port pins / power pin.

## Buttons
Top Right = **Up** · Bottom Right = **Down** · Top Left = **Back** · Bottom Left = **Select**

**Home (pet) screen**
- Tap Up → jump · Down → nap · Back → wiggle · Select → love (each ≤ 5 s)
- Hold Select → menu · Hold Back → start/pause timer · Hold Up → start/stop study session · Hold Down → hours dashboard

**Menus:** Up/Down move (hold to auto-repeat), Select opens, Back returns, hold Back = straight home.

## Features
- **Break timer:** Pomodoro (25/5 × 4, 15 long, then reset), 90-min (90/20 × 2, 30 long), or Custom.
  3 short beeps when a break starts, 1 chirp when focus resumes. Away time pauses *focus* only (toggle in Settings).
- **Deadlines:** create / view / edit / delete up to 8. Per-deadline reminder window (at due … 1 week before).
  Footer banner appears inside the window; full-screen pop-up + **long** beeps when it starts and when overdue,
  re-alerting every 15/30/60 min until acknowledged (Settings).
- **Study tracker:** per-subject totals, live session time, dashboard with bars. Edit `SUBJECT_NAMES` in `Features.h`.
- **IR presence:** away after 10 s – 5 min without detection (PIR detects *motion*, so 1–2 min suits a still student);
  resumes the instant you're detected. Settings > "IR now" shows the raw sensor reading for wiring checks.

## Limitations
- Data (deadlines, totals, settings) lives in RAM and resets on power-off; the clock restarts at the sketch's
  compile time. Set it in Settings > Set date & time. Persisting to flash (e.g. the FlashStorage library) is the next step.
- Compiled and exercised on a desktop simulator with mocked TinyScreen/Arduino APIs; not yet compiled for or run on hardware.
