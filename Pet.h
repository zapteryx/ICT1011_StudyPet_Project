#ifndef STUDYPET_PET_H
#define STUDYPET_PET_H

#include <Arduino.h>
#include <string.h>
#include "Input.h"

// TinyScreen 8-bit colour layout is BBBGGGRR (see TS_8b_* constants).
// r: 0-3, g: 0-7, b: 0-7
#define C8(r, g, b) ((uint8_t)((((b) & 7) << 5) | (((g) & 7) << 2) | ((r) & 3)))

// =====================================================================
//  SPECIES + COLOUR PALETTES
// =====================================================================
enum PetSpecies : uint8_t {
  PET_CAT = 0, PET_DOG, PET_FOX, PET_PANDA, PET_CAPYBARA, PET_RABBIT, PET_COUNT
};

enum EarStyle : uint8_t {
  EAR_POINTY,       // cat
  EAR_FLOPPY,       // dog
  EAR_TALL_POINTY,  // fox
  EAR_ROUND,        // panda
  EAR_TINY,         // capybara
  EAR_LONG          // rabbit
};

struct PetPalette {
  const char *name;
  uint8_t fur;     // head
  uint8_t edge;    // outline
  uint8_t inner;   // inner ear
  uint8_t muzzle;  // lower face / snout
  uint8_t mark;    // markings (panda patches, dog ears, fox ear tips)
  uint8_t nose;
  uint8_t eye;
};

static const uint8_t PALETTES_PER_PET = 3;

struct PetSpeciesInfo {
  const char *name;
  EarStyle ears;
  PetPalette palettes[PALETTES_PER_PET];
};

// Set to 1 to put the little mandarin back on the capybara's head.
#ifndef CAPY_MANDARIN
#define CAPY_MANDARIN 0
#endif

#define PINK C8(3, 3, 4)
#define WHITE8 C8(3, 7, 7)
#define INK C8(0, 0, 0)

static const PetSpeciesInfo PET_SPECIES[PET_COUNT] = {
  {"Cat", EAR_POINTY, {
    {"Ginger",   C8(3,4,0), C8(2,2,0), PINK, C8(3,7,5), C8(2,2,0), PINK, INK},
    {"Grey",     C8(2,5,5), C8(1,2,2), PINK, WHITE8,    C8(1,3,3), PINK, INK},
    {"Midnight", C8(1,1,2), C8(1,3,4), PINK, C8(1,2,3), C8(1,1,2), PINK, C8(2,7,0)}}},
  {"Dog", EAR_FLOPPY, {
    {"Golden",   C8(3,5,1), C8(2,3,0), PINK, C8(3,7,5), C8(2,3,0), INK, INK},
    {"Choco",    C8(2,3,1), C8(1,1,0), PINK, C8(3,6,4), C8(1,1,0), INK, INK},
    {"Spotty",   WHITE8,    C8(1,3,3), PINK, WHITE8,    C8(1,1,1), INK, INK}}},
  {"Fox", EAR_TALL_POINTY, {
    {"Red",      C8(3,3,0), C8(2,1,0), WHITE8,    WHITE8, C8(1,1,1), INK, INK},
    {"Arctic",   WHITE8,    C8(2,5,6), C8(3,5,6), WHITE8, C8(1,3,4), INK, INK},
    {"Silver",   C8(1,3,3), C8(1,2,2), WHITE8,    WHITE8, C8(0,1,1), INK, INK}}},
  {"Panda", EAR_ROUND, {
    {"Classic",  WHITE8,    C8(2,5,5), PINK, WHITE8,    C8(1,1,1), INK, WHITE8},
    {"Qinling",  C8(3,6,5), C8(2,4,3), PINK, C8(3,7,6), C8(1,1,0), INK, WHITE8},
    {"Bubblegum",C8(3,5,6), C8(2,3,5), PINK, WHITE8,    C8(2,1,4), INK, WHITE8}}},
  {"Capybara", EAR_TINY, {
    // fur, outline, ear inner, (unused), muzzle patch, feature lines, eye
    {"Caramel",  C8(2,3,1), C8(1,1,0), C8(2,2,1), C8(2,3,1), C8(2,2,1), C8(1,0,0), INK},
    {"Peach",    C8(3,5,3), C8(1,1,0), C8(2,3,2), C8(3,5,3), C8(2,3,2), C8(1,1,0), C8(1,1,0)},
    {"Mocha",    C8(2,2,1), C8(1,0,0), C8(1,1,0), C8(2,2,1), C8(1,1,0), INK,       INK}}},
  {"Rabbit", EAR_LONG, {
    {"Snow",     WHITE8,    C8(2,5,6), PINK, WHITE8,    WHITE8,    PINK, INK},
    {"Cocoa",    C8(2,3,1), C8(1,2,0), PINK, C8(3,6,4), C8(2,3,1), PINK, INK},
    {"Grey",     C8(2,4,4), C8(1,2,3), PINK, WHITE8,    C8(2,4,4), PINK, INK}}},
};

