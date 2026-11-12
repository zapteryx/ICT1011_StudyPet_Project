#!/bin/bash
# Simple icon creation script using ImageMagick
# Run this to create icons if ImageMagick is installed

if command -v convert &> /dev/null; then
    # Create 192x192 icon
    convert -size 192x192 xc:"#1a3a52" \
        -draw "fill #2e5a7d circle 96,96 96,36" \
        -draw "stroke #3b82f6 stroke-width 4 fill none circle 96,96 96,36" \
        -gravity center -pointsize 60 -fill white -font "DejaVu-Sans-Bold" \
        -annotate +0+0 "SP" icon-192.png
    echo "✓ Created icon-192.png"
    
    # Create 512x512 icon
    convert -size 512x512 xc:"#1a3a52" \
        -draw "fill #2e5a7d circle 256,256 256,96" \
        -draw "stroke #3b82f6 stroke-width 10 fill none circle 256,256 256,96" \
        -gravity center -pointsize 160 -fill white -font "DejaVu-Sans-Bold" \
        -annotate +0+0 "SP" icon-512.png
    echo "✓ Created icon-512.png"
else
    echo "ImageMagick not installed. Create icons manually or use online tools."
    echo "Recommended: https://realfavicongenerator.net/"
fi
