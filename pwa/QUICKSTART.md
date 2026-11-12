# StudyPet PWA - Quick Start Guide

> Use [SYNC_FIX.md](SYNC_FIX.md) for this project's corrected deployment steps.
> Phone access needs HTTPS; `localhost` on a phone refers to the phone itself.
> The original local-server example below applies to the computer serving it.

## What You've Built

A Progressive Web App that connects to your StudyPet TinyCircuit device via Bluetooth Low Energy. Control everything from your phone!

## Quick Start

### 1. Deploy the PWA

**Option A: Quick Test (Local)**
```bash
cd pwa
python3 -m http.server 8000
```
Open `http://localhost:8000` on your phone (same WiFi network)

**Option B: Deploy Online (Recommended)**

Deploy to Netlify (free, HTTPS included):
```bash
cd pwa
npx netlify-cli deploy --prod
```

Or use GitHub Pages:
1. Push to GitHub
2. Enable Pages in repo settings
3. Deploy from `main` branch, `/pwa` folder

### 2. Create Icons

Quick icons with ImageMagick:
```bash
cd pwa
convert -size 192x192 xc:#1a3a52 -gravity center \
  -pointsize 72 -fill white -annotate +0+0 "SP" icon-192.png
convert -size 512x512 xc:#1a3a52 -gravity center \
  -pointsize 192 -fill white -annotate +0+0 "SP" icon-512.png
```

Or use https://realfavicongenerator.net/

### 3. Add BLE to Firmware

Add to your `StudyPet_TinyScreenPlus.ino`:

```cpp
#include <BLEPeripheral.h>
#include <ArduinoJson.h>

// ... see FIRMWARE.md for full implementation
```

**Key Requirements:**
- Nordic UART Service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- Device name: "StudyPet"
- JSON command protocol

### 4. Test the Connection

1. Upload BLE firmware to TinyCircuit
2. Open PWA on phone (must be HTTPS or localhost)
3. Click "Connect Device"
4. Select "StudyPet" from Bluetooth dialog
5. Start controlling your device!

## Browser Requirements

✅ **Supported:**
- Chrome/Edge on Android
- Chrome/Edge on Desktop (Windows/Mac/Linux)

❌ **Not Supported:**
- iOS Safari (use Bluefy browser instead)
- Firefox (Web Bluetooth disabled by default)

## Features

- **Timer Control**: Start/pause/skip Pomodoro timer
- **Study Tracking**: Track hours per subject
- **Deadlines**: Add/edit/delete with reminders
- **Pet Customization**: Change species and colors
- **Settings Sync**: Buzzer, IR sensor, brightness
- **Offline Support**: UI works offline, syncs when connected

## Next Steps

1. Read `README.md` for full documentation
2. Read `FIRMWARE.md` for BLE implementation details
3. Customize the UI in `styles.css`
4. Add features in `app.js`

## Need Help?

- BLE not working? Check `chrome://bluetooth-internals/`
- PWA not installing? Must be served over HTTPS
- Device won't connect? Check advertising and UUIDs match
- Commands failing? Check firmware implements JSON protocol

## File Structure

```
pwa/
├── index.html          # Main UI
├── styles.css          # Styling
├── app.js             # App logic
├── ble.js             # Bluetooth communication
├── service-worker.js  # Offline support
├── manifest.json      # PWA config
├── icon-192.png       # App icon
├── icon-512.png       # App icon
├── README.md          # Full docs
├── FIRMWARE.md        # BLE implementation
└── QUICKSTART.md      # This file
```

Enjoy your StudyPet PWA! 🐱📱
