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

On this machine Windows Application Control blocked the current test executables
even outside the sandbox. They were syntax-checked, but these new runtime tests
have not executed here. Physical sensor accuracy and buzzer output require board
testing; compiling the sketch does not validate them.

Compile the actual board sketch separately with Arduino IDE or Arduino CLI.
Neither the desktop harness nor a board compilation uploads anything.