// =====================================================================
//  ANIMATION STATE
// =====================================================================
enum PetAction : uint8_t {  // interactive, triggered by buttons on home
  ACT_NONE = 0,
  ACT_JUMP,    // UP     (top right)
  ACT_NAP,     // DOWN   (bottom right)
  ACT_WIGGLE,  // BACK   (top left)
  ACT_LOVE     // SELECT (bottom left)
};

enum PetIdle : uint8_t {  // passive, chosen randomly every few seconds
  IDLE_NONE = 0, IDLE_LOOK, IDLE_EAR, IDLE_YAWN
};

enum EyeStyle : uint8_t { EYES_OPEN = 0, EYES_CLOSED, EYES_HAPPY };
enum MouthStyle : uint8_t { MOUTH_NEUTRAL = 0, MOUTH_SMILE, MOUTH_OPEN, MOUTH_FLAT };
enum PetExtra : uint8_t { EXTRA_NONE = 0, EXTRA_HEART, EXTRA_ZZZ, EXTRA_SPARKLE };

// Everything the renderer needs for one frame. No drawing happens here.
struct PetPose {
  uint8_t species;
  uint8_t palette;
  int8_t dx;
  int8_t dy;
  int8_t lookX;
  uint8_t eyes;
  uint8_t mouth;
  uint8_t extra;
  uint8_t extraPhase;
  bool earTwitch;
  bool blush;
};

inline bool samePose(const PetPose &a, const PetPose &b) {
  return a.species == b.species && a.palette == b.palette && a.dx == b.dx &&
         a.dy == b.dy && a.lookX == b.lookX && a.eyes == b.eyes &&
         a.mouth == b.mouth && a.extra == b.extra &&
         a.extraPhase == b.extraPhase && a.earTwitch == b.earTwitch &&
         a.blush == b.blush;
}

class Pet {
public:
  static const unsigned long MAX_ACTION_MS = 5000UL;  // hard cap (spec: <= 5 s)

  void begin(unsigned long now) {
    seed_ = 0x9E3779B9UL ^ now;
    nextIdleAt_ = now + 4000UL;
    action_ = ACT_NONE;
    idle_ = IDLE_NONE;
  }

  // ---- customisation ----
  void setSpecies(uint8_t s) { if (s < PET_COUNT) species_ = s; }
  void setPalette(uint8_t p) { if (p < PALETTES_PER_PET) palette_ = p; }
  uint8_t species() const { return species_; }
  uint8_t palette() const { return palette_; }

  static const char *speciesName(uint8_t s) { return PET_SPECIES[s % PET_COUNT].name; }
  static const char *paletteName(uint8_t s, uint8_t p) {
    return PET_SPECIES[s % PET_COUNT].palettes[p % PALETTES_PER_PET].name;
  }
  static const PetPalette &paletteOf(uint8_t s, uint8_t p) {
    return PET_SPECIES[s % PET_COUNT].palettes[p % PALETTES_PER_PET];
  }

  // ---- context from the rest of the app ----
  void setAway(bool away) { away_ = away; }
  void setOnBreak(bool onBreak) { onBreak_ = onBreak; }

  // ---- interactive animations (home screen, short press) ----
  void trigger(NavKey key, unsigned long now) {
    switch (key) {
      case KEY_UP:     startAction(ACT_JUMP, 1400, now); break;
      case KEY_DOWN:   startAction(ACT_NAP, 4500, now); break;
      case KEY_BACK:   startAction(ACT_WIGGLE, 2000, now); break;
      case KEY_SELECT: startAction(ACT_LOVE, 3000, now); break;
      default: break;
    }
  }

  void update(unsigned long now) {
    if (action_ != ACT_NONE && (unsigned long)(now - actionAt_) >= actionMs_) {
      action_ = ACT_NONE;
      nextIdleAt_ = now + 3000UL;
    }
    if (idle_ != IDLE_NONE && (unsigned long)(now - idleAt_) >= idleMs_) {
      idle_ = IDLE_NONE;
    }
    if (action_ == ACT_NONE && idle_ == IDLE_NONE && !away_ &&
        (long)(now - nextIdleAt_) >= 0) {
      const uint8_t pick = random8() % 3;
      idle_ = (pick == 0) ? IDLE_LOOK : (pick == 1) ? IDLE_EAR : IDLE_YAWN;
      idleMs_ = (idle_ == IDLE_LOOK) ? 1800 : (idle_ == IDLE_EAR) ? 700 : 1600;
      idleAt_ = now;
      nextIdleAt_ = now + 5000UL + (random8() % 60) * 100UL;  // 5 - 11 s
    }
  }

  bool actionActive() const { return action_ != ACT_NONE; }

  const char *actionLabel() const {
    switch (action_) {
      case ACT_JUMP: return "Boing!";
      case ACT_NAP: return "Power nap...";
      case ACT_WIGGLE: return "Wiggle wiggle!";
      case ACT_LOVE: return "Love you too!";
      default: return "";
    }
  }

