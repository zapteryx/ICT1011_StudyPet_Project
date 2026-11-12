# StudyPet — TinyScreen+ study companion

**Target:** TinyCircuits TinyScreen+ (ATSAMD21), 96 x 64 OLED, 8-bit colour.

## Files
| File | Role |
|---|---|
| `StudyPet_TinyScreenPlus.ino` | setup/loop; wires the state machine (input → features → pet → draw) |
| `Input.h` | **pin definitions**, button → Up/Down/Back/Select mapping (short / long / repeat), IR presence sensor |
| `Features.h` | buzzer patterns, software clock, break timer, study tracker, deadline CRUD + reminders, settings |
| `Pet.h` | 6 species × 3 colours, idle + interactive animation state (no drawing) |
| `UI.h` | home screen, three-row menus with button labels, button guide, editors, dashboard, alerts, pet sprite renderer |

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
  The timer menu shows `IR HOLD` for presence pauses and `PAUSED` for a manual pause.

If no presence sensor is attached, switch **Settings > IR sensor** off after each restart.
The IR and buzzer defaults, timing behavior, and all original shortcuts are unchanged.

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
- The updated UI was compiled for `TinyCircuits:samd:tinyscreen` using TinyCircuits SAMD 1.1.0.
  Desktop checks in `tests/ui_sim` use mocked hardware plus the installed TinyScreen library's
  real font data to exercise navigation, timers, editing, alerts, pixel bounds, and redraw caching.
  It has not been uploaded to or visually checked on a physical board in this session.
