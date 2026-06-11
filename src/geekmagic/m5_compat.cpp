#include "m5_compat.h"
#include <esp_sleep.h>

// ---- Board pin map (overridable via build_flags) --------------------------
#ifndef BUDDY_BL_PIN
#define BUDDY_BL_PIN 5            // backlight, PWM, active-low (inverted)
#endif
#ifndef BUDDY_TOUCH_PIN
#define BUDDY_TOUCH_PIN 32        // capacitive pad on top of the case
#endif
#ifndef BUDDY_TOUCH_THRESHOLD
#define BUDDY_TOUCH_THRESHOLD 40  // touchRead() below this = finger present
#endif

M5Class M5;

// ===========================================================================
//  Backlight + power
// ===========================================================================
// SmallTV Pro backlight is active-low: a high PWM duty = dim, low = bright.
static inline void blApply(int level /*0..100*/, bool on) {
  if (!on) { analogWrite(BUDDY_BL_PIN, 255); return; }   // inverted off
  int duty = constrain(level, 0, 100) * 255 / 100;
  analogWrite(BUDDY_BL_PIN, 255 - duty);                 // inverted
}

void GMAxp::begin() {
  pinMode(BUDDY_BL_PIN, OUTPUT);
  blApply(_level, _on);
}
void GMAxp::ScreenBreath(int level) { _level = constrain(level, 0, 100); blApply(_level, _on); }
void GMAxp::SetLDO2(bool on)        { _on = on; blApply(_level, _on); }
void GMAxp::PowerOff() {
  // No PMIC to cut power; blank the screen and deep-sleep. The device wakes
  // on the next reset / power cycle (touch-wake would need a confirmed pin).
  blApply(0, false);
  esp_deep_sleep_start();
}

// ===========================================================================
//  Software RTC  (bridge pushes wall-clock time over BLE)
// ===========================================================================
// Howard Hinnant's calendar algorithms: days since 1970-01-01 <-> Y/M/D.
static int64_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}
static void civil_from_days(int64_t z, int& y, unsigned& m, unsigned& d) {
  z += 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int yy = (int)yoe + (int)(era * 400);
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y = yy + (m <= 2);
}

void GMRtc::_recompute() {
  _baseEpoch  = days_from_civil(_y, _mo, _d) * 86400LL
              + (int64_t)_h * 3600 + (int64_t)_mi * 60 + _s;
  _baseMillis = millis();
  _set = true;
}
void GMRtc::SetTime(const RTC_TimeTypeDef* t) {
  _h = t->Hours; _mi = t->Minutes; _s = t->Seconds; _recompute();
}
void GMRtc::SetDate(const RTC_DateTypeDef* d) {
  _y = d->Year; _mo = d->Month; _d = d->Date; _recompute();
}
void GMRtc::GetTime(RTC_TimeTypeDef* t) {
  if (!_set) { t->Hours = t->Minutes = t->Seconds = 0; return; }
  int64_t now = _baseEpoch + (int64_t)((millis() - _baseMillis) / 1000);
  int64_t secs = now % 86400; if (secs < 0) secs += 86400;
  t->Hours = (uint8_t)(secs / 3600);
  t->Minutes = (uint8_t)((secs % 3600) / 60);
  t->Seconds = (uint8_t)(secs % 60);
}
void GMRtc::GetDate(RTC_DateTypeDef* d) {
  if (!_set) { d->WeekDay = 0; d->Month = 1; d->Date = 1; d->Year = 1970; return; }
  int64_t now = _baseEpoch + (int64_t)((millis() - _baseMillis) / 1000);
  int64_t days = now / 86400; if (now % 86400 < 0) days--;
  int y; unsigned m, dd; civil_from_days(days, y, m, dd);
  d->Year = (uint16_t)y; d->Month = (uint8_t)m; d->Date = (uint8_t)dd;
  int64_t wd = (days % 7 + 4) % 7; if (wd < 0) wd += 7;   // 0 = Sunday
  d->WeekDay = (uint8_t)wd;
}

