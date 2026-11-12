#ifndef STUDYPET_CONFIG_H
#define STUDYPET_CONFIG_H
// Edit hardware options HERE so the sketch and BLE .cpp files agree.
// Do not put per-file #defines in the .ino: that only changes one translation unit.
#ifndef STUDYPET_ENABLE_BLE
#define STUDYPET_ENABLE_BLE 1
#endif
#ifndef BUZZER_PIN
#define BUZZER_PIN A0
#endif
#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE 1
#endif
#ifndef USE_WIRELING_ADAPTER
#define USE_WIRELING_ADAPTER 1
#endif
#ifndef PRESENCE_USE_TOF
#define PRESENCE_USE_TOF 1
#endif
#ifndef TOF_SENSOR_PORT
#define TOF_SENSOR_PORT 1
#endif
#ifndef TOF_PRESENCE_MAX_MM
#define TOF_PRESENCE_MAX_MM 1000
#endif
#ifndef DEADLINE_EEPROM_ADDRESS
#define DEADLINE_EEPROM_ADDRESS 0x50
#endif
#ifndef DEADLINE_EEPROM_OFFSET
#define DEADLINE_EEPROM_OFFSET 0
#endif
#ifndef IR_SENSOR_PIN
#define IR_SENSOR_PIN A1
#endif
#ifndef IR_ACTIVE_LEVEL
#define IR_ACTIVE_LEVEL HIGH
#endif
#ifndef IR_USE_PULLUP
#define IR_USE_PULLUP 0
#endif
#ifndef IR_USE_THERMAL_CAMERA
#define IR_USE_THERMAL_CAMERA 0
#endif
#ifndef THERMAL_CAMERA_PORT
#define THERMAL_CAMERA_PORT 1
#endif
#ifndef THERMAL_MIN_QUARTER_C
#define THERMAL_MIN_QUARTER_C 112
#endif
#ifndef THERMAL_MAX_QUARTER_C
#define THERMAL_MAX_QUARTER_C 160
#endif
#ifndef THERMAL_CONTRAST_QUARTER_C
#define THERMAL_CONTRAST_QUARTER_C 10
#endif
#ifndef THERMAL_MIN_PIXELS
#define THERMAL_MIN_PIXELS 3
#endif
#endif
