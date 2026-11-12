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
inline unsigned long millis() { return simulatedMillis; }
inline void pinMode(uint8_t, int) {}
inline void digitalWrite(uint8_t, int) {}
inline int digitalRead(uint8_t) { return sensorLevel; }
inline void tone(uint8_t, unsigned int) {}
inline void noTone(uint8_t) {}
