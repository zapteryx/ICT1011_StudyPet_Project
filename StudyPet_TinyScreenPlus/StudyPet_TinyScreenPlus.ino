/*
  StudyPet - TinyScreen+ study companion
  Hardware: TinyScreen+ (ATSAMD21), ASD2022 Wireling Adapter, AST1035 buzzer
  on port 0 (A0), AST1024 TOF distance sensor on port 1 (I2C). Definitions in Input.h
  (override them here, ABOVE the #includes, if your wiring differs):

    // #define BUZZER_PIN      A0
    // #define IR_SENSOR_PIN   A1
    // #define IR_ACTIVE_LEVEL HIGH
    // #define TOF_SENSOR_PORT 1
    // #define TOF_PRESENCE_MAX_MM 1000
    // #define PRESENCE_USE_TOF 0 // only if switching back to a legacy sensor

  Button mapping (screen upright):
    Top Left  = BACK        Top Right    = UP
    Bottom Left = SELECT    Bottom Right = DOWN

  Home screen:  tap any button      -> pet animation (<= 5 s)
                hold Bottom Left    -> open menu
                hold Top Left       -> start / pause break timer
                hold Top Right      -> start / stop study session
                hold Bottom Right   -> hours dashboard

  Loop order = state machine:
    1. Input  -> UI decides: pet interaction (home) or menu navigation
    2. Features update (clock, IR presence, timer, study, deadlines, buzzer)
    3. Pet context (away / on break) + animation update
    4. UI draws: deadline pop-ups pre-empt any screen; everything else is
       either HOME (pet) or the full-screen menu stack.
*/
#include <SPI.h>
#include <Wire.h>
#include <TinyScreen.h>
#include "Input.h"
#include "Pet.h"
#include "Features.h"
#include "UI.h"

TinyScreen display = TinyScreen(TinyScreenPlus);
ButtonInput buttons;
Pet pet;
FeatureManager features;
StudyPetUI ui;

void setup() {
  display.begin();
  display.setBrightness(10);  // 0 - 15 (UI re-applies the saved setting)
  display.setBitDepth(TSBitDepth8);
  display.setFont(thinPixel7_10ptFontInfo);

  const unsigned long now = millis();
  buttons.begin(now);
  features.begin(now);
  pet.begin(now);
  ui.begin(display, now);
}

void loop() {
  const unsigned long now = millis();

  // 1. Input. Poll every loop so short taps are never missed.
  const InputEvent ev = buttons.update(display, now);
  if (ev.key != KEY_NONE) ui.handleInput(ev, pet, features, now);

  // 2. Background features keep running whatever screen is shown.
  features.update(now);

  // 3. Pet reacts to the study state.
  pet.setAway(features.settings.irEnabled && !features.presence.present());
  pet.setOnBreak(features.timer.onBreak());
  pet.update(now);

  // 4. Draw.
  ui.update(display, pet, features, now);
}
