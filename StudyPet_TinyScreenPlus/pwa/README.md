# StudyPet Companion PWA

**ASD2116 integration updated to v1.1.** Deploy this project folder's corrected
PWA, then reload the phone's installed app/cache once. See [SYNC_FIX.md](SYNC_FIX.md)
for exact steps, the current firmware contract and test results. The original
setup notes below are retained as background; use the updated guide first.

A Progressive Web App that communicates with your StudyPet TinyCircuit device via Bluetooth Low Energy (BLE). Control timers, track study sessions, manage deadlines, and customize your pet from your phone.

## Features

- 📱 **Progressive Web App** - Install on your phone home screen for native app experience
- 🔵 **Bluetooth LE** - Connect wirelessly to your TinyCircuit device
- ⏱️ **Timer Control** - Start, pause, skip, and configure Pomodoro/custom timers
- 📚 **Study Tracking** - Monitor active study sessions and view hours dashboard
- 📅 **Deadline Management** - Add, edit, and track deadlines with reminders
- 🐱 **Pet Customization** - Change species and color palettes
- ⚙️ **Device Settings** - Configure buzzer, IR sensor, brightness, and more
- 📴 **Offline Support** - UI works offline, syncs when reconnected

## Setup

### Web App Deployment

#### Option 1: Local Development Server

```bash
cd pwa
python -m http.server 8000
# Or use Node.js
npx serve .
```

Then open `http://localhost:8000` in your browser.

#### Option 2: GitHub Pages

1. Push the `pwa` folder to a GitHub repository
2. Go to Settings > Pages
3. Select the branch and `/pwa` folder
4. Your PWA will be available at `https://yourusername.github.io/repo-name/`

#### Option 3: Netlify/Vercel

1. Drag and drop the `pwa` folder to Netlify/Vercel
2. Your PWA will be deployed with HTTPS (required for Web Bluetooth)

### TinyCircuit Firmware

The TinyCircuit firmware needs to implement BLE UART service with the following UUIDs:

- **Service UUID**: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- **TX Characteristic**: `6e400002-b5a3-f393-e0a9-e50e24dcca9e` (Write)
- **RX Characteristic**: `6e400003-b5a3-f393-e0a9-e50e24dcca9e` (Notify)

Use [the current ASD2116 BLE contract](../docs/BLE.md). The original `FIRMWARE.md`
is a historical design reference; BLE is already implemented in this project.

### Icons

Replace the placeholder icon files with actual icons:

```bash
# Using ImageMagick (if installed)
cd pwa
convert -size 192x192 xc:#1a3a52 -gravity center -pointsize 72 -fill white -annotate +0+0 "SP" icon-192.png
convert -size 512x512 xc:#1a3a52 -gravity center -pointsize 192 -fill white -annotate +0+0 "SP" icon-512.png
```

