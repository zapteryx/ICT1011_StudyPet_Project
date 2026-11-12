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
```

The test checks navigation, original shortcuts, option commit/cancel, clock
editing, all pet previews, deadline editing/deletion, alert interruption and
return, presence pauses, independent break timing, dashboard scrolling, and
redraw caching. Drawing outside 96x64 pixels fails an assertion. PPM screen
captures are saved under `.verification/previews` for visual review.

Compile the actual board sketch separately with Arduino IDE or Arduino CLI.
Neither the desktop harness nor a board compilation uploads anything.