// ===========================================================================
//  Touch -> virtual buttons
// ===========================================================================
static const uint32_t DOUBLE_TAP_MS = 280;
static const uint32_t LONG_MS       = 600;   // matches firmware pressedFor(600)

static bool     s_prevDown   = false;
static uint32_t s_pressStart = 0;
static bool     s_holdFired  = false;
static bool     s_pendingTap = false;
static uint32_t s_firstTapMs = 0;
static uint8_t  s_dbCount    = 0;            // debounce: consecutive agreeing reads
static bool     s_dbState    = false;

static bool readTouchRaw() {
#if defined(BUDDY_TOUCH_DIGITAL)
  // Active-low digital button between the pin and GND.
  return digitalRead(BUDDY_TOUCH_PIN) == LOW;
#else
  return touchRead(BUDDY_TOUCH_PIN) < BUDDY_TOUCH_THRESHOLD;
#endif
}

// 3-sample debounce so a noisy capacitive read doesn't chatter.
static bool readTouch() {
  bool raw = readTouchRaw();
  if (raw == s_dbState) { s_dbCount = 0; return s_dbState; }
  if (++s_dbCount >= 3) { s_dbState = raw; s_dbCount = 0; }
  return s_dbState;
}

static void driveButton(GMButton& b, uint32_t now) {
  if (b._pulse == 2) {            // momentary press, frame 1
    b._pressed = true; b._wasPressed = true; b._pressStart = now; b._pulse = 1;
  } else if (b._pulse == 1) {     // momentary release, frame 2
    b._pressed = false; b._wasReleased = true; b._pulse = 0;
  }
}

void M5Class::update() {
  uint32_t now = millis();
  // Edge flags are valid for one update only.
  BtnA._wasPressed = BtnA._wasReleased = false;
  BtnB._wasPressed = BtnB._wasReleased = false;

  bool down = readTouch();

  if (down && !s_prevDown) {            // finger down
    s_pressStart = now; s_holdFired = false;
  }

  // Held past the long-press threshold -> BtnA held (drives menu).
  if (down && !s_holdFired && (now - s_pressStart >= LONG_MS)) {
    s_holdFired = true; s_pendingTap = false;
    BtnA._pressed = true; BtnA._wasPressed = true; BtnA._pressStart = s_pressStart;
  }
  if (down && s_holdFired) BtnA._pressed = true;   // stay held

  if (!down && s_prevDown) {            // finger up
    uint32_t dur = now - s_pressStart;
    if (s_holdFired) {
      BtnA._pressed = false; BtnA._wasReleased = true;
    } else if (dur < LONG_MS) {
      if (s_pendingTap && (now - s_firstTapMs) <= DOUBLE_TAP_MS) {
        s_pendingTap = false; BtnB._pulse = 2;       // double tap -> BtnB
      } else {
        s_pendingTap = true; s_firstTapMs = now;     // wait for a possible 2nd
      }
    }
  }

  // A single tap resolves once the double-tap window lapses with no 2nd tap.
  if (s_pendingTap && !down && (now - s_firstTapMs) > DOUBLE_TAP_MS) {
    s_pendingTap = false; BtnA._pulse = 2;           // single tap -> BtnA
  }

  driveButton(BtnA, now);
  driveButton(BtnB, now);
  s_prevDown = down;
}

// ===========================================================================
//  Bring-up
// ===========================================================================
void M5Class::begin() {
  Serial.begin(115200);
  Lcd.init();
  Lcd.setRotation(0);
  Lcd.fillScreen(TFT_BLACK);
  Axp.begin();
#if !defined(BUDDY_TOUCH_DIGITAL)
  // touchRead needs no pinMode; warm up the debounce state.
  s_dbState = readTouchRaw();
#else
  pinMode(BUDDY_TOUCH_PIN, INPUT_PULLUP);
#endif
}