  PetPose pose(unsigned long now) const {
    PetPose p;
    memset(&p, 0, sizeof(p));
    p.species = species_;
    p.palette = palette_;
    p.dy = bobOffset(now);
    p.eyes = blinking(now) ? EYES_CLOSED : EYES_OPEN;
    p.mouth = onBreak_ ? MOUTH_SMILE : MOUTH_NEUTRAL;

    if (action_ != ACT_NONE) {
      applyAction(p, (unsigned long)(now - actionAt_));
      return p;
    }
    if (away_) {  // user left the desk: pet dozes until they come back
      static const int8_t slow[8] = {0, 0, 1, 1, 2, 2, 1, 1};
      p.dy = slow[(now / 300UL) % 8UL];
      p.eyes = EYES_CLOSED;
      p.mouth = MOUTH_FLAT;
      p.extra = EXTRA_ZZZ;
      p.extraPhase = (now / 250UL) % 8UL;
      return p;
    }
    if (idle_ != IDLE_NONE) applyIdle(p, (unsigned long)(now - idleAt_));
    return p;
  }

private:
  uint8_t species_ = PET_CAT;
  uint8_t palette_ = 0;
  bool away_ = false;
  bool onBreak_ = false;
  PetAction action_ = ACT_NONE;
  unsigned long actionAt_ = 0;
  unsigned long actionMs_ = 0;
  PetIdle idle_ = IDLE_NONE;
  unsigned long idleAt_ = 0;
  unsigned long idleMs_ = 0;
  unsigned long nextIdleAt_ = 0;
  uint32_t seed_ = 1;

  uint8_t random8() {  // xorshift32, no heap / no random() state shared
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return (uint8_t)(seed_ >> 8);
  }

  void startAction(PetAction a, unsigned long ms, unsigned long now) {
    action_ = a;
    actionAt_ = now;
    actionMs_ = ms > MAX_ACTION_MS ? MAX_ACTION_MS : ms;
    idle_ = IDLE_NONE;
  }

  // Gradual 0 -> 1 -> 2 -> 3 -> 2 -> 1 pixel bob, one step every 135 ms.
  static int8_t bobOffset(unsigned long now) {
    static const int8_t steps[6] = {0, 1, 2, 3, 2, 1};
    return steps[(now / 135UL) % 6UL];
  }

  // Short 120 ms blink roughly every 3.7 s.
  static bool blinking(unsigned long now) { return (now % 3700UL) < 120UL; }

  void applyIdle(PetPose &p, unsigned long t) const {
    switch (idle_) {
      case IDLE_LOOK:
        p.lookX = (t < 600) ? -1 : (t < 900) ? 0 : (t < 1500) ? 1 : 0;
        break;
      case IDLE_EAR:
        p.earTwitch = ((t / 140UL) % 2UL) == 0;
        break;
      case IDLE_YAWN:
        if (t >= 300 && t < 1300) {
          p.mouth = MOUTH_OPEN;
          p.eyes = EYES_CLOSED;
        }
        break;
      default: break;
    }
  }

  void applyAction(PetPose &p, unsigned long t) const {
    switch (action_) {
      case ACT_JUMP: {  // two parabolic hops, 7 px high
        const unsigned long hop = 700UL;
        const unsigned long ht = t % hop;
        const long h = (4L * 7L * (long)ht * (long)(hop - ht)) / (long)(hop * hop);
        p.dy = (int8_t)(-h);
        p.eyes = (h > 2) ? EYES_HAPPY : EYES_OPEN;
        p.mouth = MOUTH_OPEN;
        if (h > 4) {
          p.extra = EXTRA_SPARKLE;
          p.extraPhase = (t / 120UL) % 8UL;
        }
        break;
      }
      case ACT_NAP: {
        static const int8_t breathe[8] = {0, 0, 1, 1, 2, 2, 1, 1};
        p.dy = breathe[(t / 250UL) % 8UL];
        p.eyes = EYES_CLOSED;
        p.mouth = MOUTH_FLAT;
        p.extra = EXTRA_ZZZ;
        p.extraPhase = (t / 250UL) % 8UL;
        break;
      }
      case ACT_WIGGLE: {
        static const int8_t wig[8] = {-2, -1, 0, 1, 2, 1, 0, -1};
        p.dx = wig[(t / 60UL) % 8UL];
        p.eyes = EYES_HAPPY;
        p.mouth = MOUTH_SMILE;
        p.earTwitch = ((t / 120UL) % 2UL) == 0;
        break;
      }
      case ACT_LOVE:
        p.eyes = EYES_HAPPY;
        p.mouth = MOUTH_SMILE;
        p.blush = true;
        p.extra = EXTRA_HEART;
        p.extraPhase = (t / 180UL) % 8UL;
        break;
      default: break;
    }
  }
};

#endif
