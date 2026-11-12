# Desktop UI checks

This harness runs the actual `UI.h`, `Features.h`, `Pet.h`, and `Input.h` against
mock hardware. It reads the installed TinyScreen library's real `font.h` data;
no font copy or additional Arduino library is bundled here.

From the sketch directory, with g++ on PATH, adjust the library path and run:

```powershell
$tinyScreenLibrary = 'C:\path\to\Arduino\libraries\TinyScreen'
New-Item -ItemType Directory -Force '.verification\previews' | Out-Null
g++ -std=c++11 -Wall -Wextra -I tests/ui_sim -I $tinyScreenLibrary tests/ui_sim/test_ui.cpp -o .verification/ui_tests.exe
& .\.verification\ui_tests.exe .verification/previews
g++ -std=c++11 -Wall -Wextra -I tests/ui_sim -I $tinyScreenLibrary tests/ui_sim/test_thermal.cpp -o .verification/thermal_tests.exe
& .\.verification\thermal_tests.exe
g++ -std=c++11 -Wall -Wextra -I tests/ui_sim -I $tinyScreenLibrary tests/ui_sim/test_distance.cpp -o .verification/distance_tests.exe
& .\.verification\distance_tests.exe
g++ -std=c++11 -Wall -Wextra -I tests/ui_sim -I $tinyScreenLibrary tests/ui_sim/test_eeprom.cpp -o .verification/eeprom_tests.exe
& .\.verification\eeprom_tests.exe
g++ -std=c++11 -Wall -Wextra -I tests/ui_sim -I $tinyScreenLibrary -I 'C:\path\to\Arduino\libraries\ArduinoJson\src' tests/ui_sim/test_ble.cpp -o .verification/ble_tests.exe
& .\.verification\ble_tests.exe
g++ -std=c++17 -Wall -Wextra -I tests/ui_sim/ble_radio -I tests/ui_sim -I $tinyScreenLibrary -I 'C:\path\to\Arduino\libraries\ArduinoJson\src' tests/ui_sim/test_ble_radio.cpp -o .verification/ble_radio_tests.exe
& .\.verification\ble_radio_tests.exe
```

The test checks navigation, original shortcuts, option commit/cancel, clock
editing, all pet previews, deadline editing/deletion, alert interruption and
return, presence pauses, independent break timing, dashboard scrolling, and
redraw caching. Drawing outside 96x64 pixels fails an assertion. PPM screen
captures are saved under `.verification/previews` for visual review.

`test_ui.cpp` selects the legacy digital presence mode to retain its existing
navigation regression cases. `test_thermal.cpp` separately checks AST1045 I2C
frames, warm-up, warm-target detection, signed temperature decoding, short reads,
camera disconnection/retry, camera port changes, and AST1035 tone patterns.

`test_distance.cpp` covers the default AST1024 mode: 999/1000/1001 mm boundaries,
return confirmation, immediate far detection, focus and short/long break holds,
study-hour holds, manual pause retention, invalid range status, 8190 mm sentinels,
too-close readings, stale frames, disconnection, retry, short transfers, sensor
port changes, user overrides, and keeping deadline/clock time running.

`test_eeprom.cpp` uses a simulated page-wrapping EEPROM with busy ACK polling,
write protection, short reads, disconnection and byte-level interrupted writes.
It checks create/edit/delete across reboots, full capacity, corruption fallback,
partial-write recovery, failed mutation rollback, reconnection without overwriting
existing deadlines, and avoiding writes for reminder updates or unchanged values.
An earlier run passed 205 EEPROM checks. Expanded first-use cases also cover
zero/example data versus damaged recognizable records; Windows blocked that
expanded executable. These checks model software behavior, not physical wiring
or electrical power-loss behavior.

`test_ble.cpp` exercises the production BleProtocol class with real ArduinoJson:
split writes, LF/NUL/CRLF framing, invalid/missing fields, atomic configuration,
Unix/local time conversion, all supported command families, EEPROM failures,
full lists, eight-entry responses, stable indices, queue backpressure, chunk
retries, partial-frame timeouts, reset behavior, telemetry and TOF holds.
The UI checks additionally cover BLE status/retry and closing stale local edits
after a phone changes deadlines. Use ArduinoJson 6.21.5 on the native include path.

`test_ble_radio.cpp` includes the production BleLink.cpp with transport fakes in
`ble_radio/`. Put `-I tests/ui_sim/ble_radio` before the other include paths.
It checks that startup and retry retain STBLE's reply interrupt, each startup
failure reports its stage/status, and a missing shield can recover on retry.
This model does not validate physical radio operation or the vendor IRQ handler.
The startup/IRQ/retry/error-stage checks compiled and executed successfully after
the IRQ startup fix. The TinyScreen+ board build also passed (80,884 bytes).

Windows Application Control blocked BLE and expanded UI/thermal/TOF test
executables in this session, including an outside-sandbox attempt. Those sources
compiled, but their runtime assertions have not executed. The earlier EEPROM test
executable was permitted and executed successfully.
Physical sensor accuracy, buzzer output and EEPROM wiring still require board
testing; compiling the sketch does not validate them.

Compile the actual board sketch separately with Arduino IDE or Arduino CLI.
Neither the desktop harness nor a board compilation uploads anything.
