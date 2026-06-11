#pragma once
// Hardware abstraction layer.
//
// The firmware was written against the M5StickC Plus API (M5.Lcd, M5.Imu,
// M5.Axp, M5.Beep, M5.Rtc, M5.BtnA/B). To run on other ESP32 boards we keep
// that API surface and swap the implementation at build time.
//
//   default build              -> real M5StickC Plus library
//   -DTARGET_GEEKMAGIC build    -> GeekMagic SmallTV Pro compat shim
//
// Every source file that used `#include <M5StickCPlus.h>` now includes this
// header instead, so a single codebase targets both boards.
#if defined(TARGET_GEEKMAGIC)
  #include "geekmagic/m5_compat.h"
#else
  #include <M5StickCPlus.h>
#endif
