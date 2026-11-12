# StudyPet Companion v1.1 — ASD2116 synchronization

The corrected PWA is in this firmware project's `pwa/` folder. The original
`C:/Users/raiso/Downloads/pwa` was copied here and left unchanged. Publish the
corrected folder to the same web host/URL you currently open on your phone.
No firmware re-upload is required for these PWA changes: keep the firmware with
the BLE interrupt startup fix already running on your TinyScreen+.

## Deploy and connect

1. Replace the hosted app with **all files from this corrected `pwa/` folder**
   (including index.html, app.js, ble.js, service-worker.js, manifest and icons).
   No build step or npm installation is needed to host these static files.
2. Use an HTTPS URL on the phone. A desktop development server's `localhost`
   address is only local to that computer. A plain LAN HTTP address on the phone
   is not a secure context for Web Bluetooth. See the
   [Chrome Web Bluetooth guide](https://developer.chrome.com/docs/capabilities/bluetooth).
3. **Clear the old site's cached data/service worker once**, then reopen the URL
   online. Alternatively remove and reinstall the PWA after clearing site data.
   The About card must show **StudyPet Companion App v1.1 · ASD2116**. The old
   cache-first worker could otherwise keep serving the original broken scripts.
4. Select Connect Device, choose StudyPet, and wait for **Connected · synced**.
   The app synchronizes the software clock, reads STATE, reads EEPROM deadlines
   and loads study totals. The device's internal local time stays at UTC+08:00;
   deadline input/display on the phone uses the phone's timezone and Unix UTC
   seconds on the wire. Use the same phone timezone as desired for date entry.
5. Try Start/Pause, change the pet, and edit a deadline. Saves report success
   after the device acknowledges them. Timers continue to obey the 1 m TOF
   presence rule; the timer/study labels explain a presence hold.

The folder can be hosted at either the website root or a subfolder. The worker
uses relative assets, includes the supplied SVG icons, and prefers fresh files
online while retaining an offline copy. Offline mode displays the app; device
controls still require an active Bluetooth connection. Commands are never
stored for replay when disconnected.

## What was fixed

- Commands now end with a real LF byte and are encoded into writes of at most
  20 **bytes**. The original code omitted the delimiter, so firmware never
  received a complete command.
- A streaming UTF-8 decoder accumulates notifications until LF. The original
  code tried to parse every 20-byte fragment as a complete JSON document.
- Commands are serialized and matched to replies by `id`. Telemetry is handled
  separately. Device ERROR responses reject operations; a transport timeout
  disconnects and invalidates queued commands. Failed saves are not retried
  automatically, since an EEPROM write might already have completed.
- Numeric phase values map to idle/focus/short/long break; progress uses the
  real configured phase duration. Countdown estimates respect counting,
  manual pause and TOF hold, and stop when disconnected/telemetry is stale.
- Mutation ACKs trigger fresh device reads. TIMER/STUDY events update every
  five seconds; full state, deadlines and totals refresh every eight seconds
  while the page is visible and idle, reflecting local device button changes.
- Pet palette options are rebuilt for the reported species. Device timer
  configuration, settings, TOF port/range and EEPROM health are displayed.
  Unsaved pet/settings/timer form edits survive periodic refreshes.
- Sorted deadlines retain the EEPROM `index`; edits/deletes use that slot,
  not the sorted row number. A remotely changed/deleted record closes a stale
  editor. New drafts survive list updates.
- The Connect button has one event handler; disconnect no longer also starts
  another connection attempt. Notification handlers are attached before
  subscribing and removed on disconnect.

The current firmware command/schema source is [docs/BLE.md](../docs/BLE.md).
EEPROM persists deadlines; pet/settings and study totals retain their existing
volatile behavior. This update does not change those persistence rules.

## Verification

Run the dependency-free JavaScript checks from the project root:

```powershell
node pwa/tests/sync.test.cjs
```

For a real mobile-size browser/DOM test with a simulated GATT peripheral, install
Playwright in your development environment and use Microsoft Edge:

```powershell
node pwa/tests/browser.test.cjs
```

If Playwright is installed outside this folder, set `STUDYPET_PLAYWRIGHT` to its
module directory. The checks exercise fragmented messages/UTF-8, serialized
commands, timeout/disconnect/error handling, numeric phases, TOF holds, pet and
settings synchronization, stable deadline slots, failed saves, local changes,
and one Connect/Disconnect handler. Browser checks also cover service-worker
installation, subfolder hosting and offline reload.

`tests/ui_sim/ble_bridge.cpp` optionally connects these JS tests to the real
firmware BleProtocol with mock Wire/EEPROM. Compile with the same include paths
as `tests/ui_sim/test_ble.cpp`, then pass the executable as the first argument to
`sync.test.cjs`. It covers production timer/study/pet/settings commands and
EEPROM CRUD across a simulated reboot. The bridge compiled in this session,
but Windows Application Control blocked its execution, even outside the sandbox.
Its combined runtime checks are **unverified**. JavaScript and browser checks
passed; physical phone/ASD2116 round-trip operation still needs user testing.