Or create professional icons using:
- [RealFaviconGenerator](https://realfavicongenerator.net/)
- [Figma](https://www.figma.com/)
- Any image editor

## Usage

### Connecting to Device

1. Open the PWA in a Web Bluetooth-compatible browser (Chrome, Edge, Opera on Android/Desktop)
2. Click "Connect Device"
3. Select your StudyPet device from the Bluetooth pairing dialog
4. Once connected, the UI will sync with your device

### Browser Compatibility

Web Bluetooth requires:
- ✅ Chrome/Edge/Opera on Android
- ✅ Chrome/Edge on Windows/macOS/Linux
- ✅ Chromium-based browsers on Desktop
- ❌ iOS Safari (Web Bluetooth not supported)
- ❌ Firefox (Web Bluetooth not enabled by default)

**Note**: iOS users can use the [Bluefy browser](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055) as a workaround.

### Installing as PWA

#### Android
1. Open the app in Chrome
2. Tap the menu (⋮) > "Install app" or "Add to Home screen"
3. The app will appear on your home screen like a native app

#### Desktop
1. Open the app in Chrome/Edge
2. Click the install icon in the address bar
3. The app will open in its own window

#### iOS (Limited)
1. Open in Safari
2. Tap Share > "Add to Home Screen"
3. Note: BLE functionality requires Bluefy browser

## API Reference

### BLE Command Protocol

The app sends JSON commands to the device:

```json
{
  "cmd": "COMMAND_NAME",
  "data": { ... }
}
```

The device responds with:

```json
{
  "type": "RESPONSE_TYPE",
  "data": { ... }
}
```

### Commands

#### Timer Commands
- `TIMER_START` - Start/resume timer
- `TIMER_PAUSE` - Pause timer
- `TIMER_SKIP` - Skip to next phase
- `TIMER_RESET` - Reset timer to idle
- `TIMER_CONFIG` - Set timer configuration
  ```json
  {
    "style": 0,        // 0=Pomodoro, 1=90min, 2=Custom
    "focusMin": 25,
    "shortMin": 5,
    "longMin": 15,
    "cycles": 4
  }
  ```

#### Study Commands
- `STUDY_START` - Start study session
  ```json
  { "subject": 0 }  // 0-4
  ```
- `STUDY_STOP` - Stop session
- `STUDY_GET_HOURS` - Request hours data
- `STUDY_RESET_HOURS` - Reset all hours

#### Deadline Commands
- `DEADLINES_GET` - Request all deadlines
- `DEADLINE_ADD` - Add new deadline
- `DEADLINE_UPDATE` - Update existing deadline
- `DEADLINE_DELETE` - Delete deadline

#### Settings Commands
- `GET_STATE` - Request full device state
- `SET_DATETIME` - Sync device clock
- `SETTINGS_SET` - Update settings
- `PET_SET` - Change pet appearance
- `BUZZER_TEST` - Test buzzer

## Development

### File Structure

```
pwa/
├── index.html           # Main app UI
├── styles.css           # Styling
├── app.js               # App logic and state management
├── ble.js               # BLE communication layer
├── service-worker.js    # PWA offline support
├── manifest.json        # PWA manifest
├── icon-192.png         # App icon (192x192)
└── icon-512.png         # App icon (512x512)
```

### Debugging

Enable Chrome DevTools for BLE debugging:

1. Open `chrome://bluetooth-internals/`
2. Monitor BLE connections and data
3. Use Console to see logs from `ble.js`

### Testing Without Device

The app UI works without a connected device. You can:
- Navigate all tabs
- Interact with forms and buttons
- Test the interface design

To test with device, ensure:
- Device is powered on
- BLE service is advertising
- Device is within range (~10 meters)

## Security

- **HTTPS Required**: Web Bluetooth only works over HTTPS (or localhost for development)
- **User Permission**: User must explicitly pair the device
- **Secure Commands**: Implement command validation in firmware
- **Data Privacy**: All data stays local (device ↔ phone), no cloud sync

## Troubleshooting

### Cannot Connect
- Ensure device is powered on and advertising
- Check device is in range
- Try refreshing the page
- Check browser permissions for Bluetooth

### Connection Drops
- Move closer to device
- Check battery level
- Avoid BLE interference (WiFi routers, other devices)

### Commands Not Working
- Check firmware implements all commands
- Verify UUIDs match between app and firmware
- Check Console for error messages

### PWA Won't Install
- Ensure served over HTTPS
- Check manifest.json is valid
- Verify icons exist and are correct size
- Try clearing cache and reloading

## Future Enhancements

- [ ] Data backup/restore via file export
- [ ] Multiple device support
- [ ] Notification API integration for deadline alerts
- [ ] Widget support (Android)
- [ ] Voice commands
- [ ] Dark/light theme toggle
- [ ] Localization support

## License

Part of the StudyPet project. See main repository for license details.

## Credits

Built with Web Bluetooth API and Progressive Web App technologies.
