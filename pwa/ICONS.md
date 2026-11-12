# Icon Creation Instructions

The PWA needs two icon files:
- `icon-192.png` (192x192 pixels)
- `icon-512.png` (512x512 pixels)

## Quick Options

### Option 1: Use Provided SVG Files (Temporary)

I've created `icon-192.svg` and `icon-512.svg` as placeholders. These will work but PNG is preferred.

### Option 2: Convert SVG to PNG

If you have ImageMagick installed:
```bash
cd pwa
convert icon-192.svg icon-192.png
convert icon-512.svg icon-512.png
```

Or use an online converter: https://cloudconvert.com/svg-to-png

### Option 3: Run the Creation Script

```bash
cd pwa
./create-icons.sh
```

This requires ImageMagick to be installed.

### Option 4: Use Online Icon Generator

Visit https://realfavicongenerator.net/ and:
1. Upload any image (logo, pet emoji, etc.)
2. Generate icons
3. Download the package
4. Replace icon-192.png and icon-512.png

### Option 5: Design Custom Icons

Use Figma, Photoshop, or GIMP to create:
- 192x192 PNG with transparent or solid background
- 512x512 PNG with the same design
- Use StudyPet branding colors (#1a3a52, #2e5a7d)

## Updating manifest.json

If you change icon filenames, update `manifest.json`:

```json
"icons": [
  {
    "src": "your-icon-192.png",
    "sizes": "192x192",
    "type": "image/png"
  },
  {
    "src": "your-icon-512.png",
    "sizes": "512x512",
    "type": "image/png"
  }
]
```

## Testing Icons

After creating icons:
1. Open the PWA in Chrome
2. Open DevTools > Application > Manifest
3. Check if icons load correctly
4. Install the PWA to see the icon on home screen
