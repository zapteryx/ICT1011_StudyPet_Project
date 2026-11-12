#pragma once
#include "Arduino.h"
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <cassert>
#include <algorithm>
struct FONT_CHAR_INFO { const uint8_t width; const uint16_t offset; };
struct FONT_INFO {
  const unsigned char height; const char startCh, endCh;
  const FONT_CHAR_INFO *charDesc; const unsigned char *bitmap;
};
// Use the installed TinyScreen library's real font data for pixel previews.
#include <font.h>
const uint8_t TinyScreenPlus = 2, TSBitDepth8 = 0;
const uint8_t TSButtonUpperLeft = 2, TSButtonUpperRight = 4;
const uint8_t TSButtonLowerLeft = 1, TSButtonLowerRight = 8;
const uint8_t TSRectangleFilled = 1, TSRectangleNoFill = 0;
const uint8_t TS_8b_Black = 0, TS_8b_White = 255, TS_8b_DarkBlue = 0x60;
class TinyScreen {
public:
  explicit TinyScreen(uint8_t = TinyScreenPlus) {}
  std::array<uint8_t, 96 * 64> pixels{};
  std::vector<std::string> printed;
  uint8_t buttons = 0;
  const FONT_INFO *font = &thinPixel7_10ptFontInfo;
  uint8_t cx = 0, cy = 0, x1 = 0, x2 = 95, y1 = 0, y2 = 63, fg = 255, bg = 0;
  void begin() {}
  void setBrightness(uint8_t) {}
  void setBitDepth(uint8_t) {}
  void setFont(const FONT_INFO &f) { font = &f; }
  uint8_t getButtons() { return buttons; }
  void clearScreen() { pixels.fill(0); printed.clear(); }
  void drawPixel(uint8_t x, uint8_t y, uint16_t color) {
    assert(x < 96 && y < 64); pixels[y * 96 + x] = color;
  }
  void drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t filled, uint16_t color) {
    assert(int(x) + w <= 96 && int(y) + h <= 64);
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx)
        if (filled || yy == y || yy == y + h - 1 || xx == x || xx == x + w - 1)
          drawPixel(xx, yy, color);
  }
  void drawLine(uint8_t x, uint8_t y, uint8_t xx, uint8_t yy, uint16_t color) {
    int dx = abs(int(xx) - x), sx = x < xx ? 1 : -1;
    int dy = -abs(int(yy) - y), sy = y < yy ? 1 : -1, err = dx + dy;
    int a = x, b = y;
    for (;;) {
      drawPixel(a, b, color); if (a == xx && b == yy) break;
      int e = 2 * err; if (e >= dy) { err += dy; a += sx; } if (e <= dx) { err += dx; b += sy; }
    }
  }
  void fontColor(uint16_t f, uint16_t b) { fg = f; bg = b; }
  void setCursor(uint8_t x, uint8_t y) { cx = x; cy = y; }
  uint8_t getPrintWidth(char *s) {
    unsigned width = 0;
    for (; *s; ++s) { assert(*s >= font->startCh && *s <= font->endCh); width += font->charDesc[*s - font->startCh].width + 1; }
    return uint8_t(width);
  }
  void print(const char *s) {
    printed.emplace_back(s);
    for (; *s; ++s) {
      assert(*s >= font->startCh && *s <= font->endCh);
      const auto &ch = font->charDesc[*s - font->startCh];
      const int bytes = (ch.width + 7) / 8;
      for (int y = 0; y < font->height; ++y)
        for (int x = 0; x <= ch.width + 1; ++x) {
          const int bit = x - 1;
          bool ink = bit >= 0 && bit < ch.width && (font->bitmap[ch.offset + bytes * font->height - 1 - y - (bytes - bit / 8 - 1) * font->height] & (0x80 >> (bit % 8)));
          assert(cx + x < 96 && cy + y < 64);
          drawPixel(cx + x, cy + y, ink ? fg : bg);
        }
      cx += ch.width + 1;
    }
  }
  void setX(uint8_t a, uint8_t b) { x1 = a; x2 = b; }
  void setY(uint8_t a, uint8_t b) { y1 = a; y2 = b; }
  void startData() {}
  void endTransfer() {}
  void writeBuffer(uint8_t *buffer, unsigned count) {
    assert(count == unsigned(x2 - x1 + 1) * (y2 - y1 + 1));
    for (unsigned i = 0; i < count; ++i)
      drawPixel(x1 + i % (x2 - x1 + 1), y1 + i / (x2 - x1 + 1), buffer[i]);
  }
  void save(const std::string &name) {
    std::ofstream out(name, std::ios::binary); assert(out.good()); out << "P6\n96 64\n255\n";
    for (auto c : pixels) {
      const char rgb[3] = {char((c & 3) * 255 / 3), char(((c >> 2) & 7) * 255 / 7), char(((c >> 5) & 7) * 255 / 7)};
      out.write(rgb, 3);
    }
  }
};
