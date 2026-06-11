#pragma once
// M5StickC Plus compatibility shim for the GeekMagic SmallTV Pro.
//
// The SmallTV Pro is an ESP32-WROOM-32 with a 240x240 ST7789 panel, a
// single capacitive touch pad, a PWM backlight, and nothing else. The
// original firmware targets an M5StickC Plus, which additionally has an
// IMU, an AXP192 PMIC, an RTC, a buzzer and two buttons. This header
// reproduces the slice of the M5 API the firmware actually calls and
// backs it with the SmallTV Pro's real hardware where it exists and with
// inert stubs where it doesn't.
//
// Pin map and panel config live in platformio.ini build_flags.

#include <Arduino.h>
#include <TFT_eSPI.h>   // provides TFT_eSPI and TFT_eSprite
#include <FS.h>         // M5StickCPlus.h used to pull these in transitively;
#include <Preferences.h>// the firmware relies on the bare `File` type and on
using fs::File;         // <Preferences.h> being available without an include.

// The firmware draws a 135-wide portrait sprite (M5StickC Plus panel) and
// pushes it at x=0. The SmallTV Pro panel is 240 wide, so center it.
#ifndef PUSH_X
#define PUSH_X ((240 - 135) / 2)   // 52px letterbox each side
#endif
#ifndef PUSH_Y
#define PUSH_Y 0
#endif

// M5StickC Plus exposes a handful of bare color names that TFT_eSPI spells
// TFT_*. The firmware uses GREEN and RED.
#ifndef GREEN
#define GREEN TFT_GREEN
#endif
#ifndef RED
#define RED TFT_RED
#endif

// ---- RTC ------------------------------------------------------------------
// No RTC chip on the SmallTV Pro. The desktop bridge pushes wall-clock time
// over BLE; we hold it in a software clock that advances off millis().
// Field order matches the aggregate initializers in data.h.
struct RTC_TimeTypeDef { uint8_t Hours; uint8_t Minutes; uint8_t Seconds; };
struct RTC_DateTypeDef { uint8_t WeekDay; uint8_t Month; uint8_t Date; uint16_t Year; };

class GMRtc {
public:
  void SetTime(const RTC_TimeTypeDef* t);
  void SetDate(const RTC_DateTypeDef* d);
  void GetTime(RTC_TimeTypeDef* t);
  void GetDate(RTC_DateTypeDef* d);
private:
  // Last components handed to us, plus the millis() at which they applied.
  uint8_t  _h = 0, _mi = 0, _s = 0, _mo = 1, _d = 1;
  uint16_t _y = 1970;
  int64_t  _baseEpoch = 0;     // seconds, computed from the components above
  uint32_t _baseMillis = 0;
  bool     _set = false;
  void _recompute();
};

// ---- IMU ------------------------------------------------------------------
// No accelerometer. Report a flat, motionless device so the face-down nap,
// shake-to-dizzy and clock-orientation logic all stay quiescent.
class GMImu {
public:
  void Init() {}
  int getAccelData(float* ax, float* ay, float* az) {
    *ax = 0.0f; *ay = 0.0f; *az = 1.0f; return 0;
  }
};

// ---- Power / backlight ----------------------------------------------------
// No AXP192. ScreenBreath drives the inverted PWM backlight; SetLDO2 is the
// screen on/off the firmware uses for idle blanking; the battery readers
// return USB-powered constants; PowerOff deep-sleeps the chip.
class GMAxp {
public:
  void  begin();
  void  ScreenBreath(int level);    // 0..100 brightness
  void  SetLDO2(bool on);           // screen power -> backlight enable
  void  PowerOff();
  float GetVBusVoltage()  { return 5.0f; }   // always on USB
  float GetBatVoltage()   { return 4.2f; }
  float GetBatCurrent()   { return 0.0f; }
  int   GetTempInAXP192() { return 35; }
  uint8_t GetBtnPress()   { return 0; }       // no hardware power button
private:
  int  _level = 100;
  bool _on = true;
};

// ---- Buzzer ---------------------------------------------------------------
// No buzzer on the SmallTV Pro. Swallow tones.
class GMBeep {
public:
  void begin() {}
  void tone(uint16_t /*freq*/, uint16_t /*dur*/) {}
  void update() {}
};

// ---- Buttons --------------------------------------------------------------
// The SmallTV Pro has one capacitive pad. M5Update() decodes gestures from
// it and drives two virtual buttons so the firmware's BtnA/BtnB logic works
// unchanged (see m5_compat.cpp for the gesture scheme):
//   single tap   -> BtnA momentary  (primary: approve / cycle / "next")
//   double tap   -> BtnB momentary  (secondary: deny / page / "change")
//   long press   -> BtnA held       (pressedFor(600) -> menu)
class GMButton {
public:
  bool isPressed()              { return _pressed; }
  bool wasPressed()             { return _wasPressed; }
  bool wasReleased()            { return _wasReleased; }
  bool pressedFor(uint32_t ms)  { return _pressed && (millis() - _pressStart >= ms); }

  // Driven by M5Update(); not part of the public M5 API.
  bool     _pressed = false, _wasPressed = false, _wasReleased = false;
  uint32_t _pressStart = 0;
  uint8_t  _pulse = 0;          // momentary-press countdown (updates)
};

// ---- Facade ---------------------------------------------------------------
class M5Class {
public:
  TFT_eSPI Lcd;
  GMImu    Imu;
  GMAxp    Axp;
  GMBeep   Beep;
  GMRtc    Rtc;
  GMButton BtnA;
  GMButton BtnB;

  void begin();
  void update();
};

extern M5Class M5;
