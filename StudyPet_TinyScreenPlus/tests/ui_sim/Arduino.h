#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PROGMEM
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define A0 14
#define A1 15
static unsigned long simulatedMillis = 0;
static int sensorLevel = LOW;
static unsigned toneCalls = 0;
static uint8_t lastTonePin = 0;
static unsigned lastToneHz = 0;
inline unsigned long millis() { return simulatedMillis; }
inline void delay(unsigned long ms) { simulatedMillis += ms; }
inline void pinMode(uint8_t, int) {}
inline void digitalWrite(uint8_t, int) {}
inline int digitalRead(uint8_t) { return sensorLevel; }
inline void tone(uint8_t pin, unsigned int hz) { ++toneCalls; lastTonePin = pin; lastToneHz = hz; }
inline void noTone(uint8_t) {}
